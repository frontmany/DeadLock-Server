#include "sender.h"
#include "user.h"



std::string SendStringsGenerator::get_friendsStatusesSuccessStr(const std::vector<std::string>& friendsLoginsVec,
    const std::vector<std::string>& friendsStatusesVec) {
    if (friendsLoginsVec.size() != friendsStatusesVec.size()) {
        std::cout << "error size friendsLoginsVec != friendsStatusesVec";

        return "";
    }

    std::ostringstream oss;
    oss << vecBegin << '\n';

    for (size_t i = 0; i < friendsLoginsVec.size(); i++) {
        oss << friendsLoginsVec[i] << ',' << friendsStatusesVec[i] << '\n';
    }
    oss << vecEnd;

    return oss.str();
}

std::string SendStringsGenerator::get_usersStr(const std::vector<User*>& usersVec) {
    std::ostringstream oss;

    oss << std::to_string(usersVec.size()) << '\n';

    for (auto user : usersVec) {
        oss << user->getLogin() << '\n'
            << user->getName() << '\n'
            << user->getLastSeen() << '\n'
            << (user->getIsHasPhoto() ? "true" : "false") << '\n'
            << std::to_string(user->getPhoto().getSize()) << '\n'
            << user->getPhoto().serialize() << '\n';
    }

    return oss.str();
}

std::string SendStringsGenerator::get_chatCreateSuccessStr(User* user) {
    std::ostringstream oss;
    oss << user->getLogin() << '\n'
        << user->getName() << '\n'
        << (user->getPhoto().getSize() > 0 ? "true" : "false") << '\n'
        << std::to_string(user->getPhoto().getSize()) << '\n'
        << "last seen: N/A" << '\n'
        << user->getPhoto().serialize();

    return oss.str();
}

std::string SendStringsGenerator::get_userInfoPacket(User* user, const std::string newLogin) {
    std::ostringstream oss;
    oss << user->getLogin() << '\n'
        << user->getName() << '\n'
        << user->getLastSeen() << '\n'
        << (user->getIsHasPhoto() ? "true" : "false") << '\n'
        << std::to_string(user->getPhoto().getSize()) << '\n'
        << user->getPhoto().serialize() << '\n'
        << newLogin;

    return oss.str();
}

std::string SendStringsGenerator::get_filePreviewStr(const std::string& friendLogin, const std::string& myLogin, const std::string& fileName, const std::string& fileId) {
    std::ostringstream oss;
    oss << myLogin << '\n'
        << friendLogin << '\n'
        << fileName << '\n'
        << fileId << '\n';

    return oss.str();
}

std::string SendStringsGenerator::get_statusStr(const std::string& login, const std::string& status) {
    std::ostringstream oss;
    oss << login << '\n'
        << status;

    return oss.str();
}

std::string SendStringsGenerator::get_prepareToReceiveFileStr(const std::string& myLogin, const std::string& friendLogin, size_t fileSize, const std::string& fileName, const std::string& fileId) {
    std::ostringstream oss;
    oss << myLogin << '\n'
        << friendLogin << '\n'
        << std::to_string(fileSize)<< '\n'
        << fileName << '\n'
        << fileId << '\n';

    return oss.str();
}