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

bool sendPacket(int socketFD, const string& message, const string& role) {
	Packet packet(message);
	vector<uint8_t> frame = packet.serialize();

	if (message != END_OF_RESPONSE) {
		size_t start = message.find_first_not_of('\n');
		string preview = (start == string::npos) ? "" : message.substr(start, message.find('\n', start) - start);
		cout << "[" << role << "] Sent " << frame.size() << " bytes (checksum "
			<< toHex(packet.checksum) << "): " << preview << endl;
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


void handleClient(int clientSocketFD, LeagueManager& league) {
	sendPacket(clientSocketFD, "Welcome to the Live Score server!", "SERVER");
	sendPacket(clientSocketFD, END_OF_RESPONSE, "SERVER");

	Packet request;
	size_t bytes = 0;

	while (receivePacket(clientSocketFD, request, bytes)) {
		if (!request.verifyChecksum()) {
			cout << "[SERVER] Corrupted request discarded." << endl;
			sendPacket(clientSocketFD, "Error: your request was corrupted, please try again.", "SERVER");
			sendPacket(clientSocketFD, END_OF_RESPONSE, "SERVER");
			continue;
		}

		string command = request.payload;
		cout << "[SERVER] Command received: " << command << " (" << bytes
			<< " bytes, checksum " << toHex(request.checksum) << " verified)" << endl;

		if (command == "PLAY") {
			if (league.isSeasonFinished()) {
				sendPacket(clientSocketFD, "The season is finished. Choose 'Reset season' to start a new one.", "SERVER");
			}
			else {
				int week = league.currentWeek;
				sendPacket(clientSocketFD, "========== WEEK " + to_string(week) + " - LIVE ==========", "SERVER");

				MatchSimulator simulator;
				simulator.minuteDelayMs = 50;

				league.playCurrentWeek(simulator, [&](const string& event) {
					sendPacket(clientSocketFD, event, "SERVER");
					});

				sendPacket(clientSocketFD, league.fixtureToString(week), "SERVER");
				sendPacket(clientSocketFD, league.standingsToString(), "SERVER");

				league.saveToFile(DATA_FILE);
				cout << "[SERVER] League saved to " << DATA_FILE << endl;
			}
		}
		else if (command == "STANDINGS") {
			sendPacket(clientSocketFD, league.standingsToString(), "SERVER");
		}
		else if (command == "FIXTURE") {
			if (league.isSeasonFinished())
				sendPacket(clientSocketFD, "The season is finished. There are no more matches.", "SERVER");
			else
				sendPacket(clientSocketFD, league.fixtureToString(league.currentWeek), "SERVER");
		}
		else if (command == "RESET") {
			league.resetSeason();
			league.saveToFile(DATA_FILE);
			cout << "[SERVER] Season reset and saved." << endl;
			sendPacket(clientSocketFD, "Season has been reset. Starting again from week 1.", "SERVER");
			sendPacket(clientSocketFD, league.standingsToString(), "SERVER");
		}
		else if (command == "EXIT") {
			sendPacket(clientSocketFD, "Goodbye!", "SERVER");
			sendPacket(clientSocketFD, END_OF_RESPONSE, "SERVER");
			break;
		}
		else {
			sendPacket(clientSocketFD, "Unknown command: " + command, "SERVER");
		}

		sendPacket(clientSocketFD, END_OF_RESPONSE, "SERVER");
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
			cout << "[CLIENT] Checksum mismatch! Packet discarded." << endl;
			continue;
		}
		if (packet.payload == END_OF_RESPONSE) return true;
		cout << packet.payload << endl;
	}
	return false;
}
void printMenu() {
	cout << "\n===== LIVE SCORE SYSTEM =====" << endl;
	cout << "1. Play next week (live)" << endl;
	cout << "2. View league standings" << endl;
	cout << "3. View next week's fixture" << endl;
	cout << "4. Reset season" << endl;
	cout << "5. Exit" << endl;
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
		if (!getline(cin, choice)) choice = "5";

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
		else if (choice == "5") command = "EXIT";
		else {
			cout << "Invalid choice. Please enter a number from 1 to 5." << endl;
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
	const size_t HEADER_SIZE = 4;

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