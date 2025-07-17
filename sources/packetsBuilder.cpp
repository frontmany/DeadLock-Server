#include "packetsBuilder.h"
#include "user.h"



std::string PacketsBuilder::get_friendsStatusesSuccessPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::vector<std::string>& friendsLoginHashesVec,
    const std::vector<std::string>& friendsStatusesVec) {
    if (friendsLoginHashesVec.size() != friendsStatusesVec.size()) {
        std::cout << "error size friendsLoginsVec != friendsStatusesVec";
        return "";
    }

    std::ostringstream oss;

    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKey, key);

    oss << encryptedKey << '\n';
    oss << vecBegin << '\n';
    for (size_t i = 0; i < friendsLoginHashesVec.size(); i++) {
        oss << crypto::AESEncrypt(key, friendsLoginHashesVec[i] + ',' + friendsStatusesVec[i]) << '\n';
    }
    oss << vecEnd;

    return oss.str();
}






std::string PacketsBuilder::get_usersPacket(const CryptoPP::RSA::PrivateKey privateKey, const CryptoPP::RSA::PublicKey& userPublicKeyTo, const std::vector<User*>& usersVec) {
    std::ostringstream oss;

    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKeyTo, key);

    oss << encryptedKey << '\n';
    oss << crypto::AESEncrypt(key, std::to_string(usersVec.size())) << '\n';

    for (auto user : usersVec) {
        oss << crypto::AESEncrypt(key, user->getLogin()) << '\n'
            << crypto::AESEncrypt(key,user->getName()) << '\n'
            << crypto::AESEncrypt(key, crypto::RSADecrypt(privateKey, user->getLastSeen())) << '\n'
            << crypto::AESEncrypt(key, (user->getIsHasPhoto() ? "true" : "false")) << '\n'
            << crypto::AESEncrypt(key, std::to_string(user->getPhoto().getSize())) << '\n';

        if (user->getIsHasPhoto()) {
            oss << crypto::AESEncrypt(key, user->getPhoto().serialize(privateKey, userPublicKeyTo)) << '\n';
        }

        oss << crypto::serializePublicKey(user->getPublicKey());
    }
    return oss.str();
}

std::string PacketsBuilder::get_userInfoPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user, const CryptoPP::RSA::PublicKey& userPublicKeyTo, const std::string& newLogin) {
    std::ostringstream oss;
    if (!oss.good()) {
        throw std::runtime_error("String stream is in a bad state!");
    }
    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKeyTo, key);

    oss << encryptedKey << '\n'
        << crypto::AESEncrypt(key, user->getLogin()) << '\n'
        << crypto::AESEncrypt(key, user->getName()) << '\n'
        << crypto::AESEncrypt(key, user->getLastSeen()) << '\n'
        << crypto::AESEncrypt(key, (user->getIsHasPhoto() ? "true" : "false")) << '\n';

        if (user->getIsHasPhoto()) {
            oss << user->getPhoto().serialize(privateKey, userPublicKeyTo) << '\n';
        }

    oss << crypto::serializePublicKey(user->getPublicKey()) << '\n';

    if (newLogin != "") {
        oss << crypto::AESEncrypt(key, newLogin);
    }
        

    return oss.str();
}







std::string PacketsBuilder::get_newLoginSuccessPacket(const std::string& approvedLogin, const CryptoPP::RSA::PublicKey& userPublicKey) {
    std::ostringstream oss;

    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKey, key);

    oss << encryptedKey << '\n'
        << crypto::AESEncrypt(key, approvedLogin);

    return oss.str();
}

std::string PacketsBuilder::get_chatCreateSuccessPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user, const CryptoPP::RSA::PublicKey& userPublicKeyTo) {
    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKeyTo, key);

    
    std::ostringstream oss;
    oss << encryptedKey << '\n'
        << crypto::AESEncrypt(key, user->getLogin()) << '\n'
        << crypto::AESEncrypt(key, user->getName()) << '\n'
        << crypto::AESEncrypt(key, (user->getPhoto().getSize() > 0 ? "true" : "false")) << '\n'
        << crypto::AESEncrypt(key, std::to_string(user->getPhoto().getSize())) << '\n'
        << crypto::AESEncrypt(key, "last seen: N/A") << '\n';
    
    if (user->getIsHasPhoto()) {
        oss << user->getPhoto().serialize(privateKey, userPublicKeyTo);
    }

    oss << crypto::serializePublicKey(user->getPublicKey());
    return oss.str();
}

std::string PacketsBuilder::get_MyInfoPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user) {
    std::ostringstream oss;
    if (!oss.good()) {
        throw std::runtime_error("String stream is in a bad state!");
    }
    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(user->getPublicKey(), key);

    oss << encryptedKey << '\n'
        << crypto::AESEncrypt(key, user->getLogin()) << '\n'
        << crypto::AESEncrypt(key, user->getName()) << '\n'
        << crypto::AESEncrypt(key, user->getLastSeen()) << '\n'
        << crypto::AESEncrypt(key, (user->getIsHasPhoto() ? "true" : "false")) << '\n'
        << crypto::AESEncrypt(key, std::to_string(user->getPhoto().getSize())) << '\n'
        << user->getPhoto().serialize(privateKey, user->getPublicKey());

    return oss.str();
}

std::string PacketsBuilder::get_statusPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::string& loginHash, const std::string& status) {
    std::ostringstream oss;

    CryptoPP::SecByteBlock key;
    crypto::generateAESKey(key);
    std::string encryptedKey = crypto::RSAEncryptKey(userPublicKey, key);

    oss << encryptedKey << '\n'
        << loginHash << '\n'
        << crypto::AESEncrypt(key, status);

    return oss.str();
}

std::string PacketsBuilder::get_fileCollectPacket(const std::string& encryptedKey, const std::string& senderLoginHash, const std::string& receiverLoginHash, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& timestamp, const std::string& caption, const std::string& blobUID, const std::string& filesInBlobCount, bool isSent) {
    std::ostringstream oss;

    oss << encryptedKey << '\n'
        << fileId << '\n'
        << blobUID << '\n'
        << receiverLoginHash << '\n'
        << senderLoginHash << '\n'
        << fileName << '\n'
        << fileSize << '\n'
        << timestamp << '\n'
        << caption << '\n'
        << filesInBlobCount;

    return oss.str();
}

std::string PacketsBuilder::get_registrationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey) {
    std::ostringstream oss;

    oss << encryptionPart << '\n'
        << crypto::serializePublicKey(serverPublicKey);

    return oss.str();
}
std::string PacketsBuilder::get_authorizationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey) {
    std::ostringstream oss;

    oss << encryptionPart << '\n'
        << crypto::serializePublicKey(serverPublicKey);

    return oss.str();
}