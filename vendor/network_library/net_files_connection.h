#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_file.h"
#include "net_filesSender.h"
#include "net_filesReceiver.h"

#include "queryType.h"
#include "crypto.h"

#include <fstream>              
#include <filesystem>          
#include <system_error>          
#include <string>              
#include <locale>                
#include <codecvt>   

namespace net {
	template<typename T>
	class files_connection : public std::enable_shared_from_this<files_connection<T>> {
	public:
		files_connection(
			asio::io_context& asioContext,
			asio::ip::tcp::socket socket,
			safe_deque<owned_file<T>>& incomingFilesQueue,
			std::function<void(std::error_code, net::file<T>)> onReceiveFileError,
			std::function<void(std::error_code, net::file<T>)> onSendFileError,
			std::function<void(net::file<T>)> onFileSent)
			: m_asioContext(asioContext),
			m_socket(std::move(socket)),
			m_incomingFilesQueue(incomingFilesQueue),
			m_filesSender(asioContext, m_socket, onFileSent, onSendFileError),
			m_filesReceiver(m_socket, [this](file<T> file) {queueReceivedFile(file); }, onReceiveFileError)
		{
			m_filesReceiver.startReceiving();
		}

		bool isConnected() const {
			return m_socket.is_open();
		}

		virtual ~files_connection() {}

		void sendFile(const net::file<T>& file) {
			m_filesSender.sendFile(file);
		}

		void disconnect() {
			if (isConnected()) {
				m_socket.close();
			}
		}

		void queueReceivedFile(file<T> file) {
			m_incomingFilesQueue.push_back({ this->shared_from_this(), file });
		}

	private:
		asio::ip::tcp::socket m_socket;
		filesSender<T> m_filesSender;
		filesReceiver<T> m_filesReceiver;
		asio::io_context& m_asioContext;
		safe_deque<owned_file<T>>& m_incomingFilesQueue;
	};
}
