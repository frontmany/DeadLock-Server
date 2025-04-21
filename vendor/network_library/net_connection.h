#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"

namespace net {

	template <typename T>
	class server_interface;

	template<typename T>
	class connection : public std::enable_shared_from_this<connection<T>> {
	public:

		enum class owner {
			server, 
			client
		};

		connection(owner owner, asio::io_context& asioContext, asio::ip::tcp::socket socket, safe_deque<owned_message<T>>& safeDequeIncomingMessages) 
			: m_asio_context(asioContext), m_socket(std::move(socket)), m_safe_deque_incoming_messages(safeDequeIncomingMessages) {
		
			m_owner_type = owner;

			if (m_owner_type == owner::server) {
				m_hand_shake_out = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

				m_hand_shake_check = scramble(m_hand_shake_out);
			}
			else {
				m_hand_shake_in = 0;
				m_hand_shake_out = 0;
			}
		}

		virtual ~connection() {}

		void connectToServer(const asio::ip::tcp::resolver::results_type& endpoint) {
			if (m_owner_type == owner::client) {
				asio::async_connect(m_socket, endpoint,
					[this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
						if (!ec) {
							std::cout << "Connected to: " << endpoint << std::endl;
							readValidation();
							readHeader();
						}
						else {
							std::cerr << "Connection failed: " << ec.message() << std::endl;
						}
					});
			}
		}

		void connectToClient(server_interface<T>* server) {
			if (m_owner_type == owner::server) {
				if (m_socket.is_open()) {
					writeValidation();
					readValidation(server);
				}
			}
		}

		void disconnect() {
			if (isConnected()) {
				asio::post(m_asio_context, [this]() { m_socket.close(); });
			}
		}

		bool isConnected() const {
			return m_socket.is_open();
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

	private:
		void readHeader() {
			asio::async_read(m_socket, asio::buffer(&m_message_tmp.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
						if (m_message_tmp.header.size > 0) {
							m_message_tmp.body.resize(m_message_tmp.header.size - sizeof(message_header<T>));
							readBody();
						}
						else
							addToIncomingMessagesQueue();
					}
					else {
						m_socket.close();
					}
				});
		}

		void readBody() {
			asio::async_read(m_socket, asio::buffer(m_message_tmp.body.data(), m_message_tmp.body.size()), 
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
						addToIncomingMessagesQueue();
					}
					else {
						m_socket.close();
					}
				});
		}

		void addToIncomingMessagesQueue() {
			if (m_owner_type == owner::server) 
				m_safe_deque_incoming_messages.push_back({ this->shared_from_this(), m_message_tmp });
			else
				m_safe_deque_incoming_messages.push_back({ nullptr, m_message_tmp });
			
			readHeader();
		}

		void writeHeader() {
			asio::async_write(m_socket, asio::buffer(&m_safe_deque_outgoing_messages.front().header, sizeof(message_header<T>)), 
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
						if (m_safe_deque_outgoing_messages.front().body.size() > 0) {
							writeBody();
						}
						else {
							m_safe_deque_outgoing_messages.pop_front();

							if (!m_safe_deque_outgoing_messages.empty()) {
								writeHeader();
							}
						}
					}
				});
		}

		void writeBody() {
			asio::async_write(m_socket, asio::buffer(m_safe_deque_outgoing_messages.front().body.data(), m_safe_deque_outgoing_messages.front().body.size()),
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
						m_safe_deque_outgoing_messages.pop_front();

						if (!m_safe_deque_outgoing_messages.empty()) {
							writeHeader();
						}
					
					}
					else {
						m_socket.close();
					}
				});
		}

		uint64_t scramble(uint64_t inputNumber) {
			uint64_t out = inputNumber ^ 0xDEADBEEFC;
			out = (out & 0xF0F0F0F0F) >> 4 | (out & 0x0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}

		void writeValidation() {
			asio::async_write(m_socket, asio::buffer(&m_hand_shake_out, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
						if (m_owner_type == owner::client)
							readHeader();
					}
					else
						m_socket.close();
				});
		}

		void readValidation(server_interface<T>* server = nullptr) {
			asio::async_read(m_socket, asio::buffer(&m_hand_shake_in, sizeof(uint64_t)),
				[this, server](std::error_code ec, std::size_t length) {
					if (!ec) {
						if (m_owner_type == owner::client) {
							m_hand_shake_out = scramble(m_hand_shake_in);
							writeValidation();
						}
						else {
							if (m_hand_shake_in == m_hand_shake_check) {
								std::cout << "Client Validated\n";
								server->onClientValidated(this->shared_from_this());
								readHeader();
							}
							else {
								std::cout << "Client Disconnected (Fail Validation)\n";
								m_socket.close();
							}
						}
					}
					else {
						m_socket.close();
					}
						
				});
		}

	protected:
		message<T> m_message_tmp;

		owner					m_owner_type = owner::server;
		asio::ip::tcp::socket	m_socket;
		asio::io_context&		m_asio_context;

		safe_deque<message<T>> m_safe_deque_outgoing_messages;
		safe_deque<owned_message<T>>& m_safe_deque_incoming_messages;

		uint64_t m_hand_shake_out = 0;
		uint64_t m_hand_shake_in = 0;
		uint64_t m_hand_shake_check = 0;
	};
}
