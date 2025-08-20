#pragma once
#include "net_common.h"
#include "net_safeDeque.h"
#include "net_message.h"
#include "net_fileMetadata.h"
#include "queryType.h"
#include "crypto.h"

namespace net
{
	class FilesConnection;

	class FilesReceiver {
	public:
		FilesReceiver(FilesConnection* filesConnection,
			asio::ip::tcp::socket& socket,
			SafeDeque<FileMetadata>& incomingFilesQueue,
			std::function<void(std::error_code, std::optional<FileMetadata>)> onReceiveError
		);

		void startReceiving();

	private:
		std::string createFilePath(const std::string& fileId);
		std::string createAvatarFilePath(const std::string& userLoginHash);
		void removePartiallyDownloadedFile();

		void readMetadataHeader();
		void readMetadataBody();
		void readChunk();

		void parseMetadata();
		void parseAvatarMetadata();
		void finalizeReceiving();
		void openFile();

	private:
		static constexpr uint32_t c_decryptedChunkSize = 8192;
		static constexpr uint32_t c_receivedChunkSize = 8220;
		static constexpr uint32_t c_overhead = 28;

		uint32_t m_lastChunkSize;
		uint32_t m_currentChunksCount;
		uint32_t m_expectedChunksCount;

		SafeDeque<FileMetadata>& m_incomingFilesQueue;
		FilesConnection* m_relatedFilesConnection;
		asio::ip::tcp::socket& m_socket;
		std::array<char, c_receivedChunkSize> m_receiveBuffer{};
		Message m_metadataMessageToReceive;
		FileMetadata m_fileMetadataToHold;
		std::ofstream m_fileStream;

		std::function<void(std::error_code, std::optional<FileMetadata>)> m_onReceiveError;
	};
}

