#pragma once

#include "net_common.h"
#include "net_connection.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"

namespace net {

	template<typename T>
	class client_interface {
	public:
		client_interface() {}

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
				bindCallbacks();
				m_connection->connectToServer(endpoint);

				m_files_connection = std::make_unique<connection<T>>(connection<T>::owner::client,
					m_context,
					asio::ip::tcp::socket(m_context),
					m_safe_deque_of_incoming_messages,
					&m_safe_deque_of_incoming_files);
				bindCallbacks();
				m_files_connection->connectToServer(endpoint);

				m_context_thread = std::thread([this]() {m_context.run(); });

				return true;
			}
			catch (std::exception& e) {
				std::cerr << "Client exception: " << e.what() << "\n";
				return false;
			}
		}

		void disconnect() {
			if (isConnected(m_connection)) {
				m_connection->disconnect();
			}
			if (isConnected(m_files_connection)) {
				m_files_connection->disconnect();
			}

			m_context.stop();

			if (m_context_thread.joinable())
				m_context_thread.join();

			m_connection.reset();
			m_files_connection.reset();
		}

		bool isConnected(std::unique_ptr<connection<T>>& connection) {
			if (connection)
				return connection->isConnected();
			else
				return false;
		}

		void send(const message<T>& msg)
		{
			if (isConnected(m_connection))
				m_connection->send(msg);
			else {
				disconnect();
			}
		}

		void sendFileOnFileConnection(const file<T>& file)
		{
			if (isConnected(m_files_connection))
				m_files_connection->sendFile(file);
			else {
				disconnect();
			}
		}

		void sendMessageOnFileConnection(const message<T>& msg)
		{
			if (isConnected(m_files_connection))
				m_files_connection->send(msg);
			else {
				disconnect();
			}
		}

		void update(size_t maxMessagesCount = std::numeric_limits<unsigned long long>::max()) {
			size_t processedMessages = 0;

			while (true) {
				if (!m_safe_deque_of_incoming_files.empty()) {
					net::owned_file<T> file = m_safe_deque_of_incoming_files.pop_front();
					onFile(std::move(file.file));
				}

				if (!m_safe_deque_of_incoming_messages.empty() && processedMessages < maxMessagesCount) {
					net::owned_message<T> msg = m_safe_deque_of_incoming_messages.pop_front();
					onMessage(std::move(msg.msg));
					processedMessages++;
				}

				std::this_thread::yield();
			}
		}

		void bindCallbacks() {
			if (m_connection) {
				m_connection->setOnSendMessageError(
					[this](std::error_code ec, net::message<T> msg) {
						this->onSendMessageError(ec, std::move(msg));
					}
				);

				m_connection->setOnReadMessageError(
					[this](std::error_code ec) {
						this->onReadMessageError(ec);
					}
				);

				m_connection->setOnConnectError(
					[this](std::error_code ec) {
						this->onConnectError(ec);
					}
				);
			}

			if (m_files_connection) {
				m_files_connection->setOnSendMessageError(
					[this](std::error_code ec, net::message<T> msg) {
						this->onSendMessageError(ec, std::move(msg));
					}
				);

				m_files_connection->setOnReadMessageError(
					[this](std::error_code ec) {
						this->onReadMessageError(ec);
					}
				);

				m_files_connection->setOnSendFileChunkError(
					[this](std::error_code ec, net::file<T> file) {
						this->onSendFileError(ec, std::move(file));
					}
				);

				m_files_connection->setOnReadFileChunkError(
					[this](std::error_code ec, net::file<T> file) {
						this->onReadFileError(ec, std::move(file));
					}
				);

				m_files_connection->setOnConnectError(
					[this](std::error_code ec) {
						this->onConnectError(ec);
					}
				);

				m_files_connection->setOnFileSent(
					[this](net::file<T> file) {
						this->onFileSent(std::move(file));
					}
				);
			}
		}

		void supplyFileData(std::string myLogin, std::string friendLogin, std::string filePath, std::string fileId, uint32_t fileSize, std::string fileTimestamp, std::string caption, const std::string& blobUID, size_t filesInBlobCount) {
			m_files_connection->supplyFileData(myLogin, friendLogin, filePath, fileId, fileSize, fileTimestamp, caption, blobUID, filesInBlobCount);
			m_files_connection->readFile();
		}

	protected:
		virtual void onMessage(net::message<T> message) = 0;
		virtual void onFile(net::file<T> file) = 0;
		virtual void onFileSent(net::file<T> sentFile) = 0;

		//errors
		virtual void onSendMessageError(std::error_code ec, net::message<T> unsentMessage) = 0;
		virtual void onSendFileError(std::error_code ec, net::file<T> unsentFile) = 0;

		virtual void onReadMessageError(std::error_code ec) = 0;
		virtual void onReadFileError(std::error_code ec, net::file<T> unreadFile) = 0;

		virtual void onConnectError(std::error_code ec) = 0;

	private:
		std::thread	 m_context_thread;
		asio::io_context m_context;
		std::unique_ptr<connection<T>> m_connection;
		std::unique_ptr<connection<T>> m_files_connection;
		safe_deque<owned_message<T>> m_safe_deque_of_incoming_messages;
		safe_deque<owned_file<T>> m_safe_deque_of_incoming_files;
	};
}

