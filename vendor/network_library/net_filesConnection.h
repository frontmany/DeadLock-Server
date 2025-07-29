#pragma once
#include "net_common.h"
#include "net_filesSender.h"
#include "net_filesReceiver.h"   
#include <string>              


namespace net {
	class FilesConnection : public std::enable_shared_from_this<FilesConnection> {
	public:
		FilesConnection(
			asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			SafeDeque<FileMetadata>& incomingFilesQueue,
			std::function<void(std::error_code, std::optional<FileMetadata>)> onReceiveFileError,
			std::function<void(std::error_code, FileMetadata)> onSendFileError,
			std::function<void(FileMetadata)> onFileSent,
			std::function<void(std::string)> onDisconnect
		);

		~FilesConnection();

		void sendFile(const FileMetadata& file);
		void close();

	private:
		asio::ip::tcp::socket m_socket;
		FilesSender m_filesSender;
		FilesReceiver m_filesReceiver;
		asio::io_context& m_asioContext;
	};
}
