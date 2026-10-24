#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Packet {
public:
	std::string payload;     
	uint16_t checksum = 0;   

	Packet() = default;

	explicit Packet(const std::string& data) : payload(data) {
		checksum = calculateChecksum(payload);
	}

	static uint16_t calculateChecksum(const std::string& data) {
		uint32_t sum = 0;
		size_t i = 0;

		for (; i + 1 < data.size(); i += 2) {
			uint16_t word = (uint8_t(data[i]) << 8) | uint8_t(data[i + 1]);
			sum += word;
		}

		if (i < data.size()) {
			sum += uint8_t(data[i]) << 8;
		}

		while (sum >> 16) {
			sum = (sum & 0xFFFF) + (sum >> 16);
		}

		return static_cast<uint16_t>(~sum);
	}

	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> frame;
		uint32_t size = static_cast<uint32_t>(payload.size());

		frame.push_back((size >> 24) & 0xFF);
		frame.push_back((size >> 16) & 0xFF);
		frame.push_back((size >> 8) & 0xFF);
		frame.push_back(size & 0xFF);

		frame.insert(frame.end(), payload.begin(), payload.end());

		frame.push_back((checksum >> 8) & 0xFF);
		frame.push_back(checksum & 0xFF);

		return frame;
	}

	bool verifyChecksum() const {
		return calculateChecksum(payload) == checksum;
	}
};