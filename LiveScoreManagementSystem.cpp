#include "LiveScoreManagementSystem.h"
#include "Packet.h"
#include "LeagueManager.h"
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

bool sendPacket(int socketFD, const string& message) {
	Packet packet(message);
	vector<uint8_t> frame = packet.serialize();
	
	size_t start = message.find_first_not_of('\n');
	string preview = (start == string::npos) ? "" : message.substr(start, message.find('\n', start) - start);

	cout << "[SERVER] Sent " << frame.size() << " bytes (checksum " << toHex(packet.checksum)
		<< "): " << preview << endl;

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
		cout << "[SERVER] Error: bind failed." << endl;
		close(serverSocketFD);
		return;
	}

	if (listen(serverSocketFD, 5) < 0) {
		cout << "[SERVER] Error: listen failed." << endl;
		close(serverSocketFD);
		return;
	}
	cout << "[SERVER] Listening on port " << PORT << "..." << endl;

	sockaddr_in clientAddress{};
	socklen_t clientAddressLength = sizeof(clientAddress);
	int clientSocketFD = accept(serverSocketFD, (sockaddr*)&clientAddress, &clientAddressLength);
	if (clientSocketFD < 0) {
		cout << "[SERVER] Error: accept failed." << endl;
		close(serverSocketFD);
		return;
	}

	char clientIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
	cout << "[SERVER] Client connected: " << clientIP << endl;

	sendPacket(clientSocketFD, "Welcome to the Live Score server!");

	if (league.isSeasonFinished()) {
		sendPacket(clientSocketFD, "The season is finished. Final standings:");
		sendPacket(clientSocketFD, league.standingsToString());
	}
	else {
		int week = league.currentWeek;
		sendPacket(clientSocketFD, "========== WEEK " + to_string(week) + " - LIVE ==========");

		MatchSimulator simulator;
		simulator.minuteDelayMs = 50;  

		league.playCurrentWeek(simulator, [&](const string& event) {
			sendPacket(clientSocketFD, event);
			});

		sendPacket(clientSocketFD, league.fixtureToString(week));
		sendPacket(clientSocketFD, league.standingsToString());

		league.saveToFile(DATA_FILE);
		cout << "[SERVER] League saved to " << DATA_FILE << endl;
	}

	close(clientSocketFD);
	close(serverSocketFD);
	cout << "[SERVER] Shut down." << endl;
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

	Packet packet;
	size_t bytes = 0;
	int packetCount = 0;
	int corruptedCount = 0;

	while (receivePacket(sockFD, packet, bytes)) {
		packetCount++;
		if (packet.verifyChecksum()) {
			cout << packet.payload << endl;
		}
		else {
			corruptedCount++;
			cout << "[CLIENT] Checksum mismatch! Packet discarded." << endl;
		}
	}

	close(sockFD);
	cout << "\n[CLIENT] Connection closed. Packets received: " << packetCount
		<< ", corrupted: " << corruptedCount << endl;
}


int main(int argc, char* argv[]) {
	signal(SIGPIPE, SIG_IGN);  

	string mode = (argc > 1) ? argv[1] : "server";

	if (mode == "client") {
		runClient();
	}
	else if (mode == "standings") {
		LeagueManager league;
		league.loadOrInitialize(DATA_FILE);
		cout << "Next week: " << league.currentWeek << endl;
		cout << league.standingsToString();
	}
	else if (mode == "reset") {
		LeagueManager league;
		league.loadOrInitialize(DATA_FILE);
		league.resetSeason();
		league.saveToFile(DATA_FILE);
		cout << "Season has been reset." << endl;
		cout << league.standingsToString();
	}
	else {
		runServer();
	}
	return 0;
}