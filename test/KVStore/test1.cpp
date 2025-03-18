#include "KVStore.h"
#include <string>
#include <iostream>
#include <format>

int main()
{
    KVStore<uint64_t, std::string> kv_store("./db");

    for(int i = 0; i < 33; i++)
    {
        if(i % 2 == 0)
            kv_store.put(i, string(i, 's'));
    }

    for(int i = 0; i < 33; i++)
    {
        auto val_ptr = kv_store.get(i);
        if(val_ptr) {
            std::cout << std::format("key: {}, value: {}\n", i, *val_ptr) << std::endl;
            if(*val_ptr == string(i, 's'))
                std::cout << "correct" << std::endl;
        }
        else
            std::cout << std::format("key: {}, value: null\n", i) << std::endl;
    }
    return 0;
}