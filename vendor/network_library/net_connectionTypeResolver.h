#pragma once
#include "net_common.h"
#include "net_safeDeque.h"
#include "net_message.h"
#include "net_fileMetadata.h"

namespace net {
	class ConnectionTypeResolver {
	public:
		ConnectionTypeResolver(
			asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			std::function<void(std::error_code)> errorCallback,
			std::function<void(asio::ip::tcp::socket socket, connection_type type, std::optional<std::string> login)> onConnectionResolved
		);

		~ConnectionTypeResolver();

		uint64_t scramble(uint64_t inputNumber);
		void writeValidation();
		void readValidation();
		void readLoginForBind();

	private:
		void completeFilesSocketValidation();
		void completeMessagesSocketValidation();
		void disconnect();

	private:
		asio::ip::tcp::socket	m_socket;
		asio::io_context&		m_asioContext;

		uint64_t				m_handshakeIn;
		uint64_t				m_handshakeOut;
		uint64_t				m_handshakeCheckForFiles;
		uint64_t				m_handshakeCheckForMessages;

		std::string             m_login;
		uint32_t                m_login_length;

		std::function<void(std::error_code)> m_on_connect_error;
		std::function<void(asio::ip::tcp::socket socket, connection_type type, std::optional<std::string> login)> m_on_connection_resolved;
	};
}
