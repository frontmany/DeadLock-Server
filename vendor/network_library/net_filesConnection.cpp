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

	FilesConnection::~FilesConnection() 
	{
	}

	void FilesConnection::sendFile(const FileMetadata& file) {
		m_filesSender.sendFile(file);
	}

    void FilesConnection::close() {
        auto self = shared_from_this();

        asio::post(m_asioContext,
            [this, self]() {
                if (m_socket.is_open()) {
                    try {
                        std::error_code ec;

                        m_socket.close(ec);
                        if (ec) {
                            std::cerr << "Socket close error: " << ec.message() << "\n";
                        }
						else {
							std::cout << "files Connection closed successfully\n";
						}
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Exception in FilesConnection::close: " << e.what() << "\n";
                    }
                }
            });
    }
}