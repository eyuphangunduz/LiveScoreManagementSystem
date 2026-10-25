#pragma once
#include "Packet.h"
#include "NoisyChannel.h"
#include <sys/socket.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


constexpr int DEFAULT_PORT = 8080;
constexpr int MAX_RETRANSMISSIONS = 20;     
constexpr size_t HEADER_SIZE = 4;      
inline const std::string END_OF_RESPONSE = "#END#";


inline bool sendAll(int socketFD, const uint8_t* data, size_t length) {
	size_t totalSent = 0;
	while (totalSent < length) {
		ssize_t sent = send(socketFD, data + totalSent, length - totalSent, 0);
		if (sent <= 0) return false;
		totalSent += sent;
	}
	return true;
}

inline bool recvAll(int socketFD, uint8_t* buffer, size_t length) {
	size_t totalReceived = 0;
	while (totalReceived < length) {
		ssize_t received = recv(socketFD, buffer + totalReceived, length - totalReceived, 0);
		if (received <= 0) return false;
		totalReceived += received;
	}
	return true;
}

inline std::string toHex(uint16_t value) {
	std::stringstream ss;
	ss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value;
	return ss.str();
}

inline std::string firstLine(const std::string& message) {
	size_t start = message.find_first_not_of('\n');
	if (start == std::string::npos) return "";
	return message.substr(start, message.find('\n', start) - start);
}

inline bool sendPacket(int socketFD, const std::string& message, const std::string& role) {
	Packet packet(message);
	std::vector<uint8_t> frame = packet.serialize();

	bool isControl = (message == END_OF_RESPONSE || message == "ACK" || message == "NACK");
	if (!isControl) {
		std::cout << "[" << role << "] Sent " << frame.size() << " bytes (checksum "
			<< toHex(packet.checksum) << "): " << firstLine(message) << std::endl;
	}
	return sendAll(socketFD, frame.data(), frame.size());
}

inline bool receivePacket(int socketFD, Packet& packet, size_t& totalBytes) {
	uint8_t header[HEADER_SIZE];
	if (!recvAll(socketFD, header, HEADER_SIZE)) return false;

	uint32_t payloadSize = (uint32_t(header[0]) << 24) | (uint32_t(header[1]) << 16)
		| (uint32_t(header[2]) << 8) | uint32_t(header[3]);
	if (payloadSize > 65536) return false;

	std::vector<uint8_t> rest(payloadSize + 2);
	if (!recvAll(socketFD, rest.data(), rest.size())) return false;

	packet.payload = std::string(rest.begin(), rest.begin() + payloadSize);
	packet.checksum = static_cast<uint16_t>((rest[payloadSize] << 8) | rest[payloadSize + 1]);
	totalBytes = HEADER_SIZE + rest.size();
	return true;
}

struct TransmissionStats {
	int packets = 0;          
	int retransmissions = 0; 
	int lost = 0;            
};

inline bool sendReliable(int socketFD, const std::string& message, NoisyChannel& channel,
	TransmissionStats& stats) {
	Packet packet(message);
	std::vector<uint8_t> original = packet.serialize();
	stats.packets++;

	if (message != END_OF_RESPONSE) {
		std::cout << "[SERVER] Sent " << original.size() << " bytes (checksum "
			<< toHex(packet.checksum) << "): " << firstLine(message) << std::endl;
	}

	for (int attempt = 0; attempt <= MAX_RETRANSMISSIONS; attempt++) {
		std::vector<uint8_t> frame = original;
		int flipped = channel.transmit(frame, HEADER_SIZE);
		if (flipped > 0) {
			std::cout << "[SERVER]   Noise flipped " << flipped << " bit(s) in transit" << std::endl;
		}

		if (!sendAll(socketFD, frame.data(), frame.size())) return false;

		Packet reply;
		size_t bytes = 0;
		if (!receivePacket(socketFD, reply, bytes)) return false;

		if (reply.payload == "ACK") return true;

		if (attempt < MAX_RETRANSMISSIONS) {
			stats.retransmissions++;
			std::cout << "[SERVER]   NACK received -> retransmitting (retry " << attempt + 1 << ")" << std::endl;
		}
	}

	stats.lost++;
	std::cout << "[SERVER]   Packet lost: gave up after " << MAX_RETRANSMISSIONS << " retransmissions." << std::endl;
	return true;
}