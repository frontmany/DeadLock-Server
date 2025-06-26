#pragma once
#include<string>
#include<iostream>
#include<vector>

#include "crypto.h"

class User;


class PacketsBuilder {
public:
    std::string get_friendsStatusesSuccessPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::vector <std::string>& friendsLoginHashesVec, const std::vector <std::string>& friendsStatusesVec);
    std::string get_usersPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::vector <User*>& usersVec);
    std::string get_userInfoPacket(User* user, const std::string newLogin = "");
    std::string get_chatCreateSuccessPacket(User* user);
    std::string get_statusPacket(const CryptoPP::RSA::PublicKey& userPublicKey, const std::string& login, const std::string& status);

    std::string get_filePreviewPacket(const std::string& senderLoginHash, const std::string& receiverLoginHash, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& timestamp, const std::string& caption, const std::string& blobUID, uint32_t filesInBlobCount);

    std::string get_registrationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey);
    std::string get_authorizationSuccessPacket(const std::string& encryptionPart, const CryptoPP::RSA::PublicKey& serverPublicKey);


private:
    const std::string vecBegin = "VEC_BEGIN";
    const std::string vecEnd = "VEC_END";
    static constexpr const char* messageBegin = "MESSAGE_BEGIN";
    static constexpr const char* messageEnd = "MESSAGE_END";
};