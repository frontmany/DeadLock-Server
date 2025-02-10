#include"message.h"

std::string Message::serialize() const {
	return m_message + "|" + m_timestamp + "|" + std::to_string(m_id) + "|" + (m_is_from_me ? "1" : "0") + '|' + (m_is_read ? "1" : "0");
}

Message Message::deserialize(const std::string& data) {
	size_t pos1 = data.find('|');
	size_t pos2 = data.find('|', pos1 + 1);
	size_t pos3 = data.find('|', pos2 + 1);
	size_t pos4 = data.find('|', pos3 + 1);

	std::string message = data.substr(0, pos1);
	std::string timestamp = data.substr(pos1 + 1, pos2 - pos1 - 1);
	int			id = std::stoi(data.substr(pos2 + 1, pos3 - pos2 - 1));
	bool		isFromMe = (data.substr(pos3 + 1) == "1");
	bool		isRead = (data.substr(pos4 + 1) == "1");

	Message msg(std::move(message), std::move(timestamp), id, isFromMe, isRead);
	return msg;
}