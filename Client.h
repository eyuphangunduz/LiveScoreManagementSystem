#pragma once
#include "Network.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>

class Client {
public:
	explicit Client(const std::string& ip = "127.0.0.1", int serverPort = DEFAULT_PORT)
		: serverIP(ip), port(serverPort) {
	}

	void run() {
		std::cout << "[CLIENT] Starting..." << std::endl;
		if (!connectToServer()) return;

		if (!readResponse()) {
			std::cout << "[CLIENT] Error: no response from the server." << std::endl;
			disconnect();
			return;
		}

		while (true) {
			std::string command = askForCommand();
			if (command.empty()) continue;

			if (!sendPacket(sockFD, command, "CLIENT") || !readResponse()) {
				std::cout << "[CLIENT] Error: connection to the server was lost." << std::endl;
				break;
			}

			if (command == "EXIT") break;
		}

		disconnect();
	}

private:
	std::string serverIP;
	int port;
	int sockFD = -1;

	bool connectToServer() {
		sockFD = socket(AF_INET, SOCK_STREAM, 0);
		if (sockFD < 0) {
			std::cout << "[CLIENT] Error: socket could not be created." << std::endl;
			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(port);

		if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) != 1) {
			std::cout << "[CLIENT] Error: invalid server address: " << serverIP << std::endl;
			close(sockFD);
			return false;
		}

		if (connect(sockFD, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
			std::cout << "[CLIENT] Error: could not connect to " << serverIP << ":" << port << std::endl;
			close(sockFD);
			return false;
		}

		std::cout << "[CLIENT] Connected to " << serverIP << ":" << port << std::endl;
		return true;
	}

	void disconnect() {
		close(sockFD);
		sockFD = -1;
		std::cout << "[CLIENT] Disconnected." << std::endl;
	}

	bool readResponse() {
		Packet packet;
		size_t bytes = 0;

		while (receivePacket(sockFD, packet, bytes)) {
			if (!packet.verifyChecksum()) {
				std::cout << "[CLIENT] Checksum mismatch (received " << toHex(packet.checksum)
					<< ", calculated " << toHex(Packet::calculateChecksum(packet.payload))
					<< ") -> NACK, requesting retransmission" << std::endl;
				if (!sendPacket(sockFD, "NACK", "CLIENT")) return false;
				continue;
			}

			if (!sendPacket(sockFD, "ACK", "CLIENT")) return false;

			if (packet.payload == END_OF_RESPONSE) return true;
			std::cout << packet.payload << std::endl;
		}
		return false;
	}

	void displayMenu() const {
		std::cout << "\n===== LIVE SCORE SYSTEM =====" << std::endl;
		std::cout << "1. Play next week (live)" << std::endl;
		std::cout << "2. View league standings" << std::endl;
		std::cout << "3. View next week's fixture" << std::endl;
		std::cout << "4. Reset season" << std::endl;
		std::cout << "5. Set channel SNR (noise)" << std::endl;
		std::cout << "6. Exit" << std::endl;
		std::cout << "Choice: ";
	}

	std::string askForCommand() {
		displayMenu();

		std::string choice;
		if (!std::getline(std::cin, choice)) return "EXIT";

		if (choice == "1") return "PLAY";
		if (choice == "2") return "STANDINGS";
		if (choice == "3") return "FIXTURE";

		if (choice == "4") {
			std::cout << "Are you sure you want to reset the season? (y/n): ";
			std::string answer;
			std::getline(std::cin, answer);
			if (answer == "y" || answer == "Y") return "RESET";
			std::cout << "Reset cancelled." << std::endl;
			return "";
		}

		if (choice == "5") {
			std::cout << "Enter SNR in dB (5-30), or 'off' for a perfect channel: ";
			std::string value;
			std::getline(std::cin, value);
			return "SNR " + value;
		}

		if (choice == "6") return "EXIT";

		std::cout << "Invalid choice. Please enter a number from 1 to 6." << std::endl;
		return "";
	}
};