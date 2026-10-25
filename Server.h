#pragma once
#include "Network.h"
#include "LeagueManager.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

class Server {
public:
	explicit Server(int serverPort = DEFAULT_PORT, const std::string& leagueFile = "league_data.txt")
		: port(serverPort), dataFile(leagueFile) {
	}

	void startServer() {
		std::cout << "[SERVER] Starting..." << std::endl;

		league.loadOrInitialize(dataFile);
		std::cout << "[SERVER] League loaded (next week: " << league.currentWeek << ")." << std::endl;

		if (!openListeningSocket()) return;
		handleConnections();
	}

private:
	using Sender = std::function<bool(const std::string&)>;

	int port;
	std::string dataFile;
	LeagueManager league;
	int serverSocketFD = -1;

	bool openListeningSocket() {
		serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocketFD < 0) {
			std::cout << "[SERVER] Error: socket could not be created." << std::endl;
			return false;
		}

		int opt = 1;
		setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(port);

		if (bind(serverSocketFD, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
			std::cout << "[SERVER] Error: bind failed. Is another server already running?" << std::endl;
			close(serverSocketFD);
			return false;
		}

		if (listen(serverSocketFD, 5) < 0) {
			std::cout << "[SERVER] Error: listen failed." << std::endl;
			close(serverSocketFD);
			return false;
		}

		std::cout << "[SERVER] Listening on port " << port << " (press Ctrl+C to stop)..." << std::endl;
		return true;
	}

	void handleConnections() {
		while (true) {
			sockaddr_in clientAddress{};
			socklen_t clientAddressLength = sizeof(clientAddress);
			int clientSocketFD = accept(serverSocketFD, (sockaddr*)&clientAddress, &clientAddressLength);
			if (clientSocketFD < 0) {
				std::cout << "[SERVER] Error: accept failed." << std::endl;
				continue;
			}

			char clientIP[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
			std::cout << "[SERVER] Client connected: " << clientIP << std::endl;

			handleClient(clientSocketFD);

			close(clientSocketFD);
			std::cout << "[SERVER] Client disconnected: " << clientIP << std::endl;
			std::cout << "[SERVER] Waiting for the next client..." << std::endl;
		}
	}

	void handleClient(int clientSocketFD) {
		NoisyChannel channel(100.0); 
		TransmissionStats stats;

		Sender sendToClient = [&](const std::string& message) {
			return sendReliable(clientSocketFD, message, channel, stats);
			};

		sendToClient("Welcome to the Live Score server!");
		sendToClient(END_OF_RESPONSE);

		Packet request;
		size_t bytes = 0;

		while (receivePacket(clientSocketFD, request, bytes)) {
			if (!request.verifyChecksum()) {
				std::cout << "[SERVER] Corrupted request discarded." << std::endl;
				sendToClient("Error: your request was corrupted, please try again.");
				sendToClient(END_OF_RESPONSE);
				continue;
			}

			std::string command = request.payload;
			std::cout << "[SERVER] Command received: " << command << " (" << bytes
				<< " bytes, checksum " << toHex(request.checksum) << " verified)" << std::endl;

			stats = TransmissionStats();

			if (command == "PLAY") {
				playWeek(sendToClient, stats);
			}
			else if (command == "STANDINGS") {
				sendToClient(league.standingsToString());
			}
			else if (command == "FIXTURE") {
				if (league.isSeasonFinished())
					sendToClient("The season is finished. There are no more matches.");
				else
					sendToClient(league.fixtureToString(league.currentWeek));
			}
			else if (command == "RESET") {
				league.resetSeason();
				league.saveToFile(dataFile);
				std::cout << "[SERVER] Season reset and saved." << std::endl;
				sendToClient("Season has been reset. Starting again from week 1.");
				sendToClient(league.standingsToString());
			}
			else if (command.rfind("SNR ", 0) == 0) {
				setChannelSnr(command.substr(4), channel, sendToClient);
			}
			else if (command == "EXIT") {
				sendToClient("Goodbye!");
				sendToClient(END_OF_RESPONSE);
				break;
			}
			else {
				sendToClient("Unknown command: " + command);
			}

			sendToClient(END_OF_RESPONSE);
		}
	}

	void playWeek(const Sender& sendToClient, TransmissionStats& stats) {
		if (league.isSeasonFinished()) {
			sendToClient("The season is finished. Choose 'Reset season' to start a new one.");
			return;
		}

		int week = league.currentWeek;
		sendToClient("========== WEEK " + std::to_string(week) + " - LIVE ==========");

		MatchSimulator simulator;
		simulator.minuteDelayMs = 50;

		league.playCurrentWeek(simulator, [&](const std::string& event) {
			sendToClient(event);
			});

		sendToClient(league.fixtureToString(week));
		sendToClient(league.standingsToString());

		league.saveToFile(dataFile);
		std::cout << "[SERVER] League saved to " << dataFile << std::endl;

		sendToClient("Transmission summary: " + std::to_string(stats.packets) + " packets, "
			+ std::to_string(stats.retransmissions) + " retransmissions, "
			+ std::to_string(stats.lost) + " lost.");
	}

	void setChannelSnr(const std::string& value, NoisyChannel& channel, const Sender& sendToClient) {
		if (value == "off" || value == "OFF") {
			channel.setSnrDb(100.0);
			std::cout << "[SERVER] Channel noise disabled." << std::endl;
			sendToClient("Noise disabled: the channel is now error-free.");
			return;
		}

		try {
			double snr = std::stod(value);
			if (snr < 5 || snr > 30) {
				sendToClient("Please enter an SNR between 5 and 30 dB.");
				return;
			}

			channel.setSnrDb(snr);
			std::stringstream ss;
			ss << "Channel SNR set to " << snr << " dB (theoretical BER "
				<< std::scientific << std::setprecision(2) << channel.theoreticalBER() << ").";
			std::cout << "[SERVER] " << ss.str() << std::endl;
			sendToClient(ss.str());
		}
		catch (...) {
			sendToClient("Invalid SNR value: " + value);
		}
	}
};