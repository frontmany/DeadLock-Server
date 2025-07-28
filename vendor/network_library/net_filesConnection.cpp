#include "net_filesConnection.h"


namespace net {
	FilesConnection::FilesConnection(
		asio::io_context& asioContext,
		asio::ip::tcp::socket socket,
		SafeDeque<FileMetadata>& incomingFilesQueue,
		std::function<void(std::error_code, std::optional<FileMetadata>)> onReceiveFileError,
		std::function<void(std::error_code, FileMetadata)> onSendFileError,
		std::function<void(FileMetadata)> onFileSent,
		std::function<void(std::string)> onDisconnect)
		: m_asioContext(asioContext),
		m_socket(std::move(socket)),
		m_filesSender(this, asioContext, m_socket, onFileSent, onSendFileError),
		m_filesReceiver(this, m_socket, incomingFilesQueue, onReceiveFileError)
	{
		m_filesReceiver.startReceiving();
	}

	FilesConnection::~FilesConnection() {}

	void FilesConnection::sendFile(const FileMetadata& file) {
		m_filesSender.sendFile(file);
	}

	void FilesConnection::disconnect() {
		if (m_socket.is_open()) {
			m_socket.close();
		}

		m_onDisconnect(m_ownerLoginHash);
	}

	void FilesConnection::setOwnerLoginHash(const std::string& ownerLoginHash) {
		m_ownerLoginHash = ownerLoginHash;
	}

	const std::string& FilesConnection::getOwnerLoginHash() {
		return m_ownerLoginHash;
	}
}