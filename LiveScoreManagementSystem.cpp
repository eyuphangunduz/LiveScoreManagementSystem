#include "LiveScoreManagementSystem.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

using namespace std;

int main() {
	cout << "Program started." << endl;

	int serverSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocketFD < 0) {
		cout << "Error: socket could not be created." << endl;
		return 1;
	}
	cout << "Socket created. File descriptor: " << serverSocketFD << endl;

	int opt = 1;
	setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(8080);

	if (bind(serverSocketFD, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
		cout << "Error: bind failed." << endl;
		close(serverSocketFD);
		return 1;
	}

	cout << "Socket bound to port 8080." << endl;

	if (listen(serverSocketFD, 5) < 0) {
		cout << "Error: listen failed." << endl;
		close(serverSocketFD);
		return 1;
	}

	cout << "Server is listening..." << endl;


	sockaddr_in clientAddress{};
	socklen_t clientAddressLength = sizeof(clientAddress);

	cout << "Waiting for a client connection..." << endl;
	int clientSocketFD = accept(serverSocketFD, (sockaddr*)&clientAddress, &clientAddressLength);

	if (clientSocketFD < 0) {
		cout << "Error: accept failed." << endl;
		close(serverSocketFD);
		return 1;
	}

	char clientIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
	cout << "Client connected: " << clientIP << endl;

	string message = "Welcome to the Live Score server!\n";
	send(clientSocketFD, message.c_str(), message.size(), 0);
	cout << "Welcome message sent." << endl;

	close(clientSocketFD);
	cout << "Client disconnected." << endl;


	close(serverSocketFD);
	cout << "Socket closed." << endl;

	return 0;
}