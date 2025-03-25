#pragma once

#include "SSTable.h"
#include "MemTable.h"
#include "SerializeWrapper.h"
#include <fstream>
#include <iostream>
#include <format>
#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>
#include <filesystem>
#include <regex>
#include <map>
#include <shared_mutex>

template<typename KType, typename VType>
class KVStore {
private:
    uint64_t curr_timestamp;
    uint64_t max_memtable_size;
    string db_path;

    shared_ptr<MemTable<KType, VType>> mem_table;
    shared_ptr<MemTable<KType, VType>> immutable_mem_table;
    vector<vector<SSTable<KType, VType>>> sstables;

    std::condition_variable compaction_cv;

    bool isCompaction{false};
    std::mutex compaction_mutex;

    mutable std::shared_mutex rw_mutex;
private:
    void compaction() {
        std::unique_lock guard(compaction_mutex);
       // std::cout <<"compaction, get compaction_mutex" << std::endl;
        compaction_cv.wait(guard, [this]() { return !isCompaction; });

        isCompaction = true;
        immutable_mem_table = std::move(mem_table);
        mem_table = make_shared<MemTable<KType, VType>>();

        std::thread bg(std::bind(&KVStore<KType, VType>::doCompaction, this));
        bg.detach();

        //std::cout <<"compaction, release compaction_mutex" << std::endl;
    }

    void doCompaction() {
        std::unique_lock<std::mutex> guard(compaction_mutex);
        minorCompaction();
        if (sstables[0].size() >= 8) {
            max_memtable_size  *= 2;
            majorCompaction();
        }
        isCompaction = false;
        guard.unlock();
        compaction_cv.notify_all();
    }

public:
    KVStore(const string& db_path_): curr_timestamp(0), max_memtable_size(64), db_path(db_path_), mem_table(make_shared<MemTable<KType, VType>>()), immutable_mem_table(nullptr), sstables(1) {
        if (!std::filesystem::exists(db_path)) {
            std::filesystem::create_directory(db_path);
        }
    }

    void put(const KType key, const VType value) {
        //std::cout<< "put, acquire for rw_lock" << std::endl;
        std::unique_lock rw_lock(rw_mutex);
        //std::cout<< "put, get rw_lock" << std::endl;
        if(mem_table -> get(key) != nullptr)
        {
            mem_table -> put(key, value);
        }
        else
        {
            if(mem_table -> get_size() + 1 <= max_memtable_size)
            {
                mem_table -> put(key, value);
            }
            else
            {
                if (mem_table -> get_size() + 1 >= max_memtable_size) {
                    rw_lock.unlock();
                    compaction();
                    rw_lock.lock();
                }
                mem_table -> put(key, value);
            }
        }
        //std::cout<< "put, release rw_lock" << std::endl;
        //std::cout << std::format("key: {}, value: {}, size: {}\n", key, value, mem_table.get_size());
    }


    unique_ptr<VType> get(const KType key) {
        std::shared_lock lock(rw_mutex);
        unique_ptr<VType> val_ptr = mem_table -> get(key);
        if(val_ptr){
            if(*val_ptr == DeleteMarker<VType>::value())
                return nullptr;
            return val_ptr;
        }

        if (immutable_mem_table != nullptr) {
            val_ptr = immutable_mem_table -> get(key);
            if(val_ptr){
                if(*val_ptr == DeleteMarker<VType>::value())
                    return nullptr;
                return val_ptr;
            }
        }

        uint64_t max_timestamp = 0;
        string value_str;
        bool find_in_sstable = false;
        for(auto& sstable_level:sstables)
            for(auto &sstable:sstable_level) {
                if(sstable.header.timestamp < max_timestamp)
                    continue;
                if(sstable.bloom_filter.contains(key)) {
                    long l = 0, r = sstable.index.size() - 1;
                    while(l <= r) {
                        long m = l + (r - l) / 2;
                        if(sstable.index[m].key == key) {
                            std::ifstream sstable_file(std::format("{}/{}-{}.sst", db_path, sstable.level, sstable.order), std::ios::binary | std::ios::in);
                            value_str = readFromSSTable(sstable, m, sstable_file);
                            sstable_file.close();
                            max_timestamp = sstable.header.timestamp;
                            find_in_sstable = true;
                            break;
                        }
                        if(sstable.index[m].key < key)
                            l = m + 1;
                        else
                            r = m - 1;
                    }
                }
            }
        if(find_in_sstable) {
            if(DeleteMarker<VType>::isDeleted(SerializeWrapper<VType>::deserialize(value_str)))
                return nullptr;
            else
                return make_unique<VType>(SerializeWrapper<VType>::deserialize(value_str));
        }
        return nullptr;
    }

