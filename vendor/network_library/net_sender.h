#pragma once
#include "net_message.h"
#include "net_safeDeque.h"


namespace net {
	class Connection;

	class Sender {
	public:
		Sender(Connection* connection, asio::io_context& asioContext,
			asio::ip::tcp::socket& socket, 
			std::function<void(std::error_code, Message)> onSendMessageError
		);

		void send(const Message& msg);

	private:
		void writeHeader();
		void writeBody();

	private:
		Connection* m_relatedConnection;
		SafeDeque<Message> m_safeDequeOutgoingMessages;

		asio::ip::tcp::socket& m_socket;
		asio::io_context& m_asioContext;

		std::function<void(std::error_code, Message)> m_onSendMessageError;
		std::function<void()> m_onClientDisconnected;
	};
}

