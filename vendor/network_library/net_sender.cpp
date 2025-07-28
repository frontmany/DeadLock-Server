#include "net_sender.h"
#include "net_connection.h"

namespace net {

	Sender::Sender(Connection* connection, asio::io_context& asioContext,
		asio::ip::tcp::socket& socket, 
		std::function<void(std::error_code, Message)> onSendMessageError)
		: m_socket(socket),
		m_relatedConnection(connection),
		m_asioContext(asioContext),
		m_onSendMessageError(onSendMessageError)
	{
	}

	void Sender::send(const Message& msg) {
		asio::post(m_asioContext, [this, msg]() {
			bool isAbleToWrite = m_safeDequeOutgoingMessages.empty();

			m_safeDequeOutgoingMessages.push_back(msg);

			if (isAbleToWrite) {
				writeHeader();
			}
		});
	}

	void Sender::writeHeader() {
		asio::async_write(
			m_socket,
			asio::buffer(&m_safeDequeOutgoingMessages.front().header,
			sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec)
				{
					m_relatedConnection->disconnect();

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
			m_socket,
			asio::buffer(m_safeDequeOutgoingMessages.front().body.data(),
				m_safeDequeOutgoingMessages.front().body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (ec) {
					m_relatedConnection->disconnect();

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

