#pragma once
#include <string>

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

namespace net {

    struct MessageHeader {
        uint32_t type = 0;
        uint32_t size = 0;
    };


    struct Message {
        MessageHeader header{};
        std::vector<uint8_t> body{};

        size_t size() const {
            return sizeof(MessageHeader) + body.size();
        }

        template <typename DataType>
        friend Message& operator << (Message& msg, const DataType& data) {
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed");

            size_t i = msg.body.size();
            msg.body.resize(msg.body.size() + sizeof(DataType));
            std::memcpy(msg.body.data() + i, &data, sizeof(DataType));
            msg.header.size = msg.size();

            return msg;
        }

        friend Message& operator << (Message& msg, const std::string& str) {
            msg.body.insert(msg.body.end(), str.begin(), str.end()); 

            uint32_t size = static_cast<uint32_t>(str.size());
            msg << size;

            msg.header.size = msg.size();

            return msg;
        }

        template <typename DataType>
        friend Message& operator >> (Message& msg, DataType& data) {
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled");

            if (msg.body.size() < sizeof(DataType))
                throw std::runtime_error("Message body too small for data type");

            size_t i = msg.body.size() - sizeof(DataType);
            std::memcpy(&data, msg.body.data() + i, sizeof(DataType));
            msg.body.resize(i);
            msg.header.size = msg.size();

            return msg;
        }

        friend Message& operator >> (Message& msg, std::string& str) {
            uint32_t size = 0;
            msg >> size;

            if (msg.body.size() < size)
                throw std::runtime_error("Message body too small for string");

            str.assign(reinterpret_cast<const char*>(msg.body.data()), size);
            msg.body.erase(msg.body.begin(), msg.body.begin() + size);
            msg.header.size = msg.size();

            return msg;
        }
    };

    class Connection;

    struct OwnedMessage {
        Connection* connection;
        Message message;
    };
}