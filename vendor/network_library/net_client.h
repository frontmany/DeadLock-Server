#pragma once

#include "net_common.h"
#include "net_connection.h"
#include "net_safe_deque.h"
#include "net_message.h"

namespace net {

	template<typename T>
	class client_interface {
	public:
		client_interface(){}

		virtual ~client_interface() {
			disconnect();
		}

		bool connect(const std::string& host, const uint16_t port) {
			try {
				asio::ip::tcp::resolver resolver(m_context);
				asio::ip::tcp::resolver::results_type endpoint = resolver.resolve(host, std::to_string(port));

				m_connection = std::make_unique<connection<T>>(connection<T>::owner::client,
					m_context,
					asio::ip::tcp::socket(m_context), 
					m_safe_deque_of_incoming_messages);

				m_connection->connectToServer(endpoint);

				m_context_thread = std::thread([this]() {m_context.run(); });
			}
			catch (std::exception& e) {
				std::cerr << "Client exception: " << e.what() << "\n";
				return false;
			}
		}

		void disconnect() {
			if (isConnected()) {
				m_connection->disconnect();
			}

			m_context.stop();

			if (m_context_thread.joinable())
				m_context_thread.join();
			 
			m_connection.release();
		}

		bool isConnected() {
			if (m_connection) 
				return m_connection->isConnected();
			else 
				return false;
		}

		void send(const message<T>& msg)
		{
			if (isConnected())
				m_connection->send(msg);
		}

		safe_deque<owned_message<T>>& getMessagesQueue() {
			return m_safe_deque_of_incoming_messages;
		}

	protected:
		std::thread	 m_context_thread;
		asio::io_context m_context;
		std::unique_ptr<connection<T>> m_connection;


	private:
		safe_deque<owned_message<T>> m_safe_deque_of_incoming_messages;
	};
}

