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

class Avatar {
public:
    Avatar(const CryptoPP::SecByteBlock& avatarsKey, const std::string& photoPath);
    Avatar();

    static void rename(const std::string& oldName, const std::string& newName);

    const std::string& getPath() const { return m_photoPath; }
    const std::size_t getSize() const { return m_size; }
    const std::size_t getEncryptedSize() const { return m_encryptedSize; }

private:
    void update(const CryptoPP::SecByteBlock& avatarsKey);

private:
    std::string m_photoPath;
    std::size_t m_size;
    std::size_t m_encryptedSize;
};

