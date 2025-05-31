#pragma once
#include<string>
#include<memory>

namespace net 
{
    template <typename T>
    class connection;

    template <typename T>
    struct file {
        uint32_t filesInBlobCount;
        std::string  blobUID;
        std::string senderLogin;
        std::string receiverLogin;
        std::string filePath;
        std::string id;
        std::string timestamp;
        uint32_t fileSize;
        std::string caption;
    };

    template <typename T>
    struct owned_file {
        std::shared_ptr<connection<T>> remote = nullptr;
        file<T> file;
    };
}