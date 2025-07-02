#include "photo.h"
#include "crypto.h"
#include <locale>
#include <codecvt>

Photo::Photo(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& photoPath)
    : m_photoPath(photoPath), m_size(0), m_private_key(privateKey) {
    if (photoPath != "") {
        updateSize();
    }
}

Photo::Photo()
    : m_photoPath(""), m_size(0), m_private_key({}) {
}

void Photo::updateSize() {
    std::ifstream file(m_photoPath, std::ios::binary);
    if (!file) return;

    try {
        std::string encryptedKey;
        if (!std::getline(file, encryptedKey)) return;

        std::string encryptedData(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        CryptoPP::SecByteBlock aesKey = crypto::RSADecryptKey(m_private_key, encryptedKey);

        std::string decryptedData = crypto::AESDecrypt(aesKey, encryptedData);
        m_size = decryptedData.size();
    }
    catch (...) {
        m_size = 0;
    }
}


 
std::string Photo::serialize(const CryptoPP::RSA::PrivateKey& privateKey, const CryptoPP::RSA::PublicKey& userPublicKey) const {
    if (m_photoPath.empty()) {
        std::cout << "error in (encryptForServerBase64) function filePath is empty\n";
        return "";
    }

    std::ifstream file(m_photoPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open photo file: " + m_photoPath);
    }

    std::string encryptedKey;
    if (!std::getline(file, encryptedKey)) {
        std::cout << "error in (encryptForServerBase64) function cannot read the encryptedKey\n";
        return "";
    }

    std::string encryptedData(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    CryptoPP::SecByteBlock aesKey = crypto::RSADecryptKey(privateKey, encryptedKey);

    std::string fileData = crypto::AESDecrypt(aesKey, encryptedData);
    file.close();

    try {
        CryptoPP::SecByteBlock aesKey;
        crypto::generateAESKey(aesKey);

        std::string encryptedData = crypto::AESEncrypt(aesKey, fileData);

        std::string encryptedKey = crypto::RSAEncryptKey(userPublicKey, aesKey);

        std::string result = encryptedKey + "\n" + encryptedData;
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Encryption failed: " + std::string(e.what()));
    }
}


std::optional<Photo> Photo::deserialize(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& data, const std::string& loginHash) {
    if (data.empty()) {
        return std::nullopt;
    }

    const std::string saveDirectory = "./ReceivedPhotos";
    try {
        std::filesystem::create_directories(saveDirectory);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return std::nullopt;
    }

    const std::string path = saveDirectory + "/" + loginHash + ".dph";

    if (std::filesystem::exists(path)) {
        try {
            std::filesystem::remove(path);
            std::cout << "Existing photo deleted: " << path << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to delete existing photo: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    try {
        std::ofstream outFile(path, std::ios::binary);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing");
        }

        outFile.write(data.data(), data.size());
        outFile.close();

        std::cout << "Photo saved successfully: " << path << std::endl;
        return Photo(privateKey, path);

    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save photo: " << e.what() << std::endl;
        return std::nullopt;
    }
}