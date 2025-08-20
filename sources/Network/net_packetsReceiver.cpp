#include "net_receiver.h"
#include "net_connection.h"

namespace net {
	Receiver::Receiver(asio::ip::tcp::socket* socket,
		std::function<void(Message)> queueReceivedMessage,
		std::function<void()> onDisconnect)
		: m_socket(socket),
		m_queueReceivedMessage(queueReceivedMessage),
		m_onDisconnect(onDisconnect)
	{
	}

	Receiver::Receiver(Receiver&& other) noexcept
		: m_socket(std::exchange(other.m_socket, nullptr)),
		m_temporaryMessage(std::move(other.m_temporaryMessage)),
		m_queueReceivedMessage(std::move(other.m_queueReceivedMessage)),
		m_onDisconnect(std::move(other.m_onDisconnect)) {
	}

	Receiver& Receiver::operator=(Receiver&& other) noexcept {
		if (this != &other) {
			m_socket = std::exchange(other.m_socket, nullptr);
			m_temporaryMessage = std::move(other.m_temporaryMessage);
			m_queueReceivedMessage = std::move(other.m_queueReceivedMessage);
			m_onDisconnect = std::move(other.m_onDisconnect);
		}
		return *this;
	}

	void Receiver::startReceiving() {
		readHeader();
	}

	void Receiver::readHeader() {
		asio::async_read(*m_socket, asio::buffer(&m_temporaryMessage.header, sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					m_onDisconnect();
				}
				else {
					if (m_temporaryMessage.header.size > sizeof(MessageHeader)) {
						m_temporaryMessage.body.resize(m_temporaryMessage.header.size - sizeof(MessageHeader));
						readBody();
					}
					else {
						m_queueReceivedMessage(m_temporaryMessage);
						readHeader();
					}
				}
			});
	}

	void Receiver::readBody() {
		asio::async_read(*m_socket, asio::buffer(m_temporaryMessage.body.data(), m_temporaryMessage.body.size()),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					m_onDisconnect();
				}
				else {
					m_queueReceivedMessage(m_temporaryMessage);
					readHeader();
				}
			});
	}
}

