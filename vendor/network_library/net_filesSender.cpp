#include "net_filesSender.h"
#include "net_filesConnection.h"


namespace net {
	FilesSender::FilesSender(FilesConnection* filesConnection, 
		asio::io_context& asioContext,
		asio::ip::tcp::socket& socket,
		std::function<void(FileMetadata)> onFileSent,
		std::function<void(std::error_code, FileMetadata)> onSendError)
		: m_socket(socket),
		m_relatedFilesConnection(filesConnection),
		m_onFileSent(onFileSent),
		m_onSendError(onSendError),
		m_asioContext(asioContext)
	{
		m_totalBytesSent = 0;
	}

	void FilesSender::sendFile(const FileMetadata& file) {
		asio::post(m_asioContext, [this, file]() {
			bool isSendingAllowed = m_outgoingFilesQueue.empty();
			m_outgoingFilesQueue.push_back(file);
			if (isSendingAllowed) {
				m_fileMetadata = m_outgoingFilesQueue.front();
				sendMetadata();
			}
		});
	}

	void FilesSender::sendMetadata() {
		std::ostringstream oss;
		oss << m_fileMetadata.encryptedKey << '\n'
			<< m_fileMetadata.id << '\n'
			<< m_fileMetadata.blobUID << "\n"
			<< m_fileMetadata.receiverLoginHash << '\n'
			<< m_fileMetadata.senderLoginHash << '\n'
			<< m_fileMetadata.fileSize << '\n'
			<< m_fileMetadata.fileName << '\n'
			<< m_fileMetadata.timestamp << '\n'
			<< m_fileMetadata.filesInBlobCount << '\n'
			<< m_fileMetadata.caption;

		m_metadataMessage.header.type = QueryType::PREPARE_TO_RECEIVE_FILE;
		m_metadataMessage << oss.str();
		m_metadataMessage.header.size = m_metadataMessage.size();

		asio::async_write(
			m_socket,
			asio::buffer(&m_metadataMessage.header, sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec)
				{
					m_relatedFilesConnection->disconnect();
					if (ec != asio::error::connection_reset) {
						m_onSendError(ec, m_outgoingFilesQueue.pop_front());
					}
				}
				else
				{
					asio::async_write(
						m_socket,
						asio::buffer(m_metadataMessage.body.data(), m_metadataMessage.body.size()),
						[this](std::error_code ec, std::size_t length)
						{
							if (ec) {
								m_relatedFilesConnection->disconnect();
								if (ec != asio::error::connection_reset) {
									m_onSendError(ec, m_outgoingFilesQueue.pop_front());
								}
							}
							else {
								sendFileChunk();
							}
						});
				}
			});
	}

	void FilesSender::sendFileChunk() {
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
						m_fileStream.close();
						m_totalBytesSent = 0;
						
						m_relatedFilesConnection->disconnect();
						if (ec != asio::error::connection_reset) {
							m_onSendError(ec, m_outgoingFilesQueue.pop_front());
						}
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
			m_metadataMessage = Message();
			m_fileMetadata = FileMetadata();

			m_onFileSent(m_outgoingFilesQueue.pop_front());

			if (!m_outgoingFilesQueue.empty())
			{
				m_fileMetadata = m_outgoingFilesQueue.front();
				sendMetadata();
			}
		}
	}

	bool FilesSender::openFile() {
		m_fileStream.open(m_fileMetadata.filePath, std::ios::binary);
		if (m_fileStream) {
			return true;
		}
		else {
			return false;
		}
	}
}

