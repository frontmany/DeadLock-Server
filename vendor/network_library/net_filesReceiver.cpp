#include "net_filesReceiver.h"
#include "net_filesConnection.h"

namespace net {
	FilesReceiver::FilesReceiver(FilesConnection* filesConnection, 
		asio::ip::tcp::socket& socket,
		SafeDeque<FileMetadata>& incomingFilesQueue,
		std::function<void(std::error_code, std::optional<FileMetadata>)> onReceiveError)
		: m_socket(socket),
		m_incomingFilesQueue(incomingFilesQueue),
		m_relatedFilesConnection(filesConnection),
		m_onReceiveError(onReceiveError)
	{
		m_currentChunksCount = 0;
		m_expectedChunksCount = 0;
	}

	void FilesReceiver::startReceiving() {
		readMetadataHeader();
	}

	void FilesReceiver::readMetadataHeader() {
		asio::async_read(m_socket, asio::buffer(&m_metadataMessageToReceive.header, sizeof(MessageHeader)),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					if (ec != asio::error::connection_reset) {
						m_onReceiveError(ec, std::nullopt);
					}
				}
				else {
					m_metadataMessageToReceive.body.resize(m_metadataMessageToReceive.header.size - sizeof(MessageHeader));
					readMetadataBody();
				}
			});
	}

	void FilesReceiver::readMetadataBody() {
		asio::async_read(m_socket, asio::buffer(m_metadataMessageToReceive.body.data(), m_metadataMessageToReceive.body.size()),
			[this](std::error_code ec, std::size_t length) {
				if (ec) {
					if (ec != asio::error::connection_reset) {
						m_onReceiveError(ec, std::nullopt);
					}
				}
				else {
					if (m_metadataMessageToReceive.header.type == QueryType::UPDATE_MY_AVATAR) {
						parseAvatarMetadata();
					}
					else {
						parseMetadata();
					}
					openFile();
					readChunk();
				}
			});
	}

	void FilesReceiver::readChunk() {
		asio::async_read(m_socket,
			asio::buffer(m_receiveBuffer.data(), c_receivedChunkSize),
			[this](std::error_code ec, std::size_t bytesTransferred) {
				if (ec) {
					removePartiallyDownloadedFile();

					if (ec != asio::error::connection_reset) {
						m_onReceiveError(ec, m_fileMetadataToHold);
					}
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

	void FilesReceiver::finalizeReceiving() {
		m_fileStream.close();
		m_currentChunksCount = 0;
		m_expectedChunksCount = 0;

		m_incomingFilesQueue.push_back(m_fileMetadataToHold);
		m_fileMetadataToHold = FileMetadata{};

		readMetadataHeader();
	}

	void FilesReceiver::parseAvatarMetadata() {
		std::string metadataString;
		m_metadataMessageToReceive >> metadataString;
		std::istringstream iss(metadataString);

		std::string senderLoginHash;
		std::getline(iss, senderLoginHash);

		std::string fileSize;
		std::getline(iss, fileSize);

		std::string line;
		while (std::getline(iss, line)) {
			if (line == "VEC_END") {
				break;
			}
			else {
				m_fileMetadataToHold.ifFileIsAvatarLoginHashesVec.push_back(line);
			}
		}

		m_fileMetadataToHold.isAvatar = true;
		m_fileMetadataToHold.filePath = createAvatarFilePath(senderLoginHash);
		m_fileMetadataToHold.senderLoginHash = senderLoginHash;
		m_fileMetadataToHold.fileSize = fileSize;
	}

	void FilesReceiver::parseMetadata() {
		std::string metadataString;
		m_metadataMessageToReceive >> metadataString;
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

		std::string filesCountInBlob;
		std::getline(iss, filesCountInBlob);

		std::string caption;
		std::getline(iss, caption);

		m_fileMetadataToHold.filePath = createFilePath(fileId);
		m_fileMetadataToHold.fileName = fileName;
		m_fileMetadataToHold.senderLoginHash = senderLoginHash;
		m_fileMetadataToHold.receiverLoginHash = receiverLoginHash;
		m_fileMetadataToHold.fileSize = fileSize;
		m_fileMetadataToHold.id = fileId;
		m_fileMetadataToHold.timestamp = timestamp;
		m_fileMetadataToHold.caption = caption;
		m_fileMetadataToHold.blobUID = blobUID;
		m_fileMetadataToHold.filesInBlobCount = filesCountInBlob;
		m_fileMetadataToHold.encryptedKey = encryptedKey;

		m_expectedChunksCount = static_cast<int>(std::ceil(static_cast<double>(std::stoi(m_fileMetadataToHold.fileSize)) / c_decryptedChunkSize));
	}

	void FilesReceiver::openFile() {
		std::error_code ec;

#ifdef _WIN32
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::wstring filePath = converter.from_bytes(m_fileMetadataToHold.filePath);
#else
		std::string filePath = m_fileMetadataToHold.filePath;
#endif
		m_fileStream.open(filePath, std::ios::binary);
		if (!m_fileStream)
			std::cerr << "Failed to create file\n";
	}

	std::string FilesReceiver::createFilePath(const std::string& fileId) {
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

	std::string FilesReceiver::createAvatarFilePath(const std::string& userLoginHash) {
		namespace fs = std::filesystem;
		std::string baseName = userLoginHash;
		std::string extension = ".dph";

		fs::create_directory("ReceivedPhotos");
		std::string filePath = "ReceivedPhotos/" + baseName + extension;

		return filePath;
	}

	void FilesReceiver::removePartiallyDownloadedFile() {
		std::string path = m_fileMetadataToHold.filePath;

		if (path.empty()) {
			return;
		}

		m_fileStream.close();
		m_currentChunksCount = 0;
		m_expectedChunksCount = 0;

		std::error_code ec;
		bool removed = std::filesystem::remove(path, ec);

		if (ec) {
			std::cerr << "Failed to delete " << path << ": " << ec.message() << "\n";
		}
	}
}

