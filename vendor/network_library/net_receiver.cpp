#include "net_receiver.h"
#include "net_connection.h"

namespace net {
	Receiver::Receiver(Connection* connection,
		asio::ip::tcp::socket& socket,
		SafeDeque<OwnedMessage>& safeDequeIncomingMessages)
		: m_socket(socket),
		m_relatedConnection(connection),
		m_safeDequeIncomingMessages(safeDequeIncomingMessages)
	{
	}

	void Receiver::startReceiving() {
		readHeader();
	}

	void Receiver::readHeader() {
		asio::async_read(m_socket, asio::buffer(&m_temporaryMessage.header, sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					if (ec == asio::error::connection_reset) {
						m_relatedConnection->disconnect();
					}
				}
				else {
					if (m_temporaryMessage.header.size > sizeof(MessageHeader)) {
						m_temporaryMessage.body.resize(m_temporaryMessage.header.size - sizeof(MessageHeader));
						readBody();
					}
					else {
						addToIncomingMessagesQueue();
					}
				}
			});
	}

	void Receiver::readBody() {
		asio::async_read(m_socket, asio::buffer(m_temporaryMessage.body.data(), m_temporaryMessage.body.size()),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					if (ec == asio::error::connection_reset) {
						m_relatedConnection->disconnect();
					}
				}
				else {
					addToIncomingMessagesQueue();
				}
			});
	}

	void Receiver::addToIncomingMessagesQueue() {
		m_safeDequeIncomingMessages.push_back({ m_relatedConnection, m_temporaryMessage });
		readHeader();
	}
}

