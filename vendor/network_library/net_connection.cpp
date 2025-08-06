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
		m_sender(&asioContext,
			&m_socket,
			onSendError,
			[this, onDisconnect]() {onDisconnect(m_ownerLoginHash); }),
		m_receiver(&m_socket,
			[this, &safeDequeIncomingMessages](Message msg) {safeDequeIncomingMessages.push_back({ shared_from_this(),std::move(msg) }); },
			[this, onDisconnect]() {onDisconnect(m_ownerLoginHash); })
	{
		m_receiver.startReceiving();
	}

	Connection::~Connection() 
	{
	}

	void Connection::close() {
		auto self = shared_from_this();

		asio::post(m_asioContext, [this, self]() {
			if (m_socket.is_open()) {
				std::error_code ec;
				m_socket.close(ec);
				if (ec) {
					std::cerr << "Socket close error: " << ec.message() << "\n";
				}
				else {
					std::cout << "Connection closed successfully\n";
				}
			}
		});
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

