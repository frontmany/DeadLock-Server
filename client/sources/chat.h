#pragma once
#include<vector>

#include"photo.h"
#include"message.h"



class Chat {
public:
	Chat() : m_is_friend_has_photo(false) {}

	std::vector<double>& getNotReadSendMessagesVec() { return m_vec_not_read_send_messages_id; }
	std::vector<double>& getNotReadReceivedMessagesVec() { return m_vec_not_read_received_messages_id; }
	std::vector<double>& getMessagesVec() { return m_vec_not_read_received_messages_id; }

	void setFriendLogin(const std::string& friendLogin) { m_friend_login = friendLogin; }
	const std::string& getFriendLogin() const { return m_friend_login; }

	void setLastIncomeMsg(const std::string& lastIncomeMessage) { m_last_incoming_message = lastIncomeMessage; }
	const std::string& getLastIncomeMessage() const { return m_last_incoming_message; }

	void setFriendName(const std::string& name) { m_friend_name = name; }
	const std::string& getFriendName() const { return m_friend_name; }

	void setFriendLastSeen(const std::string& lastSeen) { m_friend_last_seen = lastSeen; }
	const std::string& getFriendLastSeen() const { return m_friend_last_seen; }

	

	void setIsFriendHasPhoto(const bool isHasPhoto) { m_is_friend_has_photo = isHasPhoto; }
	const bool getIsFriendHasPhoto() const { return m_is_friend_has_photo; }

	void setFriendPhoto(const Photo& photo) { m_friend_photo = photo; }
	const Photo& getFriendPhoto() const { return m_friend_photo; }

	static int generateId();

private:
	static int m_minimum_available_id;

	std::vector<double>		m_vec_not_read_received_messages_id;
	std::vector<double>		m_vec_not_read_send_messages_id;
	std::vector<Message>	m_vec_messages;

	std::string				 m_friend_last_seen;
	std::string				 m_friend_login;
	std::string				 m_friend_name;
	std::string				 m_last_incoming_message;
	bool					 m_is_friend_has_photo;
	Photo					 m_friend_photo;
};