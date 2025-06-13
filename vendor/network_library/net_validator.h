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
	class validator : public std::enable_shared_from_this<validator> {
	public:
		validator(asio::io_context& asioContext,
			asio::ip::tcp::socket filesSocket,
			asio::ip::tcp::socket messagesSocket,
			std::function<void(std::error_code)> errorCallback,
			std::function<void(asio::ip::tcp::socket socket)> onFilesSocketValidated,
			std::function<void(asio::ip::tcp::socket socket)> onMessagesSocketValidated)
			: m_asio_context(asioContext),
			m_on_connect_error(std::move(errorCallback)),
			m_on_files_socket_validated(std::move(onFilesSocketValidated)),
			m_on_messages_socket_validated(std::move(onMessagesSocketValidated)),
			m_files_socket(std::move(filesSocket)),
			m_messages_socket(std::move(messagesSocket))
		{
			m_messages_socket_hand_shake_out = 0;
			m_messages_socket_hand_shake_in = 0;
			m_files_socket_hand_shake_out = 0;
			m_files_socket_hand_shake_in = 0;
		}

		validator(validator&& other) noexcept
			: m_asio_context(other.m_asio_context),
			m_files_socket(std::move(other.m_files_socket)),
			m_messages_socket(std::move(other.m_messages_socket)),
			m_messages_socket_hand_shake_out(other.m_messages_socket_hand_shake_out),
			m_messages_socket_hand_shake_in(other.m_messages_socket_hand_shake_in),
			m_files_socket_hand_shake_out(other.m_files_socket_hand_shake_out),
			m_files_socket_hand_shake_in(other.m_files_socket_hand_shake_in),
			m_on_connect_error(std::move(other.m_on_connect_error)),
			m_on_files_socket_validated(std::move(other.m_on_files_socket_validated)),
			m_on_messages_socket_validated(std::move(other.m_on_messages_socket_validated))
		{
		}

		validator(const validator&) = delete;
		validator& operator=(const validator&) = delete;

		validator& operator=(validator&& other) noexcept {
			if (this != &other) {
				m_files_socket = std::move(other.m_files_socket);
				m_messages_socket = std::move(other.m_messages_socket);

				m_messages_socket_hand_shake_out = other.m_messages_socket_hand_shake_out;
				m_messages_socket_hand_shake_in = other.m_messages_socket_hand_shake_in;
				m_files_socket_hand_shake_out = other.m_files_socket_hand_shake_out;
				m_files_socket_hand_shake_in = other.m_files_socket_hand_shake_in;

				m_on_connect_error = std::move(other.m_on_connect_error);
				m_on_files_socket_validated = std::move(other.m_on_files_socket_validated);
				m_on_messages_socket_validated = std::move(other.m_on_messages_socket_validated);
			}
			return *this;
		}

		virtual ~validator() {}

		void connectFilesSocketToServer(std::string login, const asio::ip::tcp::resolver::results_type& endpoint) {
			asio::async_connect(m_files_socket, endpoint,
				[login, this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
					if (ec) {
						disconnect();
						if (m_on_connect_error) {
							m_on_connect_error(ec);
						}
						std::cerr << "Connection failed: " << ec.message() << std::endl;
					}
					else {
						std::cout << "Connected to: " << endpoint << std::endl;
						readFilesSocketValidation(std::move(login));
					}
				});
		}

		void connectMessagesSocketToServer(const asio::ip::tcp::resolver::results_type& endpoint) {
			asio::async_connect(m_messages_socket, endpoint,
				[this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
					if (ec) {
						disconnect();
						if (m_on_connect_error) {
							m_on_connect_error(ec);
						}
						std::cerr << "Connection failed: " << ec.message() << std::endl;
					}
					else {
						std::cout << "Connected to: " << endpoint << std::endl;
						readMessagesSocketValidation();
					}
				});
		}

		void writeFilesSocketValidation(std::string login) {
			asio::async_write(m_files_socket,
				asio::buffer(&m_files_socket_hand_shake_out, sizeof(uint64_t)),
				[login, this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
						return;
					}

					uint32_t login_length = static_cast<uint32_t>(login.size());
					asio::async_write(m_files_socket,
						asio::buffer(&login_length, sizeof(uint32_t)),
						[login, this](std::error_code ec, std::size_t length) {
							if (ec) {
								disconnect();
								m_on_connect_error(ec);
								return;
							}

							asio::async_write(m_files_socket,
								asio::buffer(login.data(), login.size()),
								[login, this](std::error_code ec, std::size_t length) {
									if (ec) {
										disconnect();
										m_on_connect_error(ec);
										return;
									}

									m_on_files_socket_validated(std::move(m_files_socket));
								});
						});
				});
		}

		void writeMessagesSocketValidation() {
			asio::async_write(m_messages_socket, asio::buffer(&m_messages_socket_hand_shake_out, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
					}
					else {
						m_on_messages_socket_validated(std::move(m_messages_socket));
					}
				});
		}

		void readFilesSocketValidation(std::string login) {
			asio::async_read(m_files_socket, asio::buffer(&m_files_socket_hand_shake_in, sizeof(uint64_t)),
				[login, this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
					}
					else {
						m_files_socket_hand_shake_out = scramble(m_files_socket_hand_shake_in);
						m_files_socket_hand_shake_out++;
						writeFilesSocketValidation(std::move(login));
					}
				});
		}

		void readMessagesSocketValidation() {
			asio::async_read(m_messages_socket, asio::buffer(&m_messages_socket_hand_shake_in, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
					}
					else {
						m_messages_socket_hand_shake_out = scramble(m_messages_socket_hand_shake_in);
						writeMessagesSocketValidation();
					}
				});
		}


		void disconnect() {
			if (isConnected()) {
				asio::post(m_asio_context, [this]() { m_files_socket.close(); });
				asio::post(m_asio_context, [this]() { m_messages_socket.close(); });
			}
		}

		bool isConnected() const {
			return (m_files_socket.is_open() && m_messages_socket.is_open());
		}

		uint64_t scramble(uint64_t inputNumber) {
			uint64_t out = inputNumber ^ 0xDEADBEEFC;
			out = (out & 0xF0F0F0F0F) >> 4 | (out & 0x0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}

	private:
		asio::ip::tcp::socket m_files_socket;
		asio::ip::tcp::socket m_messages_socket;
		asio::io_context&	  m_asio_context;

		uint64_t m_messages_socket_hand_shake_out;
		uint64_t m_messages_socket_hand_shake_in;

		uint64_t m_files_socket_hand_shake_out;
		uint64_t m_files_socket_hand_shake_in;

		std::function<void(std::error_code)> m_on_connect_error;
		std::function<void(asio::ip::tcp::socket filesSocket)> m_on_files_socket_validated;
		std::function<void(asio::ip::tcp::socket messagesSocket)> m_on_messages_socket_validated;
	};
}
