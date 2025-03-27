#pragma once
#include<string>
#include<iostream>
#include<vector>

class User;

// for RPL packets only
enum class Status { NOT_STATED, SEND, NOT_SEND };

class SendStringsGenerator {
public:
    std::string get_authorizationSuccessStr();
    std::string get_friendsStatusesSuccessStr(const std::vector <std::string>& friendsLoginsVec, const std::vector <std::string>& friendsStatusesVec);
	std::string get_authorizationFailStr();
    std::string get_registrationSuccessStr();
    std::string get_registrationFailStr();
    std::string get_chatCreateFailStr();
    std::string get_messageSuccessStr();
    std::string get_messageFailStr();
    std::string get_messageReadConfirmationSuccessStr();
    std::string get_messageReadConfirmationFailStr();
    std::string get_userInfoUpdatedSuccessStr();
    std::string get_userInfoUpdatedFailStr();
    std::string get_userInfoSuccessStr(User* user);
    std::string get_userInfoFailStr();
    std::string get_loginToSendStatusStr(const std::string& login);
    std::string get_newLoginSuccessStr();
    std::string get_newLoginFailStr();
    std::string get_chatCreateSuccessStr(User* user);
    std::string get_statusStr(const std::string& login, const std::string& status);

    std::string get_userInfoPacket(const std::string& login, const std::string& name, const std::string& isHasPhotoStr, const std::string& photoStr);

private:
    const std::string vecBegin = "VEC_BEGIN";
    const std::string vecEnd = "VEC_END";
    const std::string endPacket = "_+14?bb5HmR;%@`7[S^?!#sL8";
};