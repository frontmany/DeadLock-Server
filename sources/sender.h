#pragma once
#include<string>
#include<iostream>
#include<vector>

class User;


class SendStringsGenerator {
public:
    std::string get_friendsStatusesSuccessStr(const std::vector <std::string>& friendsLoginsVec, const std::vector <std::string>& friendsStatusesVec);
    std::string get_usersStr(const std::vector <User*>& usersVec);
    std::string get_userInfoPacket(User* user, const std::string newLogin = "");
    std::string get_chatCreateSuccessStr(User* user);
    std::string get_statusStr(const std::string& login, const std::string& status);

    std::string get_filePreviewStr(const std::string& friendLogin, const std::string& myLogin, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& caption);
    std::string get_fileFastForwardStr(const std::string& friendLogin, const std::string& myLogin, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& caption);
    std::string get_prepareToReceiveFileStr(const std::string& myLogin, const std::string& friendLogin, const std::string& fileName, const std::string& fileId, const std::string& fileSize, const std::string& caption);

private:
    const std::string vecBegin = "VEC_BEGIN";
    const std::string vecEnd = "VEC_END";
    static constexpr const char* messageBegin = "MESSAGE_BEGIN";
    static constexpr const char* messageEnd = "MESSAGE_END";
};