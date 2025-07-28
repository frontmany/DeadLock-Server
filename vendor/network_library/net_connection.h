#pragma once
#include "net_common.h"
#include "net_receiver.h"
#include "net_sender.h"

namespace net {
	class Connection : public std::enable_shared_from_this<Connection> {
	public:

		Connection(asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			SafeDeque<OwnedMessage>& safeDequeIncomingMessages,
			std::function<void(std::error_code, Message)> onSendError,
			std::function<void(std::string)> onDisconnect
		);

		~Connection() {}

		bool isConnected() const;
		void disconnect();

		void setOwnerLoginHash(const std::string& ownerLoginHash);
		const std::string& getOwnerLoginHash();

		void send(const Message& message);

	private:
		std::function<void(std::string)> m_onDisconnect;
		std::string	m_ownerLoginHash;
		asio::ip::tcp::socket m_socket;
		Receiver m_receiver;
		Sender m_sender;
		asio::io_context& m_asioContext;
	};
}
