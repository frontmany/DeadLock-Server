#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdlib>
#include <cassert>  

#include <rsa.h> 


namespace crypto {
    // Cryptography functions
    void generateAESKey(CryptoPP::SecByteBlock& key);

    // RSA operations
    std::string RSAEncrypt(const CryptoPP::RSA::PublicKey& publicKey, const CryptoPP::SecByteBlock& data);
    CryptoPP::SecByteBlock RSADecrypt(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& cipher);

    // AES operations
    std::string AESEncrypt(const CryptoPP::SecByteBlock& key, const std::string& plain);
    std::string AESDecrypt(const CryptoPP::SecByteBlock& key, const std::string& cipher);

    // Key serialization
    std::string serializeKey(const CryptoPP::RSA::PublicKey& key);
    std::string serializeKey(const CryptoPP::RSA::PrivateKey& key);
    CryptoPP::RSA::PublicKey deserializePublicKey(const std::string& keyStr);
    CryptoPP::RSA::PrivateKey deserializePrivateKey(const std::string& keyStr);

    // Key validation
    bool validateKeys(const CryptoPP::RSA::PublicKey& publicKey, const CryptoPP::RSA::PrivateKey& privateKey);
    bool validatePublicKey(const CryptoPP::RSA::PublicKey& key);
    bool validatePrivateKey(const CryptoPP::RSA::PrivateKey& key);

    // hash calculation
    std::string calculateHash(const std::string& text);
}