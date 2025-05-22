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
			safe_deque<owned_message<T>>& safeDequeIncomingMessages,
			std::function<void(std::shared_ptr<connection<T>>)> onDisconnectCallback)
			: m_asio_context(asioContext), 
			m_socket(std::move(socket)), 
			m_safe_deque_incoming_messages(safeDequeIncomingMessages),
			m_on_disconnect_callback(onDisconnectCallback)
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
			safe_deque<owned_file<T>>* safeDequeIncomingFiles,
			std::function<void(std::shared_ptr<connection<T>>)> onDisconnectCallback)
			: m_asio_context(asioContext),
			m_socket(std::move(socket)),
			m_safe_deque_incoming_messages(safeDequeIncomingMessages),
			m_safe_deque_incoming_files(safeDequeIncomingFiles),
			m_on_disconnect_callback(onDisconnectCallback)
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

		void connectToServer(const asio::ip::tcp::resolver::results_type& endpoint) {
			if (m_owner_type == owner::client) {
				asio::async_connect(m_socket, endpoint,
					[this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
						if (!ec) {
							std::cout << "Connected to: " << endpoint << std::endl;
							readValidation();
						}
						else {
							disconnect();
							m_on_disconnect_callback(this->shared_from_this());
							std::cerr << "Connection failed: " << ec.message() << std::endl;
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

		void send(const message<T>& msg, std::function<void(message<T>)> errorCallbackSendMessage, std::function<void(net::file<T>)> errorCallbackSendFile) {
			asio::post(m_asio_context, [this, msg, callBackMessage = std::move(errorCallbackSendMessage), callBackFile = std::move(errorCallbackSendFile)]() {
			 	bool isAbleToWrite = m_safe_deque_outgoing_messages.empty();
				m_safe_deque_outgoing_messages.push_back(msg);
				if (isAbleToWrite) {
					writeHeader(std::move(callBackMessage), std::move(callBackFile));
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
					if (!ec) {
						if (m_message_tmp.header.size > sizeof(message_header<T>)) {
							m_message_tmp.body.resize(m_message_tmp.header.size - sizeof(message_header<T>));
							readBody();
						}
						else
							addToIncomingMessagesQueue();
					}
					else {
						disconnect();
						m_on_disconnect_callback(this->shared_from_this());
					}
				});
		}

		void supplyFileData(std::string senderLogin, std::string receiverLogin, std::string filePath, uint32_t fileSize, std::string fileId) {
			m_file_tmp.filePath = filePath;
			m_file_tmp.senderLogin = senderLogin;
			m_file_tmp.receiverLogin = receiverLogin;
			m_file_tmp.fileSize = fileSize;
			m_file_tmp.id = fileId;

			m_number_of_full_occurrences = m_file_tmp.fileSize / m_receive_file_buffer.size();
			int lastPacketSize = m_file_tmp.fileSize - (m_number_of_full_occurrences * m_receive_file_buffer.size());
			if (lastPacketSize == 0) {
				m_last_packet_size = m_receive_file_buffer.size();
			}
			else {
				m_last_packet_size = lastPacketSize;
			}
		}
		
		void sendFile(const net::file<T>& file, std::function<void(net::file<T>)> errorCallbackSendFile) {
			if (m_is_for_files) {
				asio::post(m_asio_context, [this, file, callBackFile = std::move(errorCallbackSendFile)]() mutable {
					bool isAbleToWrite = m_safe_deque_outgoing_files.empty() && m_safe_deque_outgoing_messages.empty();
					m_safe_deque_outgoing_files.push_back(file);
					if (isAbleToWrite) {
						writeFileChunk(std::move(callBackFile));
					}
					});
			}
		}

		void writeFileChunk(std::function<void(net::file<T>)> errorCallbackSendFile)
		{
			if (!m_send_file_stream.is_open())
			{
				m_send_file_stream.open(m_safe_deque_outgoing_files.front().filePath, std::ios::binary);
				if (!m_send_file_stream.is_open())
				{
					if (errorCallbackSendFile) errorCallbackSendFile(m_safe_deque_outgoing_files.front());  // Уведомляем об ошибке открытия файла
					return;
				}
			}

			m_send_file_stream.read(m_send_file_buffer.data(), m_send_file_buffer.size());
			std::streamsize bytesRead = m_send_file_stream.gcount();

			if (bytesRead > 0)
			{
				asio::async_write(
					m_socket,
					asio::buffer(m_send_file_buffer.data(), m_send_file_buffer.size()), 
					[this, callBackFile = std::move(errorCallbackSendFile)](std::error_code ec, std::size_t) mutable
					{
						if (!ec)
						{
							writeFileChunk(std::move(callBackFile));
						}
						else
						{
							m_send_file_stream.close();
							disconnect();
							if (callBackFile) callBackFile(m_safe_deque_outgoing_files.front()); 
						}
					}
				);
			}
			else
			{
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
					if (!ec) {
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
					else {
						disconnect();
						m_on_disconnect_callback(this->shared_from_this());
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
					if (!ec) {
						addToIncomingMessagesQueue();
					}
					else {
						disconnect();
						m_on_disconnect_callback(this->shared_from_this());
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

		void writeHeader(std::function<void(message<T>)> errorCallbackSendMessage,
			std::function<void(file<T>)> errorCallbackSendFile)
		{
			asio::async_write(
				m_socket,
				asio::buffer(&m_safe_deque_outgoing_messages.front().header, sizeof(message_header<T>)),
				[this, callBackMessage = std::move(errorCallbackSendMessage),
				callBackFile = std::move(errorCallbackSendFile)](std::error_code ec, std::size_t length) mutable
				{
					if (!ec)
					{
						if (m_safe_deque_outgoing_messages.front().body.size() > 0)
						{
							writeBody(std::move(callBackMessage), std::move(callBackFile));
						}
						else
						{
							m_safe_deque_outgoing_messages.pop_front();

							if (!m_safe_deque_outgoing_messages.empty())
							{
								writeHeader(std::move(callBackMessage), std::move(callBackFile));
							}
							else if (!m_safe_deque_outgoing_files.empty())
							{
								writeFileChunk(std::move(callBackFile));
							}
						}
					}
					else
					{
						disconnect();
						callBackMessage(m_safe_deque_outgoing_messages.front());
						m_safe_deque_outgoing_messages.pop_front();
					}
				}
			);
		}

		void writeBody(std::function<void(message<T>)> errorCallbackSendMessage,
			std::function<void(file<T>)> errorCallbackSendFile)
		{
			asio::async_write(
				m_socket,
				asio::buffer(m_safe_deque_outgoing_messages.front().body.data(),
					m_safe_deque_outgoing_messages.front().body.size()),
				[this, callBackMessage = std::move(errorCallbackSendMessage),
				callBackFile = std::move(errorCallbackSendFile)](std::error_code ec, std::size_t length) mutable
				{
					if (!ec)
					{
						m_safe_deque_outgoing_messages.pop_front();

						if (!m_safe_deque_outgoing_messages.empty())
						{
							writeHeader(std::move(callBackMessage), std::move(callBackFile));
						}
						else if (!m_safe_deque_outgoing_files.empty())
						{
							writeFileChunk(std::move(callBackFile));
						}
					}
					else
					{
						disconnect();
						callBackMessage(m_safe_deque_outgoing_messages.front());
						m_safe_deque_outgoing_messages.pop_front();
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
					if (!ec) {
						if (m_owner_type == owner::client)
							readHeader();
					}
					else {
						disconnect();
						m_on_disconnect_callback(this->shared_from_this());
					}
				});
		}

		void readValidation() {
			asio::async_read(m_socket, asio::buffer(&m_hand_shake_in, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length) {
					if (!ec) {
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
								m_on_disconnect_callback(this->shared_from_this());
							}
						}
					}
					else {
						disconnect();
						m_on_disconnect_callback(this->shared_from_this());
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
		asio::io_context&			  m_asio_context;

		safe_deque<message<T>>		  m_safe_deque_outgoing_messages;
		safe_deque<owned_message<T>>& m_safe_deque_incoming_messages;
		message<T>					  m_message_tmp;

		safe_deque<file<T>>			  m_safe_deque_outgoing_files;
		safe_deque<owned_file<T>>*	  m_safe_deque_incoming_files;
		file<T>						  m_file_tmp;

		uint64_t					  m_hand_shake_out;
		uint64_t					  m_hand_shake_in;
		uint64_t					  m_hand_shake_check;

		std::function<void(std::shared_ptr<connection<T>>)> m_on_disconnect_callback;
	};
}
