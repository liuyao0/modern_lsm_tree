#pragma once
#include <string>
#include <cfloat>

template<typename Derived, typename T>
struct DeleteMarker_Base {
    static bool isDeleted(T value_) {
        return value_ == Derived::value();
    }
};

template<typename T>
struct DeleteMarker: public DeleteMarker_Base<DeleteMarker<T>, T>
{
    static T value() {
        return T();
    }
};


template<>
struct DeleteMarker<std::string> : public DeleteMarker_Base<DeleteMarker<std::string>, std::string>
{
    static std::string value() {
        return std::string("~DELETED~");
    }
};

template<>
struct DeleteMarker<int> : public DeleteMarker_Base<DeleteMarker<int>, int>
{
    static int value() {
        return -1;
    }
};

template<>
struct DeleteMarker<float> : public DeleteMarker_Base<DeleteMarker<float>, float>
{
    static float value() {
        return FLT_MAX;
    }
};