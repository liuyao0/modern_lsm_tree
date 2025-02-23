#include "SSTable.h"
#include <iostream>
#include <cstdint>

int main()
{
    BloomFilter<uint64_t> bloom_filter(81920);
    for(uint64_t i = 0; i < 5; i++) {
        bloom_filter.put(i);
    }

    for(uint64_t i = 0; i < 10; i++) {
        if (bloom_filter.contains(i)) {
            std::cout << i << " is in the bloom filter" << std::endl;
        } else {
            std::cout << i << " is not in the bloom filter" << std::endl;
        }
    }
    return 0;
}