#include "net_connectionTypeResolver.h"


namespace net {
	ConnectionTypeResolver::ConnectionTypeResolver(
		asio::io_context& asioContext,
		asio::ip::tcp::socket socket,
		std::function<void(std::error_code)> errorCallback,
		std::function<void(asio::ip::tcp::socket socket, connection_type type, std::optional<std::string> login)> onConnectionResolved)
		: m_asio_context(asioContext),
		m_socket(std::move(socket)),
		m_on_connect_error(std::move(errorCallback)),
		m_on_connection_resolved(std::move(onConnectionResolved))
	{
		m_hand_shake_out = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
		m_hand_shake_in = 0;
		m_hand_shake_check_is_for_files = 0;
		m_hand_shake_check_is_for_messages = 0;
		m_login_length = 0;

		m_hand_shake_check_is_for_messages = scramble(m_hand_shake_out);
		m_hand_shake_check_is_for_files = m_hand_shake_check_is_for_messages;
		m_hand_shake_check_is_for_files++;

		writeValidation();
		readValidation();
	}

	ConnectionTypeResolver::~ConnectionTypeResolver() {}


	uint64_t ConnectionTypeResolver::scramble(uint64_t inputNumber) {
		uint64_t out = inputNumber ^ 0xDEADBEEFC;
		out = (out & 0xF0F0F0F0F) >> 4 | (out & 0x0F0F0F0F0F) << 4;
		return out ^ 0xC0DEFACE12345678;
	}

	void ConnectionTypeResolver::writeValidation() {
		asio::async_write(m_socket, asio::buffer(&m_hand_shake_out, sizeof(uint64_t)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					disconnect();
					m_on_connect_error(ec);
				}
			});
	}

	void ConnectionTypeResolver::readValidation() {
		asio::async_read(m_socket, asio::buffer(&m_hand_shake_in, sizeof(uint64_t)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					disconnect();
					m_on_connect_error(ec);
				}
				else {
					if (m_hand_shake_in == m_hand_shake_check_is_for_files) {
						readLoginForBind();
					}
					else if (m_hand_shake_in == m_hand_shake_check_is_for_messages) {
						completeMessagesSocketValidation();
					}
					else {
						disconnect();
					}
				}
			});
	}

	void ConnectionTypeResolver::readLoginForBind() {
		asio::async_read(m_socket, asio::buffer(&m_login_length, sizeof(uint32_t)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					disconnect();
					m_on_connect_error(ec);
				}
				else {
					m_login.resize(m_login_length);
					asio::async_read(m_socket, asio::buffer(&m_login[0], m_login_length),
						[this](std::error_code ec, std::size_t length) {
							if (ec) {
								disconnect();
								m_on_connect_error(ec);
							}
							else {
								completeFilesSocketValidation();
							}
						});
				}
			});
	}

	void ConnectionTypeResolver::completeFilesSocketValidation() {
		std::cout << "Files Client Validated. bind as login: " << m_login << "\n";
		m_on_connection_resolved(std::move(m_socket), connection_type::files, m_login);
	}

	void ConnectionTypeResolver::completeMessagesSocketValidation() {
		std::cout << "Messages Client Validated." << "\n";
		m_on_connection_resolved(std::move(m_socket), connection_type::messages, std::nullopt);
	}

	void ConnectionTypeResolver::disconnect() {
		if (m_socket.is_open()) {
			asio::post(m_asio_context, [this]() { m_socket.close(); });
		}
	}

	
}