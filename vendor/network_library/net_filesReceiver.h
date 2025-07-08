#pragma once
#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"
#include "queryType.h"
#include "crypto.h"

namespace net
{
	template <typename T>
	class filesReceiver {
	public:
		filesReceiver(asio::ip::tcp::socket& socket, std::function<void(net::file<T>)> queueReceivedFile, std::function<void(std::error_code, net::file<T>)> onReceiveError, std::function<void()> disconnect)
			: m_socket(socket), m_onReceiveError(onReceiveError), m_queueReceivedFile(queueReceivedFile), m_disconnect(disconnect)
		{
			m_currentChunksCount = 0;
			m_expectedChunksCount = 0;
		}

		void startReceiving() {
			readMetadataHeader();
		}

	private:
		void readMetadataHeader() {
			asio::async_read(m_socket, asio::buffer(&m_metadataMessage.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_onReceiveError(ec, net::file<T>{});
						m_disconnect();
					}
					else {
						m_metadataMessage.body.resize(m_metadataMessage.header.size - sizeof(message_header<T>));
						readMetadataBody();
					}
				});
		}

		void readMetadataBody() {
			asio::async_read(m_socket, asio::buffer(m_metadataMessage.body.data(), m_metadataMessage.body.size()),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_onReceiveError(ec, net::file<T>{});
						m_disconnect();
					}
					else {
						parseMetadata();
						openFile();
						readChunk();
					}
				});
		}

		void readChunk() {
			asio::async_read(m_socket,
				asio::buffer(m_receiveBuffer.data(), c_receivedChunkSize),
				[this](std::error_code ec, std::size_t bytesTransferred) {
					if (ec) {
						removePartiallyDownloadedFile();
						m_onReceiveError(ec, m_file);
						m_disconnect();
						return;
					}
					else {
						m_currentChunksCount++;
						
						m_fileStream.write(m_receiveBuffer.data(), c_receivedChunkSize);
						if (m_expectedChunksCount > m_currentChunksCount) {
							readChunk();
						}
						else {
							finalizeReceiving();
						}
					}
				});
		}

		void finalizeReceiving() {
			m_fileStream.close();
			m_currentChunksCount = 0;
			m_expectedChunksCount = 0;

			m_queueReceivedFile(m_file);
			m_file = file<QueryType>();

			readMetadataHeader();
		}

		void parseMetadata() {
			std::string metadataString;
			m_metadataMessage >> metadataString;
			std::istringstream iss(metadataString);

			std::string encryptedKey;
			std::getline(iss, encryptedKey);

			std::string fileId;
			std::getline(iss, fileId);

			std::string blobUID;
			std::getline(iss, blobUID);

			std::string receiverLoginHash;
			std::getline(iss, receiverLoginHash);

			std::string senderLoginHash;
			std::getline(iss, senderLoginHash);

			std::string fileSize;
			std::getline(iss, fileSize);

			std::string fileName;
			std::getline(iss, fileName);

			std::string timestamp;
			std::getline(iss, timestamp);

			std::string caption;
			std::getline(iss, caption);

			std::string filesCountInBlob;
			std::getline(iss, filesCountInBlob);

			m_file.filePath = createFilePath(fileId);
			m_file.fileName = fileName;
			m_file.senderLoginHash = senderLoginHash;
			m_file.receiverLoginHash = receiverLoginHash;
			m_file.fileSize = fileSize;
			m_file.id = fileId;
			m_file.timestamp = timestamp;
			m_file.caption = caption;
			m_file.blobUID = blobUID;
			m_file.filesInBlobCount = filesCountInBlob;
			m_file.encryptedKey = encryptedKey;

			m_expectedChunksCount = static_cast<int>(std::ceil(static_cast<double>(std::stoi(m_file.fileSize)) / c_decryptedChunkSize));
		}

		void openFile() {
			std::error_code ec;

#ifdef _WIN32
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::wstring filePath = converter.from_bytes(m_file.filePath);
#else
			std::string filePath = m_file.filePath;
#endif
			m_fileStream.open(filePath, std::ios::binary);
			if (!m_fileStream)
				std::cerr << "Failed to create file\n";
		}

		std::string createFilePath(const std::string& fileId) {
			namespace fs = std::filesystem;
			std::string baseName = fileId;
			std::string extension = ".deadlock";

			fs::create_directory("ReceivedFiles");
			std::string filePath = "ReceivedFiles/" + baseName + extension;

			int counter = 1;
			while (fs::exists(filePath)) {
				filePath = "ReceivedFiles/" + baseName + "_" + std::to_string(counter) + extension;
				counter++;
			}

			return filePath;
		}

		void removePartiallyDownloadedFile() {
			std::string path = m_file.filePath;

			if (path.empty()) {
				return;
			}

			std::error_code ec;
			bool removed = std::filesystem::remove(path, ec);

			if (ec) {
				std::cerr << "Failed to delete " << path << ": " << ec.message() << "\n";
			}
		}

	private:
		static constexpr uint32_t c_decryptedChunkSize = 8192;
		static constexpr uint32_t c_receivedChunkSize = 8220;
		static constexpr uint32_t c_overhead = 28;

		uint32_t m_currentChunksCount;
		uint32_t m_expectedChunksCount;

		asio::ip::tcp::socket& m_socket;
		std::array<char, c_receivedChunkSize> m_receiveBuffer{};
		message<T> m_metadataMessage;
		std::ofstream m_fileStream;
		file<T>	m_file;

		std::function<void()> m_disconnect;
		std::function<void(std::error_code, net::file<T>)> m_onReceiveError;
		std::function<void(net::file<T>)> m_queueReceivedFile;
	};
}

