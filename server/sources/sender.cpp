#include "sender.h"
#include "user.h"

std::string SendStringsGenerator::get_authorizationSuccessStr() {
    std::ostringstream oss;
    oss << "AUTHORIZATION_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_friendsStatusesSuccessStr(const std::vector<std::string>& friendsLoginsVec,
    const std::vector<std::string>& friendsStatusesVec) {
    if (friendsLoginsVec.size() != friendsStatusesVec.size()) {
        std::cout << "error size friendsLoginsVec != friendsStatusesVec";
        return "";
    }

    std::ostringstream oss;
    oss << "FRIENDS_STATUSES\n"
        << vecBegin << '\n';

    for (size_t i = 0; i < friendsLoginsVec.size(); i++) {
        oss << friendsLoginsVec[i] << ',' << friendsStatusesVec[i] << '\n';
    }

    oss << vecEnd << '\n'
        << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_authorizationFailStr() {
    std::ostringstream oss;
    oss << "AUTHORIZATION_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_registrationSuccessStr() {
    std::ostringstream oss;
    oss << "REGISTRATION_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_registrationFailStr() {
    std::ostringstream oss;
    oss << "REGISTRATION_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_newLoginSuccessStr() {
    std::ostringstream oss;
    oss << "NEW_LOGIN_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_newLoginFailStr() {
    std::ostringstream oss;
    oss << "NEW_LOGIN_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_chatCreateSuccessStr(User* user) {
    std::ostringstream oss;
    oss << "CHAT_CREATE_SUCCESS\n"
        << user->getLogin() << '\n'
        << user->getName() << '\n'
        << (user->getPhoto().getSize() > 0 ? "true" : "false") << '\n'
        << std::to_string(user->getPhoto().getSize()) << '\n'
        << user->getLastSeen() << '\n'
        << user->getPhoto().serialize() << '\n'
        << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_chatCreateFailStr() {
    std::ostringstream oss;
    oss << "CHAT_CREATE_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_messageSuccessStr() {
    std::ostringstream oss;
    oss << "MESSAGE_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_messageFailStr() {
    std::ostringstream oss;
    oss << "MESSAGE_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_messageReadConfirmationSuccessStr() {
    std::ostringstream oss;
    oss << "MESSAGE_READ_CONFIRAMTION_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_messageReadConfirmationFailStr() {
    std::ostringstream oss;
    oss << "MESSAGE_READ_CONFIRAMTION_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_userInfoUpdatedSuccessStr() {
    std::ostringstream oss;
    oss << "USER_INFO_UPDATED_SUCCESS\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_userInfoUpdatedFailStr() {
    std::ostringstream oss;
    oss << "USER_INFO_UPDATED_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_userInfoSuccessStr(User* user) {
    std::ostringstream oss;
    oss << "USER_INFO_SUCCESS\n"
        << user->getLogin() << '\n'
        << user->getName() << '\n'
        << (user->getIsHasPhoto() ? "true" : "false") << '\n'
        << std::to_string(user->getPhoto().getSize()) << '\n'
        << user->getPhoto().serialize() << '\n'
        << user->getLastSeen() << '\n'
        << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_userInfoFailStr() {
    std::ostringstream oss;
    oss << "USER_INFO_FAIL\n" << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_statusStr(const std::string& login, const std::string& status) {
    std::ostringstream oss;
    oss << "STATUS\n"
        << login << '\n'
        << status << '\n'
        << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_loginToSendStatusStr(const std::string& login) {
    std::ostringstream oss;
    oss << "LOGIN_TO_SEND_STATUS\n"
        << login << '\n'
        << endPacket;
    return oss.str();
}

std::string SendStringsGenerator::get_userInfoPacket(const std::string& oldLogin, const std::string& newLogin,
    const std::string& name,
    const std::string& isHasPhotoStr,
    const std::string& size,
    const std::string& photoStr) {
    std::ostringstream oss;
    oss << "FRIEND_INFO\n"
        << oldLogin << '\n'
        << newLogin << '\n'
        << name << '\n'
        << isHasPhotoStr << '\n'
        << size << '\n'
        << photoStr << '\n'
        << endPacket;
    return oss.str();
}