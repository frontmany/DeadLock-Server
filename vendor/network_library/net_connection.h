#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"

namespace net {

	template <typename T>
	class server_interface;

	template<typename T>
	class connection : public std::enable_shared_from_this<connection<T>> {
	public:

		enum class owner {
			server,
			client
		};

		connection(owner owner, asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			safe_deque<owned_message<T>>& safeDequeIncomingMessages)
			: m_asio_context(asioContext),
			m_socket(std::move(socket)),
			m_safe_deque_incoming_messages(safeDequeIncomingMessages)
		{
			m_last_packet_size = 0;
			m_hand_shake_out = 0;
			m_hand_shake_in = 0;
			m_hand_shake_check = 0;
			m_safe_deque_incoming_files = nullptr;
			m_received_file_size = 0;
			m_curent_number_of_occurrences = 0;
			m_number_of_full_occurrences = 0;
			m_is_for_files = false;
			m_owner_type = owner;

			if (m_owner_type == owner::server) {
				m_hand_shake_out = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

				m_hand_shake_check = scramble(m_hand_shake_out);
			}
		}

		connection(owner owner, asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			safe_deque<owned_message<T>>& safeDequeIncomingMessages,
			safe_deque<owned_file<T>>* safeDequeIncomingFiles)
			: m_asio_context(asioContext),
			m_socket(std::move(socket)),
			m_safe_deque_incoming_messages(safeDequeIncomingMessages),
			m_safe_deque_incoming_files(safeDequeIncomingFiles)
		{
			m_last_packet_size = 0;
			m_hand_shake_out = 0;
			m_hand_shake_in = 0;
			m_hand_shake_check = 0;
			m_safe_deque_incoming_files = nullptr;
			m_received_file_size = 0;
			m_curent_number_of_occurrences = 0;
			m_number_of_full_occurrences = 0;
			m_is_for_files = true;
			m_owner_type = owner;

			if (m_owner_type == owner::server) {
				m_hand_shake_out = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

				m_hand_shake_check = scramble(m_hand_shake_out);
			}
		}

		virtual ~connection() {}

		// new
		void setOnFileSent(std::function<void(net::file<T>)> callback) {
			m_on_file_sent = std::move(callback);
		}

		// errors
		void setOnSendMessageError(std::function<void(std::error_code, net::message<T>)> callback) {
			m_on_send_message_error = std::move(callback);
		}

		void setOnSendFileChunkError(std::function<void(std::error_code, net::file<T>)> callback) {
			m_on_send_file_chunk_error = std::move(callback);
		}

		void setOnReadMessageError(std::function<void(std::error_code)> callback) {
			m_on_read_message_error = std::move(callback);
		}

		void setOnReadFileChunkError(std::function<void(std::error_code, net::file<T>)> callback) {
			m_on_read_file_chunk_error = std::move(callback);
		}

		void setOnConnectError(std::function<void(std::error_code)> callback) {
			m_on_connect_error = std::move(callback);
		}


		bool removePartiallyDownloadedFile() {
			std::string path = m_file_tmp.filePath;

			if (path.empty()) {
				return false;
			}

			std::error_code ec;
			bool removed = std::filesystem::remove(path, ec);

			if (ec) {
				std::cerr << "Failed to delete " << path << ": " << ec.message() << "\n";
				return false;
			}

			return removed;
		}


		void connectToServer(const asio::ip::tcp::resolver::results_type& endpoint) {
			if (m_owner_type == owner::client) {
				asio::async_connect(m_socket, endpoint,
					[this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
						if (ec) {
							disconnect();
							if (m_on_connect_error) {
								m_on_connect_error(ec);
							}
							std::cerr << "Connection failed: " << ec.message() << std::endl;
						}
						else {
							std::cout << "Connected to: " << endpoint << std::endl;
							readValidation();
						}
					});
			}
		}

