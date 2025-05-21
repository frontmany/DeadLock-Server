#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_connection.h"

namespace net {
	template<typename T>
	class server_interface {
	public:
		server_interface(uint16_t port) 
		: m_asio_acceptor(m_asio_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {

		}

		virtual ~server_interface() {
			stop();
		}

		bool start() {
			try {
				waitForClientConnections();
				m_context_thread = std::thread([this]() {m_asio_context.run(); });
			}
			catch (std::runtime_error e) {
				std::cout << "[SERVER] Start Error: " << e.what() << "\n";
			}

			std::cout << "[SERVER] Started!\n";
			return true;
		}

		void stop() {
			m_asio_context.stop(); 

			if (m_context_thread.joinable())
				m_context_thread.join();

			std::cout << "[SERVER] Stopped!\n";
		}

		void waitForClientConnections() {
			m_asio_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
				if (!ec) {
					std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

					std::shared_ptr<connection<T>> newConnection = std::make_shared<connection<T>>(connection<T>::owner::server,
						m_asio_context, std::move(socket), m_safe_deque_incoming_messages);

					newConnection->setServer(this);

					if (onClientConnect(newConnection)) {
						m_deque_connections.push_back(std::move(newConnection));
						m_deque_connections.back()->connectToClient();
					}
					else {
						std::cout << "[-----] Connection Denied\n";
					}
				}
				else {
					std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
				}

				waitForClientConnections();
			});
		}

		void sendMessage(std::shared_ptr<connection<T>> connection, const message<T>& msg) {
			if (connection && connection->isConnected())
				connection->send(msg);
			else {
				onClientDisconnect(connection);
				connection.reset();

				m_deque_connections.erase(std::remove(m_deque_connections.begin(), 
					m_deque_connections.end(), connection), m_deque_connections.end());
			}
		}

		void broadcastMessage(const message<T>& msg, std::shared_ptr<connection<T>> connectionToIgnore = nullptr) {
			bool isInvalidConnectionAppears = false;

			for (auto& connection : m_deque_connections) {
				if (connection && connection->isConnected()) {
					if (connection != connectionToIgnore) {
						connection->send(msg);
					}
					else {
						onClientDisconnect(connection);
						connection.reset();
						isInvalidConnectionAppears = true;
					}
				}
			} 

			if (isInvalidConnectionAppears) {
				m_deque_connections.erase(std::remove(m_deque_connections.begin(),
					m_deque_connections.end(), nullptr), m_deque_connections.end());
			}
		}

		void update(size_t maxMessagesCount = std::numeric_limits<unsigned long long>::max()) {
			m_safe_deque_incoming_messages.wait();

			size_t  processedMessagesCount = 0;
			while (processedMessagesCount < maxMessagesCount && !m_safe_deque_incoming_messages.empty()) {
				auto msg = m_safe_deque_incoming_messages.pop_front();

				onMessage(msg.remote, msg);

				processedMessagesCount++;
			}
		}

		virtual void onClientValidated(std::shared_ptr<connection<T>> connection) {}

		virtual void onClientDisconnect(std::shared_ptr<connection<T>> connection) {}

	protected:
		virtual bool onClientConnect(std::shared_ptr<connection<T>> connection) {
			return true;
		}


		virtual void onMessage(std::shared_ptr<connection<T>> connection, owned_message<T>& msg) {
		}

	protected:
		safe_deque<owned_message<T>>				m_safe_deque_incoming_messages;
		std::deque<std::shared_ptr<connection<T>>>  m_deque_connections;

		asio::io_context		m_asio_context;
		std::thread			m_context_thread;
		asio::ip::tcp::acceptor m_asio_acceptor;
	};
}