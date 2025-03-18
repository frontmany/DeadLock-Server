#include"sender.h"
#include"user.h"

std::string SendStringsGenerator::get_authorizationSuccessStr() {
	std::string s;
	s += "AUTHORIZATION_SUCCESS\n";
	s += endPacket;
	return s;
}

std::string SendStringsGenerator::get_friendsStatusesSuccessStr(const std::vector <std::string>& friendsLoginsVec, const std::vector <std::string>& friendsStatusesVec) {
	if (friendsLoginsVec.size() != friendsStatusesVec.size()) {
		std::cout << "error size friendsLoginsVec != friendsStatusesVec";
		return "";
	}

	std::string s;
	s += "FRIENDS_STATUSES\n";
	s += vecBegin + '\n';
	for (int i = 0; i < friendsLoginsVec.size(); i++) {
		s += friendsLoginsVec[i] + ',' + friendsStatusesVec[i] + '\n';
	}
	s += vecEnd + '\n';
	s += endPacket;
	return s;
}

std::string SendStringsGenerator::get_authorizationFailStr() {
	return "AUTHORIZATION_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_registrationSuccessStr() {
	return "REGISTRATION_SUCCESS\n" + endPacket;
}

std::string SendStringsGenerator::get_registrationFailStr() {
	return "REGISTRATION_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_chatCreateSuccessStr(User* user) {
	return "CHAT_CREATE_SUCCESS\n" + user->getLogin() + '\n' + user->getName() + '\n' + (user->getIsHasPhoto() == true ? "true" : "false") + '\n'
		+ user->getPhoto().serialize() + '\n' + user->getLastSeen() + '\n' + endPacket;
}

std::string SendStringsGenerator::get_chatCreateFailStr() {
	return "CHAT_CREATE_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_messageSuccessStr() {
	return "MESSAGE_SUCCESS\n" + endPacket;
}

std::string SendStringsGenerator::get_messageFailStr() {
	return "MESSAGE_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_messageReadConfirmationSuccessStr() {
	return "MESSAGE_READ_CONFIRAMTION_SUCCESS\n" + endPacket;
}

std::string SendStringsGenerator::get_messageReadConfirmationFailStr() {
	return "MESSAGE_READ_CONFIRAMTION_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_userInfoUpdatedSuccessStr() {
	return "USER_INFO_UPDATED_SUCCESS\n" + endPacket;
}

std::string SendStringsGenerator::get_userInfoUpdatedFailStr() {
	return "USER_INFO_UPDATED_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_userInfoSuccessStr(User* user) {
	return "USER_INFO_SUCCESS\n" + user->getLogin() + '\n' + user->getName() + '\n' + (user->getIsHasPhoto() == true ? "true" : "false") + '\n'
		+ user->getPhoto().serialize() + '\n' + user->getLastSeen() + '\n' + endPacket;
}
std::string SendStringsGenerator::get_userInfoFailStr() {
	return "USER_INFO_FAIL\n" + endPacket;
}

std::string SendStringsGenerator::get_statusStr(const std::string& login, const std::string& status) {
	return "STATUS\n" + login + '\n' + status + '\n' + endPacket;
}

std::string SendStringsGenerator::get_loginToSendStatusStr(const std::string& login) {
	return "LOGIN_TO_SEND_STATUS\n" + login + '\n' + endPacket;
}

