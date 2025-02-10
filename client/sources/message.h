#pragma once
#include<string>

class Message {
public:
	Message(const std::string& message, const std::string& timestamp, int id, bool isFromMe, bool isRead = false)
	: m_message(message), m_timestamp(timestamp), m_id(id), m_is_from_me(isFromMe), m_is_read(isRead){}

	const std::string& getMessage() const { return m_message; }
	void setMessage(const std::string& message) { m_message = message; }

	const std::string& getTimestamp() const { return m_timestamp; }
	void setTimestamp(const std::string& timestamp) { m_timestamp = timestamp; }

	void setId(double id) { m_id = id; }
	const double getId() const { return m_id; }


	void setIsSend(bool isFromMe) { m_is_from_me = isFromMe; }
	const bool getIsSend() const { return m_is_from_me; }

	void setIsRead(bool isRead) { m_is_read = isRead; }
	const bool getIsRead() const { return m_is_read; }

	std::string serialize() const;
	static Message deserialize(const std::string& data);

private:
	std::string m_message;
	std::string m_timestamp;
	double		m_id;
	bool		m_is_from_me;
	bool		m_is_read;

};
