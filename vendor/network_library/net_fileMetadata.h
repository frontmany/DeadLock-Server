#pragma once
#include <string>
#include <rsa.h>

namespace net
{
    struct FileMetadata {
        CryptoPP::RSA::PublicKey friendPublicKey;
        std::string blobUID;
        std::string senderLoginHash;
        std::string receiverLoginHash;
        std::string filePath;
        std::string fileName;
        std::string id;
        std::string timestamp;
        std::string caption;
        std::string filesInBlobCount;
        std::string fileSize;
        std::string encryptedKey;
    };
}