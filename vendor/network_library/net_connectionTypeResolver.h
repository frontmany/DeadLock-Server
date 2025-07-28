#pragma once

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

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
			uint64_t id,
			std::function<void(std::error_code, uint64_t)> errorCallback,
			std::function<void(asio::ip::tcp::socket)> onConnectionResolved,
			std::function<void(asio::ip::tcp::socket socket, std::string login)> onFilesConnectionResolved
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
		void startTimeout();
		void cancelTimeout();
	private:
		asio::ip::tcp::socket	m_socket;
		asio::io_context&		m_asioContext;

		uint64_t				m_handshakeIn;
		uint64_t				m_handshakeOut;
		uint64_t				m_handshakeCheckForFiles;
		uint64_t				m_handshakeCheckForMessages;

		std::string             m_login;
		uint32_t                m_loginLength;
		uint64_t                m_id;

		asio::steady_timer m_timeoutTimer;
		std::function<void(std::error_code, uint64_t)> m_onConnectError;
		std::function<void(asio::ip::tcp::socket)> m_onConnectionResolved;
		std::function<void(asio::ip::tcp::socket, std::string)> m_onFilesConnectionResolved;
	};
}
