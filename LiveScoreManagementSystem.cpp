#include "Server.h"
#include "Client.h"
#include "Experiment.h"
#include <csignal>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
	std::signal(SIGPIPE, SIG_IGN);

	std::string mode = (argc > 1) ? argv[1] : "server";

	if (mode == "server") {
		Server server;
		server.startServer();
	}
	else if (mode == "client") {
		Client client;
		client.run();
	}
	else if (mode == "ber") {
		runBerExperiment();
	}

	else {
		std::cout << "Unknown mode: " << mode << std::endl;
		std::cout << "Usage: LiveScoreManagementSystem [ server | client | ber]" << std::endl;
		return 1;
	}
	return 0;
}