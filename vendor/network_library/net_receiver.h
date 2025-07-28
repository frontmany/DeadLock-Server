#pragma once
#include "net_message.h"
#include "net_safeDeque.h"

namespace net {
	class Connection;

	class Receiver {
	public:
		Receiver(Connection* connection, 
			asio::ip::tcp::socket& socket,
			SafeDeque<OwnedMessage>& safeDequeIncomingMessages
		);

		void startReceiving();

	private:
		void readHeader();
		void readBody();
		void addToIncomingMessagesQueue();

	private:
		Connection* m_relatedConnection;
		SafeDeque<OwnedMessage>& m_safeDequeIncomingMessages;

		asio::ip::tcp::socket& m_socket;
		Message	m_temporaryMessage;

		std::function<void()> m_onClientDisconnected;
	};
}

