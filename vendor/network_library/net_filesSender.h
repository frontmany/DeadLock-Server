#pragma once
#include "net_common.h"
#include "net_safeDeque.h"
#include "net_message.h"
#include "net_fileMetadata.h"
#include "queryType.h"


namespace net
{
	class FilesConnection;

	class FilesSender {
	public:
		FilesSender(FilesConnection* filesConnection,
			asio::io_context& asioContext,
			asio::ip::tcp::socket& socket,
			std::function<void(FileMetadata)> onFileSent,
			std::function<void(std::error_code, FileMetadata)> onSendError
		);

		void sendFile(const FileMetadata& file);

	private:
		void sendMetadata();
		void sendFileChunk();
		bool openFile();

	private:
		static constexpr uint32_t c_readChunkSize = 8192;
		static constexpr uint32_t c_encryptedOutputChunkSize = 8220;

		FilesConnection* m_relatedFilesConnection;
		asio::ip::tcp::socket& m_socket;
		asio::io_context& m_asioContext;
		SafeDeque<FileMetadata> m_outgoingFilesQueue;

		std::array<char, c_encryptedOutputChunkSize> m_readBuffer{};
		std::ifstream m_fileStream;
		uint64_t m_totalBytesSent;
		Message	m_metadataMessage;
		FileMetadata m_fileMetadata;


		std::function<void(const FileMetadata)> m_onFileSent;
		std::function<void(std::error_code, FileMetadata)> m_onSendError;
	};
}