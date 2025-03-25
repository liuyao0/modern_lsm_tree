#include "KVStore.h"
#include <string>
#include <iostream>
#include <format>

int main()
{
    KVStore<uint64_t, std::string> kv_store("./db");

    for(int i = 0; i < 2020;i++)
    {
        if(i % 2 == 0)
            kv_store.put(i, std::to_string(i));
    }

    for(int i = 0; i < 2048; i++)
    {
      if(auto val_ptr = kv_store.get(i)) {
            std::cout << std::format("key: {}, value: {}\n", i, *val_ptr) << std::endl;
            if(*val_ptr == std::to_string(i))
                std::cout << "correct" << std::endl;
        }
        else
            std::cout << std::format("key: {}, value: null\n", i) << std::endl;
    }
    return 0;
}