#include "crypto.h"

#include "secblock.h"
#include "base64.h"
#include <rsa.h>                 
#include <hex.h>                    
#include <osrng.h>       
#include <sha.h>                  
#include <filters.h>               
#include <base64.h>                 
#include <gcm.h>                 
#include <aes.h>                    
#include <modes.h>                  
#include <queue.h>                                
#include <cryptlib.h> 
#include <oaep.h> 
#include <base64_my.h>

using namespace CryptoPP;

void  crypto::generateAESKey(SecByteBlock& key) {
    AutoSeededRandomPool rng;
    key = SecByteBlock(32);
    rng.GenerateBlock(key, key.size());
}

std::string crypto::RSAEncrypt(const CryptoPP::RSA::PublicKey& publicKey,
    const std::string& plainText)
{
    if (plainText.empty()) {
        return "";
    }

    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Encryptor encryptor(publicKey);

        size_t cipherSize = encryptor.CiphertextLength(plainText.size());
        std::string cipherText;
        cipherText.resize(cipherSize);

        encryptor.Encrypt(rng,
            reinterpret_cast<const CryptoPP::byte*>(plainText.data()),
            plainText.size(),
            reinterpret_cast<CryptoPP::byte*>(&cipherText[0]));

        std::string encoded;
        CryptoPP::StringSource ss(cipherText, true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(encoded),
                false 
            ));

        return encoded;
    }
    catch (const CryptoPP::Exception& e) {
        throw std::runtime_error(std::string("RSA encryption error: ") + e.what());
    }
}

std::string crypto::RSADecrypt(const CryptoPP::RSA::PrivateKey& privateKey,
    const std::string& cipherTextBase64)
{
    if (cipherTextBase64.empty()) {
        return "";
    }

    try {
        std::string cipherText;
        CryptoPP::StringSource ss(cipherTextBase64, true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(cipherText)
            ));

        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES_OAEP_SHA_Decryptor decryptor(privateKey);

        size_t maxPlainTextSize = decryptor.MaxPlaintextLength(cipherText.size());
        std::string plainText;
        plainText.resize(maxPlainTextSize);

        CryptoPP::DecodingResult result = decryptor.Decrypt(rng,
            reinterpret_cast<const CryptoPP::byte*>(cipherText.data()),
            cipherText.size(),
            reinterpret_cast<CryptoPP::byte*>(&plainText[0]));

        plainText.resize(result.messageLength);

        return plainText;
    }
    catch (const CryptoPP::Exception& e) {
        throw std::runtime_error(std::string("RSA decryption error: ") + e.what());
    }
}

std::string crypto::RSAEncryptKey(const CryptoPP::RSA::PublicKey& publicKey, const CryptoPP::SecByteBlock& data) {
    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Encryptor encryptor(publicKey);

        std::string cipher;
        CryptoPP::StringSource(
            data.data(),
            data.size(),
            true,
            new CryptoPP::PK_EncryptorFilter(
                rng,
                encryptor,
                new CryptoPP::StringSink(cipher)
            )
        );

        std::string base64Cipher;
        CryptoPP::StringSource(
            cipher,
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(base64Cipher),
                false
            )
        );

        return base64Cipher;
    }
    catch (const CryptoPP::Exception& e) {
        throw std::runtime_error(std::string("RSA encryption error: ") + e.what());
    }
}

CryptoPP::SecByteBlock crypto::RSADecryptKey(const CryptoPP::RSA::PrivateKey& privateKey,
    const std::string& base64Cipher) {
    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Decryptor decryptor(privateKey);

        std::string cipher;
        CryptoPP::StringSource(
            base64Cipher,
            true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(cipher)
            )
        );

        if (cipher.size() != privateKey.GetModulus().ByteCount()) {
            throw std::runtime_error("Invalid cipher size after Base64 decoding");
        }

        CryptoPP::SecByteBlock decrypted(decryptor.MaxPlaintextLength(cipher.size()));

        CryptoPP::DecodingResult result = decryptor.Decrypt(
            rng,
            reinterpret_cast<const CryptoPP::byte*>(cipher.data()),
            cipher.size(),
            decrypted.data()
        );

        if (!result.isValidCoding) {
            throw std::runtime_error("Failed to decrypt RSA data");
        }

        decrypted.resize(result.messageLength);
        return decrypted;

    }
    catch (const CryptoPP::Exception& e) {
        throw std::runtime_error(std::string("RSA decryption error: ") + e.what());
    }
}

