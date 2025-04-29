#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>


namespace net {

	class Connection;

	struct PacketHeader {
		uint32_t type;
		uint32_t size = sizeof(PacketHeader);
	};


	class Packet {
	public:
		uint32_t getSize() const;
		PacketHeader* getHeaderAdr();

		void setType(uint32_t type);
		uint32_t getType();


		// for "standard layout" data in & out
		template <typename DataType>
		friend Packet& operator << (Packet& packet, const DataType& data) {
			static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed");

			size_t currentSize = packet.m_vec_data.size();
			packet.m_vec_data.resize(packet.m_vec_data.size() + sizeof(DataType));
			std::memcpy(packet.m_vec_data.data() + currentSize, &data, sizeof(DataType));
			packet.updateSize(packet.m_vec_data.size());

			return packet;
		}

		template <typename DataType>
		friend Packet& operator >> (Packet& packet, DataType& data) {
			static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed");

			size_t remainingSize = packet.m_vec_data.size() - sizeof(DataType);
			std::memcpy(&data, packet.m_vec_data.data() + remainingSize, sizeof(DataType));
			packet.m_vec_data.resize(remainingSize);
			packet.updateSize(remainingSize);

			return packet;
		}


		// for strings in & out
		friend Packet& operator << (Packet& packet, const std::string& str) {
			packet.m_vec_data.insert(packet.m_vec_data.end(), str.begin(), str.end());
			uint32_t size = static_cast<uint32_t>(str.size());
			packet << size;
			packet.updateSize(packet.m_vec_data.size());

			return packet;
		}

		friend Packet& operator >> (Packet& packet, std::string& str) {
			uint32_t storedSize = 0;
			packet >> storedSize;

			if (packet.m_vec_data.size() < storedSize)
				throw std::runtime_error("packet size error");

			size_t remainingSize = packet.m_vec_data.size() - storedSize;

			str.assign(reinterpret_cast<const char*>(packet.m_vec_data.data() + remainingSize), storedSize);
			auto itFrom = packet.m_vec_data.begin() + remainingSize;
			packet.m_vec_data.erase(itFrom, itFrom + storedSize);
			packet.updateSize(remainingSize);

			return packet;
		}

		PacketHeader m_header;
		std::vector<std::uint8_t> m_vec_data;

	private:
		void updateSize(size_t newSize);

	};


	struct OwnedPacket {
		Connection* remote = nullptr;
		Packet packet;
	};
};