		void redefineAsFileConnection(safe_deque<owned_file<T>>* safeDequeIncomingFiles) {
			m_safe_deque_incoming_files = safeDequeIncomingFiles;
			m_is_for_files = true;
		}

		void connectToClient() {
			if (m_owner_type == owner::server) {
				writeValidation();
				readValidation();
			}
		}

		void disconnect() {
			if (m_receive_file_stream.is_open()) {
				m_receive_file_stream.close();
			}
			if (m_send_file_stream.is_open()) {
				m_send_file_stream.close();
			}

			if (isConnected()) {
				asio::post(m_asio_context, [this]() { m_socket.close(); });
			}
		}

		bool isConnected() const {
			return m_socket.is_open();
		}

		void send(const message<T>& msg) {
			asio::post(m_asio_context, [this, msg]() {
				bool isAbleToWrite = m_safe_deque_outgoing_messages.empty();
				m_safe_deque_outgoing_messages.push_back(msg);
				if (isAbleToWrite) {
					writeHeader();
				}
				});
		}

		void readFile() {
			std::error_code ec;
			std::string originalPath = m_file_tmp.filePath;
			std::string newPath = originalPath;
			int counter = 1;

			auto make_new_path = [&]() {
				size_t dotPos = originalPath.find_last_of('.');
				if (dotPos != std::string::npos && dotPos > 0) {
					return originalPath.substr(0, dotPos) +
						"(" + std::to_string(counter) + ")" +
						originalPath.substr(dotPos);
				}
				return originalPath + "(" + std::to_string(counter) + ")";
				};

			while (std::filesystem::exists(newPath, ec) && !ec) {
				newPath = make_new_path();
				counter++;

				if (counter > 1000) {
					std::cerr << "Cannot find available filename for: " << originalPath << "\n";
					break;
				}
			}

			if (ec) {
				std::cerr << "Filesystem error: " << ec.message() << "\n";
				return;
			}

			m_file_tmp.filePath = newPath;

			m_receive_file_stream.open(newPath, std::ios::binary | std::ios::app);
			if (!m_receive_file_stream) {
				std::cerr << "Failed to create file: " << newPath << "\n";
				return;
			}

			readFileChunk();
		}

		void readHeader() {
			asio::async_read(m_socket, asio::buffer(&m_message_tmp.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_on_read_message_error(ec);
						disconnect();
					}
					else {
						if (m_message_tmp.header.size > sizeof(message_header<T>)) {
							m_message_tmp.body.resize(m_message_tmp.header.size - sizeof(message_header<T>));
							readBody();
						}
						else
							addToIncomingMessagesQueue();
					}
				});
		}

		void supplyFileData(std::string myLogin, std::string friendLogin, std::string filePath, std::string fileId, uint32_t fileSize, std::string timestamp, std::string caption, const std::string& blobUID, size_t filesInBlobCount) {
			m_file_tmp.filePath = filePath;
			m_file_tmp.senderLogin = myLogin;
			m_file_tmp.receiverLogin = friendLogin;
			m_file_tmp.fileSize = fileSize;
			m_file_tmp.id = fileId;
			m_file_tmp.timestamp = timestamp;
			m_file_tmp.caption = caption;
			m_file_tmp.blobUID = blobUID;
			m_file_tmp.filesInBlobCount = filesInBlobCount;


			m_number_of_full_occurrences = m_file_tmp.fileSize / m_receive_file_buffer.size();
			int lastPacketSize = m_file_tmp.fileSize - (m_number_of_full_occurrences * m_receive_file_buffer.size());
			if (lastPacketSize == 0) {
				m_last_packet_size = m_receive_file_buffer.size();
			}
			else {
				m_last_packet_size = lastPacketSize;
			}
		}

		void sendFile(const net::file<T>& file) {
			if (m_is_for_files) {
				asio::post(m_asio_context, [this, file]() {
					bool isAbleToWrite = m_safe_deque_outgoing_files.empty() && m_safe_deque_outgoing_messages.empty();
					m_safe_deque_outgoing_files.push_back(file);
					if (isAbleToWrite) {
						writeFileChunk();
					}
					});
			}
		}