std::string crypto::AESEncrypt(const SecByteBlock& key, const std::string& plain) {
    AutoSeededRandomPool rng;

    const size_t ivSize = 12;
    byte iv[ivSize];
    rng.GenerateBlock(iv, ivSize);

    GCM<AES>::Encryption enc;
    enc.SetKeyWithIV(key, key.size(), iv, ivSize);

    std::string cipher;
    AuthenticatedEncryptionFilter ef(enc, new StringSink(cipher));
    StringSource ss(plain, true, new Redirector(ef));

    std::string binaryResult;
    binaryResult.assign(reinterpret_cast<const char*>(iv), ivSize);
    binaryResult += cipher;

    std::string base64Result;
    CryptoPP::StringSource ss2(
        binaryResult, true,
        new CryptoPP::Base64Encoder(
            new CryptoPP::StringSink(base64Result),
            false
        )
    );

    return base64Result;
}

std::string crypto::AESDecrypt(const SecByteBlock& key, const std::string& cipher) {
    std::string cipherDecoded;
    CryptoPP::StringSource ss1(
        cipher, true,
        new CryptoPP::Base64Decoder(
            new CryptoPP::StringSink(cipherDecoded)
        )
    );

    const size_t ivSize = 12;
    if (cipherDecoded.size() < ivSize + 16 + 1)
        throw std::runtime_error("Invalid ciphertext");

    byte iv[ivSize];
    std::memcpy(iv, cipherDecoded.data(), ivSize);

    std::string encrypted = cipherDecoded.substr(ivSize);

    GCM<AES>::Decryption dec;
    dec.SetKeyWithIV(key, key.size(), iv, ivSize);

    std::string plain;
    AuthenticatedDecryptionFilter df(dec, new StringSink(plain));

    try {
        StringSource ss2(encrypted, true, new Redirector(df));
    }
    catch (const Exception& e) {
        throw std::runtime_error("Decryption failed: " + std::string(e.what()));
    }

    return plain;
}


std::string crypto::serializePublicKey(const RSA::PublicKey& key) {
    std::string encoded;
    ByteQueue queue;
    key.Save(queue);

    StringSink sink(encoded);
    Base64Encoder encoder(new Redirector(sink), false);
    queue.CopyTo(encoder);
    encoder.MessageEnd();

    return encoded;
}

RSA::PublicKey crypto::deserializePublicKey(const std::string& keyStr) {
    try {
        ByteQueue queue;
        StringSource ss(keyStr, true,
            new Base64Decoder(
                new Redirector(queue)
            ));

        RSA::PublicKey key;
        key.Load(queue);

        return key;
    }
    catch (const Exception& e) {
        throw std::runtime_error("Failed to deserialize public key: " + std::string(e.what()));
    }
}

std::string crypto::serializePrivateKey(const RSA::PrivateKey& key) {
    std::string encoded;
    ByteQueue queue;
    key.Save(queue);

    StringSink sink(encoded);
    Base64Encoder encoder(new Redirector(sink), false);
    queue.CopyTo(encoder);
    encoder.MessageEnd();

    return encoded;
}

RSA::PrivateKey crypto::deserializePrivateKey(const std::string& keyStr) {
    try {
        ByteQueue queue;
        StringSource ss(keyStr, true,
            new Base64Decoder(
                new Redirector(queue)
            ));

        RSA::PrivateKey key;
        key.Load(queue);
        return key;
    }
    catch (const Exception& e) {
        throw std::runtime_error("Failed to deserialize private key: " + std::string(e.what()));
    }
}

bool crypto::validateKeys(const RSA::PublicKey& publicKey, const RSA::PrivateKey& privateKey) {
    try {
        AutoSeededRandomPool rng;
        std::string testMsg = "test message";
        std::string encrypted, decrypted;

        RSAES<OAEP<SHA256>>::Encryptor e(publicKey);
        StringSource ss1(testMsg, true,
            new PK_EncryptorFilter(rng, e,
                new StringSink(encrypted)
            )
        );

        RSAES<OAEP<SHA256>>::Decryptor d(privateKey);
        StringSource ss2(encrypted, true,
            new PK_DecryptorFilter(rng, d,
                new StringSink(decrypted)
            )
        );

        return testMsg == decrypted;
    }
    catch (...) {
        return false;
    }
}

bool crypto::validatePrivateKey(const RSA::PrivateKey& key) {
    AutoSeededRandomPool rng;
    return key.GetModulus().BitCount() >= 2048 &&
        !key.GetPrivateExponent().IsZero() &&
        key.Validate(rng, 3);
}

bool crypto::validatePublicKey(const RSA::PublicKey& key) {
    try {
        CryptoPP::AutoSeededRandomPool rng;

        const bool validSize = key.GetModulus().BitCount() >= 2048;

        const bool validExponent = !key.GetPublicExponent().IsZero();

        const bool validStructure = key.Validate(rng, 3);

        return validSize && validExponent && validStructure;

    }
    catch (...) {
        return false;
    }
}

std::string crypto::calculateHash(const std::string& text) {
    using namespace CryptoPP;

    SHA256 hash;

    std::string digest;
    StringSource ss(
        text,
        true,
        new HashFilter(
            hash,
            new HexEncoder(
                new StringSink(digest)
            )
        )
    );

    return digest;
}