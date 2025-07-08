#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <optional>

#include "rsa.h"

class Photo {
public:
    Photo(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& photoPath);
    Photo();

    const std::string& getPhotoPath() const { return m_photoPath; }
    void setPhotoPath(std::string& photoPath) { m_photoPath = photoPath; updateSize(); }
    std::size_t getSize() const { return m_size; }

    std::string serialize(const CryptoPP::RSA::PrivateKey& privateKey, const CryptoPP::RSA::PublicKey& userPublicKey) const;
    static std::optional<Photo> deserialize(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& data, const std::string& loginHash);

private:
    void updateSize();

private:
    std::string m_photoPath;
    std::size_t m_size;
    CryptoPP::RSA::PrivateKey m_private_key;
};