    SSTable<KType, VType> writeImmutableToDisk() {
        SSTable<KType, VType> sstable;
        sstable.level = 0;
        sstable.order = sstables[0].size();
        sstable.header.timestamp = curr_timestamp;
        sstable.header.kv_count = immutable_mem_table->get_size();
        size_t offset = 0;

        if(immutable_mem_table->get_min_max_key(sstable.header.min_key, sstable.header.max_key))
        {
            printf("Error: get_min_max_key failed, because there is no node in memtable.\n");
        }

        sstable.bloom_filter = BloomFilter<KType>();

        offset += sstable.header.getHeaderSpace() +
            immutable_mem_table->get_size() * (sizeof(SSTableIndex::key) + sizeof(SSTableIndex::offset)) +
            sstable.bloom_filter.bit_array.size() / 8 * sizeof(bool);

        for(auto kv_wrapper: immutable_mem_table->get_all_kv())
        {
            sstable.bloom_filter.put(*(kv_wrapper.key_ptr));
            sstable.index.push_back(SSTableIndex(*(kv_wrapper.key_ptr), offset));
            // std::cout << std::format("key: {}, value {},offset: {}\n", *(kv_wrapper.key_ptr), *(kv_wrapper.value_ptr) , offset);
            offset += SerializeWrapper<VType>::serialize_size(*(kv_wrapper.value_ptr));
        }

        string filename = std::format("{}/{}-{}.sst", db_path, sstable.level, sstable.order);
        std::ofstream sstable_file(filename, std::ios::binary | std::ios::out);
        // write metadata to file
        sstable.writeToFile(sstable_file);

        // write data to file
        for(auto kv_wrapper: immutable_mem_table->get_all_kv()){
            sstable_file.write(SerializeWrapper<VType>::serialize(*(kv_wrapper.value_ptr)).c_str(),
            SerializeWrapper<VType>::serialize_size(*(kv_wrapper.value_ptr)));
        }
        return sstable;
    }

    void minorCompaction()
    {
        curr_timestamp++;

        SSTable<KType, VType> sstable = writeImmutableToDisk();
       // std::cout<< "minor Compaction, acquire for rw_lock" << std::endl;
        std::unique_lock rw_lock(rw_mutex);
        //std::cout<< "minor Compaction, get rw_lock" << std::endl;

        sstables[0].push_back(std::move(sstable));

        immutable_mem_table.reset();
    }


    void del(const KType key) {
        put(key, DeleteMarker<VType>::value());
    }

