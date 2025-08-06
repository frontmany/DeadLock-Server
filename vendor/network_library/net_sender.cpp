#include "net_sender.h"
#include "net_connection.h"

namespace net {

	Sender::Sender(asio::io_context* asioContext,
		asio::ip::tcp::socket* socket, 
		std::function<void(std::error_code, Message)> onSendMessageError,
		std::function<void()> onDisconnect)
		: m_socket(socket),
		m_asioContext(asioContext),
		m_onSendMessageError(onSendMessageError),
		m_onDisconnect(onDisconnect)
	{
	}

	void Sender::send(const Message& msg) {
		asio::post(*m_asioContext, [this, msg]() {
			bool isAbleToWrite = m_safeDequeOutgoingMessages.empty();

			m_safeDequeOutgoingMessages.push_back(msg);

			if (isAbleToWrite) {
				writeHeader();
			}
		});
	}

	Sender::Sender(Sender&& other) noexcept
		: m_socket(std::exchange(other.m_socket, nullptr)),
		m_asioContext(std::exchange(other.m_asioContext, nullptr)),
		m_onSendMessageError(std::move(other.m_onSendMessageError)),
		m_onDisconnect(std::move(other.m_onDisconnect)) {
	}

	Sender& Sender::operator=(Sender&& other) noexcept {
		if (this != &other) {
			m_socket = std::exchange(other.m_socket, nullptr);
			m_asioContext = std::exchange(other.m_asioContext, nullptr);
			m_onSendMessageError = std::move(other.m_onSendMessageError);
			m_onDisconnect = std::move(other.m_onDisconnect);
		}
		return *this;
	}

	void Sender::writeHeader() {
		asio::async_write(
			*m_socket,
			asio::buffer(&m_safeDequeOutgoingMessages.front().header,
			sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec)
				{
					m_onDisconnect();

					if (ec != asio::error::connection_reset) {
						m_onSendMessageError(ec, m_safeDequeOutgoingMessages.pop_front());

					}
				}
				else
				{
					if (m_safeDequeOutgoingMessages.front().body.size() > 0)
					{
						writeBody();
					}
					else
					{
						m_safeDequeOutgoingMessages.pop_front();

						if (!m_safeDequeOutgoingMessages.empty())
						{
							writeHeader();
						}
					}
				}
			}
		);
	}

	void Sender::writeBody() {
		asio::async_write(
			*m_socket,
			asio::buffer(m_safeDequeOutgoingMessages.front().body.data(),
				m_safeDequeOutgoingMessages.front().body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (ec) {
					m_onDisconnect();

					if (ec != asio::error::connection_reset) {
						m_onSendMessageError(ec, m_safeDequeOutgoingMessages.pop_front());

					}
				}
				else {
					m_safeDequeOutgoingMessages.pop_front();

					if (!m_safeDequeOutgoingMessages.empty())
					{
						writeHeader();
					}
				}
			}
		);
	}
}

