#include "LiveScoreManagementSystem.h"
#include "Packet.h"
#include "LeagueManager.h"
#include "NoisyChannel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

using namespace std;

const int PORT = 8080;
const string DATA_FILE = "league_data.txt";
const string END_OF_RESPONSE = "#END#";     
const int MAX_RETRANSMISSIONS = 20;        
const size_t HEADER_SIZE = 4;              


bool sendAll(int socketFD, const uint8_t* data, size_t length) {
	size_t totalSent = 0;
	while (totalSent < length) {
		ssize_t sent = send(socketFD, data + totalSent, length - totalSent, 0);
		if (sent <= 0) return false;
		totalSent += sent;
	}
	return true;
}

bool recvAll(int socketFD, uint8_t* buffer, size_t length) {
	size_t totalReceived = 0;
	while (totalReceived < length) {
		ssize_t received = recv(socketFD, buffer + totalReceived, length - totalReceived, 0);
		if (received <= 0) return false;
		totalReceived += received;
	}
	return true;
}

string toHex(uint16_t value) {
	stringstream ss;
	ss << "0x" << hex << uppercase << setw(4) << setfill('0') << value;
	return ss.str();
}

string firstLine(const string& message) {
	size_t start = message.find_first_not_of('\n');
	if (start == string::npos) return "";
	return message.substr(start, message.find('\n', start) - start);
}

bool sendPacket(int socketFD, const string& message, const string& role) {
	Packet packet(message);
	vector<uint8_t> frame = packet.serialize();

	bool isControl = (message == END_OF_RESPONSE || message == "ACK" || message == "NACK");
	if (!isControl) {
		cout << "[" << role << "] Sent " << frame.size() << " bytes (checksum "
			<< toHex(packet.checksum) << "): " << firstLine(message) << endl;
	}
	return sendAll(socketFD, frame.data(), frame.size());
}

bool receivePacket(int socketFD, Packet& packet, size_t& totalBytes) {
	uint8_t header[4];
	if (!recvAll(socketFD, header, 4)) return false;

	uint32_t payloadSize = (uint32_t(header[0]) << 24) | (uint32_t(header[1]) << 16)
		| (uint32_t(header[2]) << 8) | uint32_t(header[3]);
	if (payloadSize > 65536) return false;

	vector<uint8_t> rest(payloadSize + 2);
	if (!recvAll(socketFD, rest.data(), rest.size())) return false;

	packet.payload = string(rest.begin(), rest.begin() + payloadSize);
	packet.checksum = static_cast<uint16_t>((rest[payloadSize] << 8) | rest[payloadSize + 1]);
	totalBytes = 4 + rest.size();
	return true;
}


struct TransmissionStats {
	int packets = 0;           
	int retransmissions = 0;  
	int lost = 0;              
};

bool sendReliable(int socketFD, const string& message, NoisyChannel& channel, TransmissionStats& stats) {
	Packet packet(message);
	vector<uint8_t> original = packet.serialize();
	stats.packets++;

	if (message != END_OF_RESPONSE) {
		cout << "[SERVER] Sent " << original.size() << " bytes (checksum "
			<< toHex(packet.checksum) << "): " << firstLine(message) << endl;
	}

	for (int attempt = 0; attempt <= MAX_RETRANSMISSIONS; attempt++) {
		vector<uint8_t> frame = original;
		int flipped = channel.transmit(frame, HEADER_SIZE);
		if (flipped > 0) {
			cout << "[SERVER]   Noise flipped " << flipped << " bit(s) in transit" << endl;
		}

		if (!sendAll(socketFD, frame.data(), frame.size())) return false;

		Packet reply;
		size_t bytes = 0;
		if (!receivePacket(socketFD, reply, bytes)) return false;

		if (reply.payload == "ACK") return true;

		if (attempt < MAX_RETRANSMISSIONS) {
			stats.retransmissions++;
			cout << "[SERVER]   NACK received -> retransmitting (retry " << attempt + 1 << ")" << endl;
		}
	}

	stats.lost++;
	cout << "[SERVER]   Packet lost: gave up after " << MAX_RETRANSMISSIONS << " retransmissions." << endl;
	return true;
}


