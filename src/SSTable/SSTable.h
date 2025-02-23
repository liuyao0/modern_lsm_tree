#pragma once
#include <cstdint>
#include <fstream>
#include "Hash.h"

template<typename KType>
struct BloomFilter {
    using HashWrapper = HashWrapper<KType, MurmurHash3<KType>>;

    uint64_t bit_num;
    vector<bool> bit_array;
    HashWrapper hash_wrapper;

    explicit BloomFilter(uint64_t bit_num_ = 81920):bit_num(bit_num_),bit_array(bit_num_, false) {}

    void put(const KType &key){
        auto hash = hash_wrapper.hash(key, sizeof(key));
        for(auto h: hash) {
            bit_array[h % bit_num] = true;
        }
    }

    bool contains(const KType &key) {
        auto hash = hash_wrapper.hash(key, sizeof(key));
        for(auto h: hash) {
            if(!bit_array[h % bit_num]) {
                return false;
            }
        }
        return true;
    }
};

template<typename KType>
struct SSTableHeader {
    uint64_t timestamp;
    uint64_t kv_count;
    KType min_key;
    KType max_key;

    uint64_t getHeaderSpace() {
        return sizeof(timestamp) + sizeof(kv_count) + sizeof(min_key) + sizeof(max_key);
    }
};

struct SSTableIndex
{
    uint64_t key;
    uint64_t offset;
    SSTableIndex(uint64_t key_, uint64_t offset_): key(key_), offset(offset_) {}
};


template<typename KType, typename VType>
class SSTable {
public:
    uint32_t level;
    uint32_t order;
    
    SSTableHeader<KType> header;
    BloomFilter<KType> bloom_filter;
    vector<SSTableIndex> index;

    size_t getIndexSpace() const
    {
        return index.size() * (sizeof(SSTableIndex::key) + sizeof(SSTableIndex::offset));
    }


    void writeToFile(std::ofstream& ofs)
    {
        ofs.write(reinterpret_cast<const char*>(&header.timestamp), sizeof(header.timestamp));
        ofs.write(reinterpret_cast<const char*>(&header.kv_count), sizeof(header.kv_count));
        ofs.write(reinterpret_cast<const char*>(&header.min_key), sizeof(header.min_key));
        ofs.write(reinterpret_cast<const char*>(&header.max_key), sizeof(header.max_key));
        for(int i = 0; i < bloom_filter.bit_array.size(); i += sizeof(bool) * 8)
        {
            uint8_t byte = 0;
            for(int j = 0; j < 8; j++)
                byte |= (bloom_filter.bit_array[i + j] << (8 - j - 1));
            ofs.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
        }
        for(auto index: index) {
            ofs.write(reinterpret_cast<const char*>(&index.key), sizeof(index.key));
            ofs.write(reinterpret_cast<const char*>(&index.offset), sizeof(index.offset));
        }
    }

};