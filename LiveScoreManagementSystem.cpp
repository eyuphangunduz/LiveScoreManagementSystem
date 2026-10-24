#include "LiveScoreManagementSystem.h"
#include "Packet.h"
#include "LeagueManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

using namespace std;

const int PORT = 8080;


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

void runServer() {
	cout << "[SERVER] Starting..." << endl;

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

	Packet packet("Welcome to the Live Score server!");
	vector<uint8_t> frame = packet.serialize();

	cout << "[SERVER] Serialization: " << packet.payload.size() << "-byte payload constructed." << endl;
	cout << "[SERVER] Checksum: Generated " << toHex(packet.checksum) << ", appended to frame." << endl;

	if (sendAll(clientSocketFD, frame.data(), frame.size())) {
		cout << "[SERVER] Network: Transmitted " << frame.size() << " bytes over TCP socket." << endl;
	}
	else {
		cout << "[SERVER] Error: transmission failed." << endl;
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

	uint8_t header[4];
	if (!recvAll(sockFD, header, 4)) {
		cout << "[CLIENT] Error: could not read packet header." << endl;
		close(sockFD);
		return;
	}
	uint32_t payloadSize = (uint32_t(header[0]) << 24) | (uint32_t(header[1]) << 16)
		| (uint32_t(header[2]) << 8) | uint32_t(header[3]);

	if (payloadSize > 65536) {
		cout << "[CLIENT] Error: invalid packet size." << endl;
		close(sockFD);
		return;
	}

	vector<uint8_t> rest(payloadSize + 2);
	if (!recvAll(sockFD, rest.data(), rest.size())) {
		cout << "[CLIENT] Error: could not read packet body." << endl;
		close(sockFD);
		return;
	}
	cout << "[CLIENT] Network: " << (4 + rest.size()) << " bytes received." << endl;

	Packet packet;
	packet.payload = string(rest.begin(), rest.begin() + payloadSize);
	packet.checksum = (uint16_t(rest[payloadSize]) << 8) | uint16_t(rest[payloadSize + 1]);

	if (packet.verifyChecksum()) {
		cout << "[CLIENT] Integrity: Checksum verified (" << toHex(packet.checksum) << ")." << endl;
		cout << "[CLIENT] Display: " << packet.payload << endl;
	}
	else {
		cout << "[CLIENT] Integrity: Checksum MISMATCH! Packet discarded." << endl;
	}

	close(sockFD);
	cout << "[CLIENT] Disconnected." << endl;
}

int main(int argc, char* argv[]) {
	string mode = (argc > 1) ? argv[1] : "server";

	if (mode == "client") {
		runClient();
	}
	else if (mode == "league") {
		LeagueManager league;
		league.initialize();
		for (int w = 1; w <= league.totalWeeks(); w++) {
			cout << league.fixtureToString(w);
		}
		cout << league.standingsToString();
	}
	else {
		runServer();
	}
	return 0;
}