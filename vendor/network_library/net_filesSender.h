#pragma once
#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"
#include "queryType.h"


namespace net
{
	template <typename T>
	class filesSender {
	public:
		filesSender(asio::io_context& asioContext, asio::ip::tcp::socket& socket, std::function<void(net::file<T>)> onFileSent, std::function<void(std::error_code, net::file<T>)> onSendError, std::function<void()> disconnect)
			: m_socket(socket), m_onFileSent(onFileSent), m_onSendError(onSendError), m_disconnect(disconnect), m_asioContext(asioContext)
		{
			m_totalBytesSent = 0;
		}

		void sendFile(const net::file<T>& file) {
			asio::post(m_asioContext, [this, file]() {
				bool isSendingAllowed = m_outgoingFilesQueue.empty();
				m_outgoingFilesQueue.push_back(file);
				if (isSendingAllowed) {
					m_file = m_outgoingFilesQueue.front();
					sendMetadata();
				}
			});
		}

	private:
		void sendMetadata() {
			std::ostringstream oss;
			oss << m_file.encryptedKey << '\n'
				<< m_file.id << '\n'
				<< m_file.blobUID << "\n"
				<< m_file.receiverLoginHash << '\n'
				<< m_file.senderLoginHash << '\n'
				<< m_file.fileSize << '\n'
				<< m_file.fileName << '\n'
				<< m_file.timestamp << '\n'
				<< m_file.caption << '\n'
				<< m_file.filesInBlobCount;

			m_metadataMessage.header.type = QueryType::PREPARE_TO_RECEIVE_FILE;
			m_metadataMessage << oss.str();
			m_metadataMessage.header.size = m_metadataMessage.size();

			asio::async_write(
				m_socket,
				asio::buffer(&m_metadataMessage.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec)
					{
						m_onSendError(ec, m_outgoingFilesQueue.pop_front());
						m_disconnect();
					}
					else
					{
						asio::async_write(
							m_socket,
							asio::buffer(m_metadataMessage.body.data(), m_metadataMessage.body.size()),
							[this](std::error_code ec, std::size_t length)
							{
								if (ec) {
									m_onSendError(ec, m_outgoingFilesQueue.pop_front());
								}
								else {
									sendFileChunk();
								}
							});
					}
				});
		}

		void sendFileChunk() {
			if (!m_fileStream.is_open()) {
				bool isOpen = openFile();
				if (!isOpen)
					return;
			}

			m_fileStream.read(m_readBuffer.data(), c_encryptedOutputChunkSize);
			std::streamsize bytesRead = m_fileStream.gcount();

			if (bytesRead > 0) {
				asio::async_write(
					m_socket,
					asio::buffer(m_readBuffer.data(), c_encryptedOutputChunkSize),
					[this](std::error_code ec, std::size_t length) {
						if (ec) {
							m_onSendError(ec, m_file);
							m_disconnect();
						}
						else {
							m_totalBytesSent += c_readChunkSize;
							sendFileChunk();
						}
					}
				);
			}
			else {
				m_totalBytesSent = 0;
				m_fileStream.close();
				m_metadataMessage = message<T>();
				m_file = file<T>();

				m_onFileSent(m_outgoingFilesQueue.pop_front());

				if (!m_outgoingFilesQueue.empty())
				{
					m_file = m_outgoingFilesQueue.front();
					sendMetadata();
				}
			}
		}

		bool openFile() {
			m_fileStream.open(m_file.filePath, std::ios::binary);
			if (m_fileStream) {
				return true;
			}
			else {
				return false;
			}
		}

	private:
		static constexpr uint32_t c_readChunkSize = 8192;
		static constexpr uint32_t c_encryptedOutputChunkSize = 8220;

		asio::ip::tcp::socket& m_socket;
		asio::io_context& m_asioContext;
		safe_deque<file<T>>	m_outgoingFilesQueue;

		std::array<char, c_encryptedOutputChunkSize> m_readBuffer{};
		std::ifstream m_fileStream;
		uint64_t m_totalBytesSent;
		message<T>	m_metadataMessage;
		net::file<T> m_file;


		std::function<void(const net::file<T>&)> m_onFileSent;
		std::function<void(std::error_code, net::file<T>)> m_onSendError;
		std::function<void()> m_disconnect;
	};
}