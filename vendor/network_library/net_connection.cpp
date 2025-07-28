#include "net_connection.h"
#include "net_common.h"
#include "net_safeDeque.h"
#include "net_message.h"


namespace net {
	Connection::Connection(asio::io_context& asioContext,
		asio::ip::tcp::socket socket,
		SafeDeque<OwnedMessage>& safeDequeIncomingMessages,
		std::function<void(std::error_code, Message)> onSendError,
		std::function<void(std::string)> onDisconnect)
		: m_asioContext(asioContext),
		m_socket(std::move(socket)),
		m_ownerLoginHash(""),
		m_sender(this, asioContext, m_socket, onSendError),
		m_receiver(this, m_socket, safeDequeIncomingMessages),
		m_onDisconnect(onDisconnect)
	{
		m_receiver.startReceiving();
	}

	Connection::~Connection() {}
	
	void Connection::disconnect() {
		if (m_socket.is_open()) {
			m_socket.close();
		}

		m_onDisconnect(m_ownerLoginHash);
	}

	void Connection::setOwnerLoginHash(const std::string& ownerLoginHash) {
		m_ownerLoginHash = ownerLoginHash;
	}

	const std::string& Connection::getOwnerLoginHash() {
		return m_ownerLoginHash;
	}

	void Connection::send(const Message& message) {
		m_sender.send(message);
	}


	asio::ip::tcp::endpoint Connection::getEndpoint() {
		return m_socket.remote_endpoint();
	}
}

