# pragma once
#include<string>
#include<vector>

struct Blob {
    std::string receiverLoginHash;
    std::string senderLoginHash;
    std::string blobUID;
    int filesCountInBlob = 0;
    int filesReceived = 0;
    int filesSent = 0;
    std::vector<std::string> filePacketsVec{};
};