#pragma once

#include <iostream>
#include <memory>
#include <random>
#include <vector>
#include "SkipList.h"
#include "DeleteMarker.h"

using std::vector;
using std::unique_ptr;

template<typename KType, typename VType>
struct KVWrapper {
    KType* key_ptr;
    VType* value_ptr;
    KVWrapper(KType* key_ptr_, VType* value_ptr_): key_ptr(key_ptr_), value_ptr(value_ptr_) {}
};

template<typename KType, typename VType>
class MemTable {
    private:
        SkipList<KType, VType> skip_list;
        uint64_t size;
    public:
        MemTable(int max_level = 16, float probability = 0.5):
            skip_list(max_level, probability), size(0)
        {}

        void put(const KType key, const VType value) {
            size += skip_list.put(key, value);
        }

        unique_ptr<VType> get(const KType& key) const {
            return skip_list.get(key);
        }

        int remove(const KType key) {
            size += skip_list.insert(key, DeleteMarker<VType>::value());
        }

        uint64_t get_size() const {
            return size;
        }

        int get_min_max_key(KType &min_key, KType &max_key) const {
            return skip_list.get_min_max_key(min_key, max_key);
        }

        vector<KVWrapper<KType, VType>> get_all_kv() const {
            vector<KVWrapper<KType, VType>> kv_wrappers;
            auto current = skip_list.head;
            while(current->next[0] != nullptr) {
                kv_wrappers.push_back(KVWrapper(&(current->next[0]->key), &(current->next[0]->value)));
                current = current->next[0];
            }
            return kv_wrappers;
        }
};
