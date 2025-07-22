#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"

#include <fstream>              
#include <filesystem>          
#include <system_error>          
#include <string>              
#include <locale>                
#include <codecvt>   

namespace net {

	template <typename T>
	class server_interface;

	template<typename T>
	class connection : public std::enable_shared_from_this<connection<T>> {
	public:
		uint32_t m_current_bytes_sent = 0;

		connection(asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			safe_deque<owned_message<T>>& safeDequeIncomingMessages,
			std::function<void(std::error_code, net::message<T>)> onSendError,
			std::function<void(std::shared_ptr<connection<T>> connection, std::error_code)> onReceiveError)
			: m_asio_context(asioContext),
			m_socket(std::move(socket)),
			m_safe_deque_incoming_messages(safeDequeIncomingMessages),
			m_on_send_message_error(std::move(onSendError)),
			m_on_receive_message_error(std::move(onReceiveError))
		{
			readHeader();
		}

		virtual ~connection() {}

		bool isConnected() const {
			return m_socket.is_open();
		}

		void disconnect() {
			if (m_socket.is_open()) {
				asio::post(m_asio_context, [this]() { m_socket.close(); });
			}
		}

		void send(const message<T>& msg) {
			asio::post(m_asio_context, [this, msg]() {
				bool isAbleToWrite = m_safe_deque_outgoing_messages.empty();
				m_safe_deque_outgoing_messages.push_back(msg);
				if (isAbleToWrite) {
					writeHeader();
				}
			});
		}

		void setOwnerLoginHash(const std::string& ownerLoginHash) {
			m_owner_login_hash = ownerLoginHash;
		}

		const std::string& getOwnerLoginHash() {
			return m_owner_login_hash;
		}

	private:
		void writeHeader() {
			asio::async_write(
				m_socket,
				asio::buffer(&m_safe_deque_outgoing_messages.front().header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec)
					{
						disconnect();
						m_on_send_message_error(ec, m_safe_deque_outgoing_messages.pop_front());
					}
					else
					{
						if (m_safe_deque_outgoing_messages.front().body.size() > 0)
						{
							writeBody();
						}
						else
						{
							m_safe_deque_outgoing_messages.pop_front();

							if (!m_safe_deque_outgoing_messages.empty())
							{
								writeHeader();
							}
						}
					}

				}
			);
		}

		void writeBody() {
			asio::async_write(
				m_socket,
				asio::buffer(m_safe_deque_outgoing_messages.front().body.data(),
					m_safe_deque_outgoing_messages.front().body.size()),
				[this](std::error_code ec, std::size_t length)
				{
					if (ec) {
						disconnect();
						m_on_send_message_error(ec, m_safe_deque_outgoing_messages.pop_front());
					}
					else {
						m_safe_deque_outgoing_messages.pop_front();

						if (!m_safe_deque_outgoing_messages.empty())
						{
							writeHeader();
						}
					}
				}
			);
		}

		void readHeader() {
			asio::async_read(m_socket, asio::buffer(&m_message_tmp.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_on_receive_message_error(this->shared_from_this(), ec);
						disconnect();
					}
					else {
						if (m_message_tmp.header.size > sizeof(message_header<T>)) {
							m_message_tmp.body.resize(m_message_tmp.header.size - sizeof(message_header<T>));
							readBody();
						}
						else
							addToIncomingMessagesQueue();
					}
				});
		}

		void readBody() {
			asio::async_read(m_socket, asio::buffer(m_message_tmp.body.data(), m_message_tmp.body.size()),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_on_receive_message_error(this->shared_from_this(), ec);
						disconnect();
					}
					else {
						addToIncomingMessagesQueue();
					}
				});
		}

		void addToIncomingMessagesQueue() {
			m_safe_deque_incoming_messages.push_back({ this->shared_from_this(), m_message_tmp });
			readHeader();
		}

	private:
		asio::ip::tcp::socket		  m_socket;
		asio::io_context&			  m_asio_context;
		std::string					  m_owner_login_hash;

		safe_deque<message<T>>		  m_safe_deque_outgoing_messages;
		safe_deque<owned_message<T>>& m_safe_deque_incoming_messages;
		message<T>					  m_message_tmp;

		// errors
		std::function<void(std::shared_ptr<connection<T>> connection, std::error_code)> m_on_receive_message_error;
		std::function<void(std::error_code, net::message<T>)> m_on_send_message_error;

	};
}