void handleClient(int clientSocketFD, LeagueManager& league) {
	NoisyChannel channel(100.0);   
	TransmissionStats stats;

	auto send = [&](const string& message) {
		return sendReliable(clientSocketFD, message, channel, stats);
		};

	send("Welcome to the Live Score server!");
	send(END_OF_RESPONSE);

	Packet request;
	size_t bytes = 0;

	while (receivePacket(clientSocketFD, request, bytes)) {
		if (!request.verifyChecksum()) {
			cout << "[SERVER] Corrupted request discarded." << endl;
			send("Error: your request was corrupted, please try again.");
			send(END_OF_RESPONSE);
			continue;
		}

		string command = request.payload;
		cout << "[SERVER] Command received: " << command << " (" << bytes
			<< " bytes, checksum " << toHex(request.checksum) << " verified)" << endl;

		stats = TransmissionStats();

		if (command == "PLAY") {
			if (league.isSeasonFinished()) {
				send("The season is finished. Choose 'Reset season' to start a new one.");
			}
			else {
				int week = league.currentWeek;
				send("========== WEEK " + to_string(week) + " - LIVE ==========");

				MatchSimulator simulator;
				simulator.minuteDelayMs = 50;

				league.playCurrentWeek(simulator, [&](const string& event) {
					send(event);
					});

				send(league.fixtureToString(week));
				send(league.standingsToString());

				league.saveToFile(DATA_FILE);
				cout << "[SERVER] League saved to " << DATA_FILE << endl;

				send("Transmission summary: " + to_string(stats.packets) + " packets, "
					+ to_string(stats.retransmissions) + " retransmissions, "
					+ to_string(stats.lost) + " lost.");
			}
		}
		else if (command == "STANDINGS") {
			send(league.standingsToString());
		}
		else if (command == "FIXTURE") {
			if (league.isSeasonFinished())
				send("The season is finished. There are no more matches.");
			else
				send(league.fixtureToString(league.currentWeek));
		}
		else if (command == "RESET") {
			league.resetSeason();
			league.saveToFile(DATA_FILE);
			cout << "[SERVER] Season reset and saved." << endl;
			send("Season has been reset. Starting again from week 1.");
			send(league.standingsToString());
		}
		else if (command.rfind("SNR ", 0) == 0) { 
			string value = command.substr(4);
			if (value == "off" || value == "OFF") {
				channel.setSnrDb(100.0);
				cout << "[SERVER] Channel noise disabled." << endl;
				send("Noise disabled: the channel is now error-free.");
			}
			else {
				try {
					double snr = stod(value);
					if (snr < 5 || snr > 30) {
						send("Please enter an SNR between 5 and 30 dB.");
					}
					else {
						channel.setSnrDb(snr);
						stringstream ss;
						ss << "Channel SNR set to " << snr << " dB (theoretical BER "
							<< scientific << setprecision(2) << channel.theoreticalBER() << ").";
						cout << "[SERVER] " << ss.str() << endl;
						send(ss.str());
					}
				}
				catch (...) {
					send("Invalid SNR value: " + value);
				}
			}
		}
		else if (command == "EXIT") {
			send("Goodbye!");
			send(END_OF_RESPONSE);
			break;
		}
		else {
			send("Unknown command: " + command);
		}

		send(END_OF_RESPONSE);
	}
}

void runServer() {
	cout << "[SERVER] Starting..." << endl;

	LeagueManager league;
	league.loadOrInitialize(DATA_FILE);
	cout << "[SERVER] League loaded (next week: " << league.currentWeek << ")." << endl;

	int serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocketFD < 0) {
		cout << "[SERVER] Error: socket could not be created." << endl;
		return;
	}

	int opt = 1;
	setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(PORT);

	if (bind(serverSocketFD, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
		cout << "[SERVER] Error: bind failed. Is another server already running?" << endl;
		close(serverSocketFD);
		return;
	}

	if (listen(serverSocketFD, 5) < 0) {
		cout << "[SERVER] Error: listen failed." << endl;
		close(serverSocketFD);
		return;
	}
	cout << "[SERVER] Listening on port " << PORT << " (press Ctrl+C to stop)..." << endl;

	while (true) {
		sockaddr_in clientAddress{};
		socklen_t clientAddressLength = sizeof(clientAddress);
		int clientSocketFD = accept(serverSocketFD, (sockaddr*)&clientAddress, &clientAddressLength);
		if (clientSocketFD < 0) {
			cout << "[SERVER] Error: accept failed." << endl;
			continue;
		}

		char clientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
		cout << "[SERVER] Client connected: " << clientIP << endl;

		handleClient(clientSocketFD, league);

		close(clientSocketFD);
		cout << "[SERVER] Client disconnected: " << clientIP << endl;
		cout << "[SERVER] Waiting for the next client..." << endl;
	}
}

bool readResponse(int socketFD) {
	Packet packet;
	size_t bytes = 0;

	while (receivePacket(socketFD, packet, bytes)) {
		if (!packet.verifyChecksum()) {
			cout << "[CLIENT] Checksum mismatch (received " << toHex(packet.checksum)
				<< ", calculated " << toHex(Packet::calculateChecksum(packet.payload))
				<< ") -> NACK, requesting retransmission" << endl;
			if (!sendPacket(socketFD, "NACK", "CLIENT")) return false;
			continue;
		}

		if (!sendPacket(socketFD, "ACK", "CLIENT")) return false;

		if (packet.payload == END_OF_RESPONSE) return true;
		cout << packet.payload << endl;
	}
	return false;   // Connection was closed
}

