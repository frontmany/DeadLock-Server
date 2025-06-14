#pragma once

#include "net_common.h"
#include "net_connection.h"
#include "net_files_connection.h"
#include "net_validator.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"

namespace net {

	template<typename T>
	class client_interface {
	public:
		client_interface() 
		{
			m_validator = std::make_unique<validator>(
				m_context,
				asio::ip::tcp::socket(m_context),
				asio::ip::tcp::socket(m_context),
				[this](std::error_code ec) { onConnectError(ec); },
				[this](asio::ip::tcp::socket socket) { createFilesConnection(std::move(socket)); },
				[this](asio::ip::tcp::socket socket) { createMessagesConnection(std::move(socket)); }
			);
		}

		virtual ~client_interface() {
			disconnect();
		}

		bool connectMessagesSocket(const std::string& host, const uint16_t port) {
			try {
				asio::ip::tcp::resolver resolver(m_context);
				asio::ip::tcp::resolver::results_type endpoint = resolver.resolve(host, std::to_string(port));
				m_validator->connectMessagesSocketToServer(endpoint);

				m_context_thread = std::thread([this]() {m_context.run(); });

				return true;
			}
			catch (std::exception& e) {
				std::cerr << "Client exception: " << e.what() << "\n";
				return false;
			}
		}

		bool connectFilesSocket(const std::string& login, const std::string& host, const uint16_t port) {
			try {
				asio::ip::tcp::resolver resolver(m_context);
				asio::ip::tcp::resolver::results_type endpoint = resolver.resolve(host, std::to_string(port));
				m_validator->connectFilesSocketToServer(std::move(login), endpoint);

				return true;
			}
			catch (std::exception& e) {
				std::cerr << "Client exception: " << e.what() << "\n";
				return false;
			}
		}

		void disconnect() {
			try {
				if (m_messages_connection) {
					m_messages_connection->disconnect();
					m_messages_connection.reset();
				}

				if (m_files_connection) {
					m_files_connection->disconnect();
					m_files_connection.reset();
				}

				m_context.stop();

				if (m_context_thread.joinable()) {
					m_context_thread.join();
				}
			}
			catch (const std::exception& e) {
				std::cout << "error on disconnect " << e.what();
			}
		}

		bool isConnected() {
			if (m_messages_connection && m_files_connection) {
				return m_messages_connection->isConnected() && m_files_connection->isConnected();
			}
			else {
				return false;
			}
		}

		void send(const message<T>& msg)
		{
			if (isConnected())
				m_messages_connection->send(msg);
			else {
				disconnect();
			}
		}
		
		void sendFile(net::file<T> file)
		{
			if (isConnected())
				m_files_connection->sendFile(file);
			else {
				disconnect();
			}
		}

		void update(size_t maxMessagesCount = std::numeric_limits<size_t>::max()) {
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

	protected:
		virtual void onMessage(net::message<T> message) = 0;
		virtual void onFile(net::file<T> file) = 0;
		virtual void onFileSent(net::file<T> sentFile) = 0;
		virtual void onSendFileProgressUpdate(const net::file<T>& file, uint32_t progressPercent) = 0;
		virtual void onReceiveFileProgressUpdate(const net::file<T>& file, uint32_t progressPercent) = 0;

		//errors
		virtual void onSendMessageError(std::error_code ec, net::message<T> unsentMessage) = 0;
		virtual void onReceiveMessageError(std::error_code ec) = 0;
		
		virtual void onSendFileError(std::error_code ec, net::file<T> unsentFile) = 0;
		virtual void onReceiveFileError(std::error_code ec, net::file<T> unreadFile) = 0;

		virtual void onConnectError(std::error_code ec) = 0;

		bool is_messages_socket_validated = false;

	private:
		void createMessagesConnection(asio::ip::tcp::socket messagesSocket) {
			m_messages_connection = std::make_unique<connection<T>>(
				owner::client,
				m_context,
				std::move(messagesSocket),
				m_safe_deque_of_incoming_messages,
				[this](std::error_code ec, net::message<T> unsentMessage) {onSendMessageError(ec, std::move(unsentMessage)); },
				[this](std::error_code ec) {onReceiveMessageError(ec); }
			);

			is_messages_socket_validated = true;
		}

		void createFilesConnection(asio::ip::tcp::socket filesSocket) {
			m_files_connection = std::make_unique<files_connection<T>>(
				owner::client,
				m_context,
				std::move(filesSocket),
				m_safe_deque_of_incoming_files,
				[this](std::error_code ec, net::file<T> unreadFile) {onReceiveFileError(ec, unreadFile); },
				[this](std::error_code ec, net::file<T> unsentFile) {onSendFileError(ec, unsentFile); },
				[this](net::file<T> file) {onFileSent(file); },
				[this](net::file<T> file, uint32_t progressPercent) {onSendFileProgressUpdate(file, progressPercent); },
				[this](net::file<T> file, uint32_t progressPercent) {onReceiveFileProgressUpdate(file, progressPercent); }
			);
		}

	private:
		std::unique_ptr<validator> m_validator;
		std::thread	m_context_thread;
		asio::io_context m_context;
		std::unique_ptr<connection<T>> m_messages_connection;
		std::unique_ptr<files_connection<T>> m_files_connection;
		safe_deque<owned_message<T>> m_safe_deque_of_incoming_messages;
		safe_deque<owned_file<T>> m_safe_deque_of_incoming_files;
	};
}

