#pragma once
#include<string>
#include<iostream>
#include<vector>

class User;


class SendStringsGenerator {
public:
    std::string get_friendsStatusesSuccessStr(const std::vector <std::string>& friendsLoginsVec, const std::vector <std::string>& friendsStatusesVec);
    std::string get_userInfoPacket(User* user);
    std::string get_chatCreateSuccessStr(User* user);
    std::string get_statusStr(const std::string& login, const std::string& status);

private:
    const std::string vecBegin = "VEC_BEGIN";
    const std::string vecEnd = "VEC_END";
    const std::string endPacket = "_+14?bb5HmR;%@`7[S^?!#sL8";
};