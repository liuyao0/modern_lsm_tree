#pragma once
#include <string>
#include <cstdint>

using std::string;

template<typename T>
class SerializeWrapper {
public:
    static std::string serialize(const T &obj) {
        return "";
    }

    static T deserialize(const std::string &data) {
        return T();
    }

    static uint64_t serialize_size(const T &obj) {
        return 0;
    }
};

template<>
class SerializeWrapper<std::string> {
public:
    static std::string serialize(const std::string &obj) {
        return obj;
    }

    static std::string deserialize(const std::string &data) {
        return data;
    }

    static uint64_t serialize_size(const std::string &obj) {
        return obj.length();
    }
};