		void writeFileChunk() {
			if (!m_send_file_stream.is_open()) {
				m_send_file_stream.open(m_safe_deque_outgoing_files.front().filePath, std::ios::binary);
				if (!m_send_file_stream.is_open()) {
					if (m_on_send_file_chunk_error) {
						m_on_send_file_chunk_error(
							std::make_error_code(std::errc::no_such_file_or_directory),
							m_safe_deque_outgoing_files.front()
						);
					}
					return;
				}
			}

			m_send_file_stream.read(m_send_file_buffer.data(), m_send_file_buffer.size());
			std::streamsize bytesRead = m_send_file_stream.gcount();

			if (bytesRead > 0) {
				asio::async_write(
					m_socket,
					asio::buffer(m_send_file_buffer.data(), bytesRead),
					[this](std::error_code ec, std::size_t) {
						if (ec) {
							if (m_on_send_file_chunk_error) {
								m_on_send_file_chunk_error(ec, m_safe_deque_outgoing_files.front());
							}
							disconnect();
						}
						else {
							writeFileChunk();
						}
					}
				);
			}
			else {
				m_send_file_stream.close();
				m_safe_deque_outgoing_files.pop_front();
			}
		}

		bool isFileConnection() {
			return m_is_for_files;
		}

	private:
		void readFileChunk() {
			m_socket.async_read_some(
				asio::buffer(m_receive_file_buffer.data(), m_receive_file_buffer.size()),
				[this](std::error_code ec, std::size_t bytes_transferred) {
					if (ec) {
						disconnect();
						removePartiallyDownloadedFile();
						if (m_on_read_file_chunk_error) {
							m_on_read_file_chunk_error(ec, m_file_tmp);
						}
						return;
					}
					else {
						m_received_file_size += bytes_transferred;
						m_curent_number_of_occurrences++;

						if (m_file_tmp.fileSize >= m_receive_file_buffer.size()) {
							if (m_curent_number_of_occurrences > m_number_of_full_occurrences) {
								m_receive_file_stream.write(m_receive_file_buffer.data(), m_last_packet_size);
							}
							else {
								m_receive_file_stream.write(m_receive_file_buffer.data(), m_receive_file_buffer.size());
							}
						}
						else {
							m_receive_file_stream.write(m_receive_file_buffer.data(), m_last_packet_size);
						}

						if (m_received_file_size < m_file_tmp.fileSize) {
							readFileChunk();
						}
						else {
							finalizeFileTransfer();
						}
					}

				});
		}

		void finalizeFileTransfer() {
			m_receive_file_stream.close();
			if (m_owner_type == owner::server)
				m_safe_deque_incoming_files->push_back({ this->shared_from_this(), m_file_tmp });
			else
				m_safe_deque_incoming_files->push_back({ nullptr, m_file_tmp });

			m_on_file_sent(std::move(m_file_tmp));

			m_received_file_size = 0;
			m_curent_number_of_occurrences = 0;
			m_number_of_full_occurrences = 0;
			m_file_tmp = file<T>();

			readHeader();
		}

