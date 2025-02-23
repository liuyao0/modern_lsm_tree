#include "DeleteMarker.h"
#include <cstdio>
#include <string>
#include <map>


int main()
{   
    std::map<int, std::string> int2str;
    auto str_delete_value = DeleteMarker<std::string>::value();
    auto str_isDeleted = DeleteMarker<std::string>::isDeleted;

    int2str[1] = "one";
    int2str[2] = "two";
    int2str[3] = "three";
    int2str[4] = str_delete_value;

    for (auto it = int2str.begin(); it != int2str.end(); ++it) {
            printf("Key: %d, Value: %s", it->first, it->second.c_str());
            if(str_isDeleted(it->second)) {
                printf(" (deleted)");
            }
            printf("\n");
    }

    std::map<int, float> int2float;
    auto float_delete_value = DeleteMarker<float>::value();
    auto float_isDeleted = DeleteMarker<float>::isDeleted;

    int2float[1] = 1.0;
    int2float[2] = 2.0;
    int2float[3] = float_delete_value;
    int2float[4] = 4.0;

    for (auto it = int2float.begin(); it != int2float.end(); ++it) {
            printf("Key: %d, Value: %f", it->first, it->second);
            if(float_isDeleted(it->second)) {
                printf(" (deleted)");
            }
            printf("\n");
    }

    std::map<int, double> int2double;
    auto double_delete_value = DeleteMarker<double>::value();
    auto double_isDeleted = DeleteMarker<double>::isDeleted;
    int2double[1] = 1.0;
    int2double[2] = 2.0;
    int2double[3] = double_delete_value;
    int2double[4] = 4.0;

    for (auto it = int2double.begin(); it != int2double.end(); ++it) {
            printf("Key: %d, Value: %f", it->first, it->second);
            if(double_isDeleted(it->second)) {
                printf(" (deleted)");
            }
            printf("\n");
    }

    return 0;
}