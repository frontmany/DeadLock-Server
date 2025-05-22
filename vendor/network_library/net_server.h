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
						m_asio_context,
						std::move(socket),
						m_safe_deque_incoming_messages,
						[this](std::shared_ptr<connection<T>> connectionPtr) {this->onClientDisconnect(connectionPtr); });

					if (onClientConnect(newConnection)) {
						m_set_connections.push_back(std::move(newConnection));
						m_set_connections.back()->connectToClient();
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
				connection->send(msg, [this](net::message<T> msg) {this->onSendMessageError(msg); }, [this](net::file<T> file) {this->onSendFileError(file); });
			else {
				onClientDisconnect(connection);
				connection.reset();
				auto it = std::find(m_set_connections.begin(), m_set_connections.end(), connection);
				if (it != m_set_connections.end()) {
					m_set_connections.erase(it);
				}
			}
		}


		void sendFileOnFileConnection(std::shared_ptr<connection<T>> connection, const file<T>& file)
		{
			if (isConnected(connection))
				connection->sendFile(file, [this](net::file<T> file) {this->onSendFileError(file); });
			else {
				onClientDisconnect(connection);
				connection.reset();
				auto it = std::find(m_set_connections.begin(), m_set_connections.end(), connection);
				if (it != m_set_connections.end()) {
					m_set_connections.erase(it);
				}
			}
		}

		void sendMessageOnFileConnection(std::shared_ptr<connection<T>> connection, const message<T>& msg)
		{
			if (isConnected(connection))
				connection->send(msg, [this](net::message<T> msg) {this->onSendMessageError(msg); }, [this](net::file<T> file) {this->onSendFileError(file); });
			else {
				onClientDisconnect(connection);
				connection.reset();
				auto it = std::find(m_set_connections.begin(), m_set_connections.end(), connection);
				if (it != m_set_connections.end()) {
					m_set_connections.erase(it);
				}
			}
		}

		void broadcastMessage(const message<T>& msg, std::shared_ptr<connection<T>> connectionToIgnore = nullptr) {
			bool isInvalidConnectionAppears = false;

			for (auto& connection : m_set_connections) {
				if (connection && connection->isConnected()) {
					if (connection != connectionToIgnore) {
						connection->send(msg, [this](net::message<T> msg) {this->onSendMessageError(msg); }, [this](net::file<T> file) {this->onSendFileError(file); });
					}
					else {
						onClientDisconnect(connection);
						connection.reset();
						isInvalidConnectionAppears = true;
					}
				}
			}

			if (isInvalidConnectionAppears) {
				m_set_connections.erase_if([](const auto& ptr) { return ptr.get() == nullptr; });
			}
		}

		void update(size_t maxMessagesCount = std::numeric_limits<unsigned long long>::max()) {
			size_t processedMessages = 0;

			while (true) {
				if (!m_safe_deque_of_incoming_files.empty()) {
					net::owned_file<T> file = m_safe_deque_of_incoming_files.pop_front();
					onFile(std::move(file.file));
				}

				if (!m_safe_deque_incoming_messages.empty() && processedMessages < maxMessagesCount) {
					net::owned_message<T> msg = m_safe_deque_incoming_messages.pop_front();
					onMessage(msg.remote, std::move(msg.msg));
					processedMessages++;
				}

				std::this_thread::yield();
			}
		}

		bool isConnected(std::shared_ptr<connection<T>>& connection) {
			if (connection)
				return connection->isConnected();
			else
				return false;
		}

	protected:
		virtual void onMessage(std::shared_ptr<connection<T>> connection, message<T> msg) = 0;
		virtual void onFile(net::file<T> file) = 0;
		virtual void onSendMessageError(net::message<T> unsentMessage) = 0;
		virtual void onSendFileError(net::file<T> unsentFille) = 0;
		virtual bool onClientConnect(std::shared_ptr<connection<T>> connection) = 0;
		virtual void onClientDisconnect(std::shared_ptr<connection<T>> connection) = 0;

	protected:
		safe_deque<owned_message<T>>				m_safe_deque_incoming_messages;
		safe_deque<owned_file<T>>					m_safe_deque_of_incoming_files;
		std::deque<std::shared_ptr<connection<T>>>	m_set_connections;

		asio::io_context		m_asio_context;
		std::thread			m_context_thread;
		asio::ip::tcp::acceptor m_asio_acceptor;
	};
}