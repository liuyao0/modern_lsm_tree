#pragma once
#include "MurmurHash3.h"
#include <vector>
#include <memory>

using std::vector;
using std::shared_ptr;

template<typename KType, typename HashFunc>
struct HashWrapper {
    static auto hash(const KType& key, const int len) -> decltype(HashFunc::hash(key, len)) {
        return HashFunc::hash(key, len);
    }
};


template<typename KType>
struct MurmurHash3 {
    static vector<uint32_t> hash(const KType& key, const int len) {
        vector<uint32_t> res(4);
        MurmurHash3_x64_128(static_cast<const void*>(&key), len, 0, res.data());
        return res;
    }
};