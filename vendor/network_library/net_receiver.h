#pragma once
#include "net_message.h"
#include "net_safeDeque.h"
#include "asio.hpp"

namespace net {
	class Connection;

	class Receiver {
	public:
		Receiver() = default;

		Receiver(asio::ip::tcp::socket* socket,
			std::function<void(Message)> queueReceivedMessage,
			std::function<void()> onDisconnect
		);

		Receiver(Receiver&& other) noexcept;
		Receiver& operator=(Receiver&& other) noexcept;

		Receiver(const Receiver&) = delete;
		Receiver& operator=(const Receiver&) = delete;

		void startReceiving();

	private:
		void readHeader();
		void readBody();

	private:
		asio::ip::tcp::socket* m_socket;
		Message	m_temporaryMessage;

		std::function<void()> m_onClientDisconnected;
		std::function<void(Message)> m_queueReceivedMessage;
		std::function<void()> m_onDisconnect;
	};
}

