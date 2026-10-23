#include "LiveScoreManagementSystem.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

using namespace std;

const int PORT = 8080;

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

	string message = "Welcome to the Live Score server!";
	send(clientSocketFD, message.c_str(), message.size(), 0);
	cout << "[SERVER] Welcome message sent." << endl;

	close(clientSocketFD);
	close(serverSocketFD);
	cout << "[SERVER] Shut down." << endl;
}

void runClient() {
	cout << "[CLIENT] Starting..." << endl;

	// 1) Create a TCP socket
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

	char buffer[1024];
	ssize_t bytesReceived = recv(sockFD, buffer, sizeof(buffer) - 1, 0);
	if (bytesReceived > 0) {
		buffer[bytesReceived] = '\0';
		cout << "[CLIENT] " << bytesReceived << " bytes received: " << buffer << endl;
	}

	close(sockFD);
	cout << "[CLIENT] Disconnected." << endl;
}

int main(int argc, char* argv[]) {
	if (argc > 1 && string(argv[1]) == "client")
		runClient();
	else
		runServer();
	return 0;
}