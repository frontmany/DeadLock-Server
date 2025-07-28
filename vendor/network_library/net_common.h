#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include <system_error>
#include <filesystem>
#include <unordered_set>  
#include <chrono>
#include <cstdint>        
#include <string>              
#include <locale>                
#include <codecvt>   

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "asio.hpp"
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>