    void majorCompaction() {
        std::map<KType, VType> k2v;
        std::map<KType, uint64_t> k2timestamp;
        size_t sstable_num = sstables[0].size();
        vector<SSTable<KType, VType>> new_sstables;

        // read all kv-pairs to k2v and k2timestamp
        for (auto sstable:sstables[0]) {
            std::ifstream sstable_file(std::format("{}/{}-{}.sst", db_path, sstable.level, sstable.order), std::ios::binary | std::ios::in);
            auto timestamp = sstable.header.timestamp;
            for (size_t i = 0; i < sstable.index.size(); i++) {
                auto key = sstable.index[i].key;
                auto offset = sstable.index[i].offset;
                if (!k2timestamp.contains(key) || k2timestamp[key] < timestamp) {
                    k2timestamp[key] = timestamp;
                    string value_str = readFromSSTable(sstable, i, sstable_file);
                    VType value = SerializeWrapper<VType>::deserialize(value_str);
                    if (DeleteMarker<VType>::isDeleted(value)) {
                        k2v.erase(key);
                    } else {
                        k2v[key] = value;
                    }
                }
            }
        }

        size_t kv_num = k2v.size();
        size_t new_sstable_num = sstable_num / 2;
        size_t kv_num_per_sstable = kv_num / new_sstable_num;
        curr_timestamp++;
        auto iter = k2v.begin();

        // construct new sstables
        if (kv_num > 0) {
            for (size_t i = 0; i < new_sstable_num ; i++) {
                SSTable<KType, VType> sstable;
                vector<VType*> value_ptr_vec;
                sstable.level = 0;
                sstable.order = i;
                sstable.header.timestamp = curr_timestamp;
                sstable.header.kv_count = kv_num_per_sstable + (i == new_sstable_num - 1 ? kv_num % new_sstable_num : 0);
                sstable.bloom_filter = BloomFilter<KType>();
                size_t begin = i * kv_num_per_sstable;
                size_t end = begin + sstable.header.kv_count;
                sstable.header.min_key = iter->first;
                size_t offset = sstable.header.getHeaderSpace() +
                    sstable.header.kv_count * (sizeof(SSTableIndex::key) + sizeof(SSTableIndex::offset)) +
                    sstable.bloom_filter.bit_array.size() / 8 * sizeof(bool);
                for (int j = begin; j < end; j++) {
                    const KType &key = iter->first;
                    const VType &value = iter->second;
                    value_ptr_vec.push_back(&(iter->second));
                    sstable.bloom_filter.put(key);
                    sstable.index.emplace_back(key, offset);
                    offset += SerializeWrapper<VType>::serialize_size(value);
                    ++iter;
                }
                --iter;
                sstable.header.max_key = iter -> first;
                ++iter;

                string filename = std::format("{}/{}-{}.sst.temp", db_path, sstable.level, sstable.order);
                std::ofstream sstable_file(filename, std::ios::binary | std::ios::out);
                // write metadata to file
                sstable.writeToFile(sstable_file);

                for (int j = begin; j < end; j++) {
                    sstable_file.write(SerializeWrapper<VType>::serialize(*(value_ptr_vec[j- begin])).c_str(),
                    SerializeWrapper<VType>::serialize_size(*(value_ptr_vec[j- begin])));
                }
                sstable_file.close();
                new_sstables.push_back(sstable);
            }
        }

        std::unique_lock rw_lock(rw_mutex);

        for (const auto& entry : std::filesystem::directory_iterator(db_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".sst") {
                std::filesystem::remove(entry.path());
                //std::cout << "已删除文件: " << entry.path() << std::endl;
            }
        }

        for (const auto& entry : std::filesystem::directory_iterator(db_path)) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                std::string filename = path.filename().string();
                size_t pos = filename.rfind(".sst.temp");
                if (pos != std::string::npos) {
                    std::string new_name = filename.substr(0, pos) + ".sst";
                    std::filesystem::path new_path = path.parent_path() / new_name;
                    std::filesystem::rename(path, new_path);
                    //std::cout << "已重命名: " << path << " -> " << new_path << std::endl;
                }
            }
        }
        sstables[0] = std::move(new_sstables);
    }

    vector<string> getAllSSTables() {
        vector<string> all_sstables;
        std::regex pattern(R"(^\d+-\d+\.sst$)");
        for (const auto& entry : std::filesystem::directory_iterator(db_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, pattern)) {
                    all_sstables.push_back(entry.path().string());
                }
            }
        }
        return all_sstables;
    }

    string readFromSSTable(SSTable<KType, VType> &sstable,size_t index, std::ifstream &sstable_file) {
        sstable_file.seekg(sstable.index[index].offset);
        string value_str;
        if (index == sstable.index.size() - 1) {
            char c;
            while (sstable_file.get(c)) {
                value_str += c;
            }
        } else {
            uint64_t read_size = sstable.index[index + 1].offset - sstable.index[index].offset;
            unique_ptr<char[]> value_buffer(new char[read_size]);
            sstable_file.read(value_buffer.get(), read_size);
            value_str = string(value_buffer.get(), read_size);
        }
        return value_str;
    }
};
