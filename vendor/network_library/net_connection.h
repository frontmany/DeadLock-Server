#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"

#include <chrono>
#include <thread>

#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>
#include <locale>
#include <codecvt>

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

		void setOnReadMessageError(std::function<void(std::shared_ptr<net::connection<T>>, std::error_code)> callback) {
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

#ifdef _WIN32
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::wstring originalPath = converter.from_bytes(m_file_tmp.filePath);
			std::wstring newPath = originalPath;
#else
			std::string originalPath = m_file_tmp.filePath;
			std::string newPath = originalPath;
#endif

			int counter = 1;

			auto make_new_path = [&]() {
#ifdef _WIN32
				size_t dotPos = originalPath.find_last_of(L'.');
				if (dotPos != std::wstring::npos && dotPos > 0) {
					return originalPath.substr(0, dotPos) +
						L"(" + std::to_wstring(counter) + L")" +
						originalPath.substr(dotPos);
				}
				return originalPath + L"(" + std::to_wstring(counter) + L")";
#else
				size_t dotPos = originalPath.find_last_of('.');
				if (dotPos != std::string::npos && dotPos > 0) {
					return originalPath.substr(0, dotPos) +
						"(" + std::to_string(counter) + ")" +
						originalPath.substr(dotPos);
				}
				return originalPath + "(" + std::to_string(counter) + ")";
#endif
				};

			while (std::filesystem::exists(newPath, ec) && !ec) {
				newPath = make_new_path();
				counter++;

				if (counter > 1000) {
#ifdef _WIN32
					std::wcerr << L"Cannot find available filename for: " << originalPath << L"\n";
#else
					std::cerr << "Cannot find available filename for: " << originalPath << "\n";
#endif
					break;
				}
			}

			if (ec) {
				std::cerr << "Filesystem error: " << ec.message() << "\n";
				return;
			}

#ifdef _WIN32
			m_file_tmp.filePath = converter.to_bytes(newPath);
#else
			m_file_tmp.filePath = newPath;
#endif

#ifdef _WIN32
			m_receive_file_stream.open(newPath, std::ios::binary);
#else
			m_receive_file_stream.open(newPath, std::ios::binary);
#endif

			if (!m_receive_file_stream.is_open()) {
#ifdef _WIN32
				std::wcerr << L"Failed to create file: " << newPath << L"\n";
#else
				std::cerr << "Failed to create file: " << newPath << "\n";
#endif
				return;
			}
			else {
#ifdef _WIN32
				std::wcout << L"File created: " << newPath << L"\n";
#else
				std::cout << "File created: " << newPath << "\n";
#endif
			}

			readFileChunk();
		}

		void readHeader() {
			asio::async_read(m_socket, asio::buffer(&m_message_tmp.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length) {
					if (ec) {
						m_on_read_message_error(this->shared_from_this(), ec);
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

		void supplyFileData(std::string myLogin, std::string friendLogin, std::string filePath, std::string fileName, std::string fileId, uint32_t fileSize, std::string timestamp, std::string caption, const std::string& blobUID, size_t filesInBlobCount) {
			m_file_tmp.filePath = filePath;
			m_file_tmp.fileName = fileName;
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
				const auto& file = m_safe_deque_outgoing_files.front();

#ifdef _WIN32
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
				std::wstring widePath = converter.from_bytes(file.filePath);
				std::filesystem::path fsPath(widePath);
#else
				std::filesystem::path fsPath(file.filePath);
#endif

				m_send_file_stream.open(fsPath, std::ios::binary);

				if (!m_send_file_stream.is_open()) {
					std::error_code ec;
					bool exists = std::filesystem::exists(fsPath, ec);

					if (m_on_send_file_chunk_error) {
						if (ec) {
							m_on_send_file_chunk_error(ec, file);
						}
						else if (!exists) {
							m_on_send_file_chunk_error(
								std::make_error_code(std::errc::no_such_file_or_directory),
								file
							);
						}
						else {
							m_on_send_file_chunk_error(
								std::make_error_code(std::errc::permission_denied),
								file
							);
						}
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
							m_current_bytes_sent += m_send_file_buffer.size();
							if (m_safe_deque_outgoing_files.front().fileSize <= m_current_bytes_sent) {
								m_current_bytes_sent = 0;
							}

							writeFileChunk();

						}
					}
				);
			}
			else {
				m_send_file_stream.close();
				m_on_file_sent(m_safe_deque_outgoing_files.pop_front());
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
						m_on_read_message_error(this->shared_from_this(), ec);
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

		uint32_t m_current_bytes_sent = 0;
		// callbacks
		std::function<void(net::file<T>)> m_on_file_sent;

		// errors
		std::function<void(std::error_code, net::message<T>)> m_on_send_message_error;
		std::function<void(std::error_code, net::file<T>)> m_on_send_file_chunk_error;

		std::function<void(std::shared_ptr<net::connection<T>>, std::error_code)> m_on_read_message_error;
		std::function<void(std::error_code, net::file<T>)> m_on_read_file_chunk_error;

		std::function<void(std::error_code)> m_on_connect_error;
	};
}
