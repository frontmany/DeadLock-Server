#pragma once
#include<string>
#include<iostream>
#include<vector>

#include "crypto.h"

class User;


class PacketsBuilder {
public:
    std::string get_friendsStatusesSuccessPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::vector <std::string>& friendsLoginHashesVec, const std::vector <std::string>& friendsStatusesVec);
    std::string get_usersPacket(const CryptoPP::RSA::PrivateKey privateKey, const CryptoPP::RSA::PublicKey& userPublicKeyTo, const std::vector <User*>& usersVec);
    std::string get_userInfoPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user, const CryptoPP::RSA::PublicKey& userPublicKeyTo, const std::string& newLogin = "");
    std::string get_MyInfoPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user);
    std::string get_chatCreateSuccessPacket(const CryptoPP::RSA::PrivateKey privateKey, User* user, const CryptoPP::RSA::PublicKey& userPublicKeyTo);
    std::string get_statusPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::string& login, const std::string& status);
    std::string get_updateOfferPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::string& versionNumber);

    std::string get_fileCollectPacket(const std::string& encryptedKey, const std::string& senderLoginHash, const std::string& receiverLoginHash, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& timestamp, const std::string& caption, const std::string& blobUID, const std::string& filesInBlobCount, bool isSent = false);

    std::string get_registrationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey);
    std::string get_authorizationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey);

    std::string get_newLoginSuccessPacket(const std::string& approvedLogin, const CryptoPP::RSA::PublicKey& userPublicKey);
    std::string get_avatarsKeyPacket(const CryptoPP::SecByteBlock avatarsKey);

private:
    const std::string vecBegin = "VEC_BEGIN";
    const std::string vecEnd = "VEC_END";
    static constexpr const char* messageBegin = "MESSAGE_BEGIN";
    static constexpr const char* messageEnd = "MESSAGE_END";
};