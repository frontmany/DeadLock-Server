#pragma once

#include "net_common.h"
#include <string>

namespace net {

    // message header
    template<typename T>
    struct message_header {
        T type{};
        uint32_t size = 0;
    };

    // message
    template<typename T>
    struct message {
        message_header<T> header{};
        std::vector<uint8_t> body;

        size_t size() const {
            return sizeof(message_header<T>) + body.size();
        }

        friend std::ostream& operator << (std::ostream& os, const message<T>& msg) {
            os << "ID: " << int(msg.header.type) << " Size: " << msg.header.size;
            return os;
        }

        template <typename DataType>
        friend message<T>& operator << (message<T>& msg, const DataType& data) {
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed");

            size_t i = msg.body.size();
            msg.body.resize(msg.body.size() + sizeof(DataType));
            std::memcpy(msg.body.data() + i, &data, sizeof(DataType));
            msg.header.size = msg.size();

            return msg;
        }

        friend message<T>& operator << (message<T>& msg, const std::string& str) {
            msg.body.insert(msg.body.end(), str.begin(), str.end()); 
            msg.header.size = msg.size();

            uint32_t size = static_cast<uint32_t>(str.size());
            msg << size;

            return msg;
        }

        template <typename DataType>
        friend message<T>& operator >> (message<T>& msg, DataType& data) {
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled");

            if (msg.body.size() < sizeof(DataType))
                throw std::runtime_error("Message body too small for data type");

            size_t i = msg.body.size() - sizeof(DataType);
            std::memcpy(&data, msg.body.data() + i, sizeof(DataType));
            msg.body.resize(i);
            msg.header.size = msg.size();

            return msg;
        }

        friend message<T>& operator >> (message<T>& msg, std::string& str) {
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

    template <typename T>
    class connection;

    template <typename T>
    struct owned_message {
        std::shared_ptr<connection<T>> remote = nullptr;
        message<T> msg;

        friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg) {
            os << msg.msg;
            return os;
        }
    };
};