		void readBody() {
			asio::async_read(m_socket, asio::buffer(m_message_tmp.body.data(), m_message_tmp.body.size()),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_on_read_message_error(ec);
						disconnect();
					}
					else {
						addToIncomingMessagesQueue();
					}
				});
		}

		void addToIncomingMessagesQueue() {
			if (m_owner_type == owner::server)
				m_safe_deque_incoming_messages.push_back({ this->shared_from_this(), m_message_tmp });
			else
				m_safe_deque_incoming_messages.push_back({ nullptr, m_message_tmp });

			m_message_tmp = message<T>();

			if (!m_is_for_files) {
				readHeader();
			}
		}

		void writeHeader() {
			asio::async_write(
				m_socket,
				asio::buffer(&m_safe_deque_outgoing_messages.front().header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec)
					{
						disconnect();
						m_on_send_message_error(ec, m_safe_deque_outgoing_messages.pop_front());
					}
					else
					{
						if (m_safe_deque_outgoing_messages.front().body.size() > 0)
						{
							writeBody();
						}
						else
						{
							m_safe_deque_outgoing_messages.pop_front();

							if (!m_safe_deque_outgoing_messages.empty())
							{
								writeHeader();
							}
							else if (!m_safe_deque_outgoing_files.empty())
							{
								writeFileChunk();
							}
						}
					}

				}
			);
		}

		void writeBody() {
			asio::async_write(
				m_socket,
				asio::buffer(m_safe_deque_outgoing_messages.front().body.data(),
					m_safe_deque_outgoing_messages.front().body.size()),
				[this](std::error_code ec, std::size_t length)
				{
					if (ec) {
						disconnect();
						m_on_send_message_error(ec, m_safe_deque_outgoing_messages.pop_front());
					}
					else {
						m_safe_deque_outgoing_messages.pop_front();

						if (!m_safe_deque_outgoing_messages.empty())
						{
							writeHeader();
						}
						else if (!m_safe_deque_outgoing_files.empty())
						{
							writeFileChunk();
						}
					}
				}
			);
		}

		uint64_t scramble(uint64_t inputNumber) {
			uint64_t out = inputNumber ^ 0xDEADBEEFC;
			out = (out & 0xF0F0F0F0F) >> 4 | (out & 0x0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}

		void writeValidation() {
			asio::async_write(m_socket, asio::buffer(&m_hand_shake_out, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
					}
					else {
						if (m_owner_type == owner::client)
							readHeader();
					}
				});
		}

		void readValidation() {
			asio::async_read(m_socket, asio::buffer(&m_hand_shake_in, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						disconnect();
						m_on_connect_error(ec);
					}
					else {
						if (m_owner_type == owner::client) {
							m_hand_shake_out = scramble(m_hand_shake_in);
							writeValidation();
						}
						else {
							if (m_hand_shake_in == m_hand_shake_check) {
								std::cout << "Client Validated\n";
								readHeader();
							}
							else {
								disconnect();
							}
						}
					}
				});
		}

	protected:
		bool						  m_is_for_files;
		uint32_t					  m_received_file_size;
		uint32_t					  m_last_packet_size;
		uint32_t					  m_number_of_full_occurrences;
		uint32_t					  m_curent_number_of_occurrences;

		std::array<char, 8192>		  m_send_file_buffer{};
		std::ifstream				  m_send_file_stream;

		std::array<char, 8192>		  m_receive_file_buffer{};
		std::ofstream				  m_receive_file_stream;

		owner						  m_owner_type;
		asio::ip::tcp::socket		  m_socket;
		asio::io_context& m_asio_context;

		safe_deque<message<T>>		  m_safe_deque_outgoing_messages;
		safe_deque<owned_message<T>>& m_safe_deque_incoming_messages;
		message<T>					  m_message_tmp;

		safe_deque<file<T>>			  m_safe_deque_outgoing_files;
		safe_deque<owned_file<T>>* m_safe_deque_incoming_files;
		file<T>						  m_file_tmp;

		uint64_t					  m_hand_shake_out;
		uint64_t					  m_hand_shake_in;
		uint64_t					  m_hand_shake_check;

		// callbacks
		std::function<void(net::file<T>)> m_on_file_sent;

		// errors
		std::function<void(std::error_code, net::message<T>)> m_on_send_message_error;
		std::function<void(std::error_code, net::file<T>)> m_on_send_file_chunk_error;

		std::function<void(std::error_code)> m_on_read_message_error;
		std::function<void(std::error_code, net::file<T>)> m_on_read_file_chunk_error;

		std::function<void(std::error_code)> m_on_connect_error;
	};
}
