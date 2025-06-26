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

using namespace CryptoPP;

void  crypto::generateAESKey(SecByteBlock& key) {
    AutoSeededRandomPool rng;
    key = SecByteBlock(32);
    rng.GenerateBlock(key, key.size());
}


std::string crypto::RSAEncrypt(const CryptoPP::RSA::PublicKey& publicKey, const CryptoPP::SecByteBlock& data) {
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
    return cipher;
}

SecByteBlock crypto::RSADecrypt(const RSA::PrivateKey& privateKey, const std::string& cipher) {
    AutoSeededRandomPool rng;
    RSAES<OAEP<SHA256>>::Decryptor decryptor(privateKey);

    SecByteBlock decrypted(decryptor.MaxPlaintextLength(cipher.size()));

    DecodingResult result = decryptor.Decrypt(rng,
        reinterpret_cast<const byte*>(cipher.data()),
        cipher.size(), decrypted.data());

    if (!result.isValidCoding) {
        throw std::runtime_error("Failed to decrypt RSA data");
    }

    decrypted.resize(result.messageLength);
    return decrypted;
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

    std::string result;
    result.assign(reinterpret_cast<const char*>(iv), ivSize);
    result += cipher;
    return result;
}

std::string crypto::AESDecrypt(const SecByteBlock& key, const std::string& cipher) {
    if (cipher.size() < 12 + 16 + 1)
        throw std::runtime_error("Invalid ciphertext");

    const size_t ivSize = 12;
    byte iv[ivSize];
    std::memcpy(iv, cipher.data(), ivSize);

    std::string encrypted = cipher.substr(ivSize);
    GCM<AES>::Decryption dec;
    dec.SetKeyWithIV(key, key.size(), iv, ivSize);

    std::string plain;
    AuthenticatedDecryptionFilter df(dec, new StringSink(plain));

    try {
        StringSource ss(encrypted, true, new Redirector(df));
    }
    catch (const Exception& e) {
        throw std::runtime_error("Decryption failed: " + std::string(e.what()));
    }

    return plain;
}

std::string crypto::serializeKey(const RSA::PublicKey& key) {
    std::string encoded;
    ByteQueue queue;
    key.Save(queue);

    StringSink sink(encoded);
    Base64Encoder encoder(new Redirector(sink), false);
    queue.CopyTo(encoder);
    encoder.MessageEnd();

    return encoded;
}

std::string crypto::serializeKey(const RSA::PrivateKey& key) {
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