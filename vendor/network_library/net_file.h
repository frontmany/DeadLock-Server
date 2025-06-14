#pragma once

#include <string>
#include <memory>

namespace net
{
    template <typename T>
    class files_connection;

    template <typename T>
    struct file {
        std::string blobUID;
        std::string senderLogin;
        std::string receiverLogin;
        std::string filePath;
        std::string fileName;
        std::string id;
        std::string timestamp;
        std::string caption;

        uint32_t filesInBlobCount;
        uint32_t fileSize;
        bool isRequested;
    };


    template <typename T>
    struct owned_file {
        std::shared_ptr<files_connection<T>> remote = nullptr;
        file<T> file;
    };
}