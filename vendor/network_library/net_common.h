#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <set>
#include <variant>
#include <fstream>
#include <iostream>
#include <system_error>
#include <filesystem>
#include <unordered_set>  
#include <system_error> 
#include <algorithm>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include "asio.hpp"
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>


namespace net {
	enum class owner {
		server,
		client
	};

	enum class connection_type {
		messages,
		files
	};
}