#include<iostream>
#include<vector>

class Photo;

class SendStringsGenerator {
public:
	enum class QueryType {
		AUTHORIZATION,
		REGISTRATION,
		CREATE_CHAT,
		UPDATE_MY_INFO,
		MESSAGE,
		MESSAGES_READ_CONFIRMATION,
		STATUS
		//LOAD_ALL_FRIENDS_STATUSES
	};

	std::string get_authorization_QueryStr(const std::string& login, const std::string& password);
	std::string get_registration_QueryStr(const std::string& login, const std::string& name, const std::string& password);
	std::string get_createChat_QueryStr(const std::string& myLogin, const std::string& friendLogin);
	std::string get_message_ReplyStr(const std::string& myLogin, const std::string& friendLogin, const std::string& message, const std::string& timestamp);
	std::string get_status_ReplyStr(const std::string& status, const std::string& myLogin, std::vector<std::string>& friendsLoginsVec);
	std::string get_messageReadConfirmation_ReplyStr(const std::string& myLogin, const std::string& friendLogin, const std::vector<int>& readMessagesIdsVec);
	std::string get_updateMyInfo_QueryStr(std::string login, std::string name, std::string password, bool isHasPhoto, Photo photo);
	//std::string get_loadFriendInfo_QueryStr(const std::string& login);
	//std::string get_loadAllFriendsInfo_QueryStr(const std::vector<std::string>& friendsLoginsVec);

private:
	std::string parseTypeToStr(QueryType type);

private:
	const std::string get = "GET";
	const std::string rpl = "RPL";
	const std::string broadcast = "BROADCAST";
	const std::string messageBegin = "MESSAGE_BEGIN";
	const std::string messageEnd = "MESSAGE_END";
	const std::string vecBegin = "VEC_BEGIN";
	const std::string vecEnd = "VEC_END";
	const std::string endPacket = "_+14?bb5HmR;%@`7[S^?!#sL8";
};