#pragma once

#include "net_message.h"
#include "net_safeDeque.h"
#include "asio.hpp"


namespace net {
	class Connection;

	class Sender {
	public:
		Sender() = default;

		Sender(asio::io_context* asioContext,
			asio::ip::tcp::socket* socket, 
			std::function<void(std::error_code, Message)> onSendMessageError,
			std::function<void()> onDisconnect
		);

		Sender(const Sender&) = delete;
		Sender& operator=(const Sender&) = delete;

		Sender& operator=(Sender&& other) noexcept;
		Sender(Sender&& other) noexcept;

		void send(const Message& msg);

	private:
		void writeHeader();
		void writeBody();

	private:
		asio::ip::tcp::socket* m_socket = nullptr;
		asio::io_context* m_asioContext = nullptr;

		SafeDeque<Message> m_safeDequeOutgoingMessages;

		std::function<void(std::error_code, Message)> m_onSendMessageError;
		std::function<void()> m_onDisconnect;
	};
}