void printMenu() {
	cout << "\n===== LIVE SCORE SYSTEM =====" << endl;
	cout << "1. Play next week (live)" << endl;
	cout << "2. View league standings" << endl;
	cout << "3. View next week's fixture" << endl;
	cout << "4. Reset season" << endl;
	cout << "5. Set channel SNR (noise)" << endl;
	cout << "6. Exit" << endl;
	cout << "Choice: ";
}

void runClient() {
	cout << "[CLIENT] Starting..." << endl;

	int sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFD < 0) {
		cout << "[CLIENT] Error: socket could not be created." << endl;
		return;
	}

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

	if (connect(sockFD, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
		cout << "[CLIENT] Error: could not connect to the server." << endl;
		close(sockFD);
		return;
	}
	cout << "[CLIENT] Connected to the server." << endl;

	if (!readResponse(sockFD)) {
		cout << "[CLIENT] Error: no response from the server." << endl;
		close(sockFD);
		return;
	}

	while (true) {
		printMenu();

		string choice;
		if (!getline(cin, choice)) choice = "6";

		string command;
		if (choice == "1") command = "PLAY";
		else if (choice == "2") command = "STANDINGS";
		else if (choice == "3") command = "FIXTURE";
		else if (choice == "4") {
			cout << "Are you sure you want to reset the season? (y/n): ";
			string answer;
			getline(cin, answer);
			if (answer != "y" && answer != "Y") {
				cout << "Reset cancelled." << endl;
				continue;
			}
			command = "RESET";
		}
		else if (choice == "5") {
			cout << "Enter SNR in dB (5-30), or 'off' for a perfect channel: ";
			string value;
			getline(cin, value);
			command = "SNR " + value;
		}
		else if (choice == "6") command = "EXIT";
		else {
			cout << "Invalid choice. Please enter a number from 1 to 6." << endl;
			continue;
		}

		if (!sendPacket(sockFD, command, "CLIENT") || !readResponse(sockFD)) {
			cout << "[CLIENT] Error: connection to the server was lost." << endl;
			break;
		}

		if (command == "EXIT") break;
	}

	close(sockFD);
	cout << "[CLIENT] Disconnected." << endl;
}

void runBerExperiment() {
	const int PACKETS_PER_SNR = 5000;

	Packet sample("[Minute 18'] GOAL! Galatasaray 1 - 0 Fenerbahce  (Galatasaray)");
	vector<uint8_t> original = sample.serialize();
	size_t bitsPerPacket = (original.size() - HEADER_SIZE) * 8;

	cout << "\n===== BER vs SNR EXPERIMENT (BPSK, " << PACKETS_PER_SNR << " packets per SNR) =====\n";
	cout << left << setw(10) << "SNR(dB)" << setw(16) << "Theoretical BER"
		<< setw(14) << "Measured BER" << setw(12) << "Corrupted"
		<< setw(10) << "Detected" << "Undetected\n";

	for (int snrDb = 0; snrDb <= 10; snrDb++) {
		NoisyChannel channel(snrDb);
		long long totalBitErrors = 0;
		int corrupted = 0, detected = 0, undetected = 0;

		for (int i = 0; i < PACKETS_PER_SNR; i++) {
			vector<uint8_t> frame = original;
			int flipped = channel.transmit(frame, HEADER_SIZE);
			totalBitErrors += flipped;
			if (flipped == 0) continue;

			corrupted++;

			Packet received;
			received.payload = string(frame.begin() + HEADER_SIZE, frame.end() - 2);
			received.checksum = static_cast<uint16_t>((frame[frame.size() - 2] << 8) | frame.back());

			if (received.verifyChecksum()) undetected++;
			else detected++;
		}

		double measuredBer = double(totalBitErrors) / (double(PACKETS_PER_SNR) * bitsPerPacket);

		cout << left << setw(10) << snrDb
			<< scientific << setprecision(2) << setw(16) << channel.theoreticalBER()
			<< setw(14) << measuredBer << defaultfloat
			<< setw(12) << corrupted << setw(10) << detected << undetected << "\n";
	}
}

int main(int argc, char* argv[]) {
	signal(SIGPIPE, SIG_IGN);   

	string mode = (argc > 1) ? argv[1] : "server";

	if (mode == "server") {
		runServer();
	}
	else if (mode == "client") {
		runClient();
	}
	else if (mode == "ber") {
		runBerExperiment();
	}
	else {
		cout << "Unknown mode: " << mode << endl;
		cout << "Usage: LiveScoreManagementSystem [server | client | ber]" << endl;
		return 1;
	}
	return 0;
}