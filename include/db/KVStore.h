#pragma once

#include "SSTable.h"
#include "MemTable.h"
#include "SerializeWrapper.h"
#include <fstream>
#include <iostream>
#include <format>


template<typename KType, typename VType>
class KVStore {
private:
    uint64_t curr_timestamp;
    uint64_t max_memtable_size;
    string db_path;
    MemTable<KType, VType> mem_table;
    vector<vector<SSTable<KType, VType>>> sstables;

public:
    KVStore(const string& db_path_): curr_timestamp(0), max_memtable_size(8), db_path(db_path_), mem_table(), sstables(1) {}

    void put(const KType key, const VType value) {
        if(mem_table.get(key) != nullptr)
        {
            mem_table.put(key, value);
        }
        else
        {
            if(mem_table.get_size() + 1 <= max_memtable_size)
            {
                mem_table.put(key, value);
            }
            else
            {
                oversize();
                mem_table.put(key, value);
            }
        }
        //std::cout << std::format("key: {}, value: {}, size: {}\n", key, value, mem_table.get_size());
    }

    void oversize() {
        SSTable<KType, VType> sstable;
        sstable.level = 0;
        sstable.order = sstables[0].size();
        sstable.header.timestamp = curr_timestamp;
        sstable.header.kv_count = mem_table.get_size();
        size_t offset = 0;
        if(mem_table.get_min_max_key(sstable.header.min_key, sstable.header.max_key))
        {
            printf("Error: get_min_max_key failed, because there is no node in memtable.\n");
        }
        sstable.bloom_filter = BloomFilter<KType>();

        offset += sstable.header.getHeaderSpace() + 
            mem_table.get_size() * (sizeof(SSTableIndex::key) + sizeof(SSTableIndex::offset)) + 
            sstable.bloom_filter.bit_array.size() / 8 * sizeof(bool);  

        for(auto kv_wrapper: mem_table.get_all_kv())
        {
            sstable.bloom_filter.put(*(kv_wrapper.key_ptr));
            sstable.index.push_back(SSTableIndex(*(kv_wrapper.key_ptr), offset));
            offset += SerializeWrapper<VType>::serialize_size(*(kv_wrapper.value_ptr));
            // std::cout << std::format("key: {}, value {},offset: {}\n", *(kv_wrapper.key_ptr), *(kv_wrapper.value_ptr) , offset);
        }       
        sstables[0].push_back(sstable);

        string filename = std::format("{}/{}-{}.sst", db_path, sstable.level, sstable.order);
        std::ofstream sstable_file(filename, std::ios::binary | std::ios::out);
        sstable_file.rdbuf() -> pubsetbuf(0, 0);
        sstable.writeToFile(sstable_file);

        for(auto kv_wrapper: mem_table.get_all_kv()){
            sstable_file.write(SerializeWrapper<VType>::serialize(*(kv_wrapper.value_ptr)).c_str(), 
            SerializeWrapper<VType>::serialize_size(*(kv_wrapper.value_ptr)));
        }
        
        sstable_file.close();
        curr_timestamp++;
        mem_table = MemTable<KType, VType>();
    }

    unique_ptr<VType> get(const KType key) {
        unique_ptr<VType> val_ptr = mem_table.get(key);
        if(val_ptr){
            if(*val_ptr == DeleteMarker<VType>::value())
                return nullptr;
            else
                return val_ptr;
        }
        uint64_t max_timestamp = 0;
        string value_str;
        bool find_in_sstable = false;
        for(auto& sstable_level:sstables)
            for(auto &sstable:sstable_level) {
                if(sstable.header.timestamp < max_timestamp)
                    continue;
                if(sstable.bloom_filter.contains(key)) {
                    size_t l = 0, r = sstable.index.size() - 1;
                    while(l <= r) {
                        size_t m = l + (r - l) / 2;
                        if(sstable.index[m].key == key) {
                            std::ifstream sstable_file(std::format("{}/{}-{}.sst", db_path, sstable.level, sstable.order), std::ios::binary | std::ios::in);
                            sstable_file.seekg(sstable.index[m].offset);
                            value_str.clear();
                            if(m == sstable.index.size() - 1) {
                                char c;
                                while(sstable_file.get(c))
                                    value_str += c;
                            }
                            else {
                                uint64_t read_size = sstable.index[m + 1].offset - sstable.index[m].offset;
                                unique_ptr<char[]> value_buffer(new char[read_size]);
                                sstable_file.read(value_buffer.get(), read_size);
                                value_str = string(value_buffer.get(), read_size);
                            }
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

    void del(const KType key) {
        put(key, DeleteMarker<VType>::value());
    }
};