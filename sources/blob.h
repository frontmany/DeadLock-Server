# pragma once

#include<fstream>
#include<string>
#include<vector>

#include "net_file.h"

template <typename T>
struct filesBlob {
    filesBlob(size_t received, size_t overAllCount, const std::vector<net::file<T>>& vec) {
        this->received = received;
        this->overAllCount = overAllCount;
        this->filesVec = vec;
    }
    size_t sent = 0;
    size_t received = 0;
    size_t overAllCount = 0;
    std::vector<net::file<T>> filesVec;
};