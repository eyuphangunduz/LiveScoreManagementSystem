#pragma once
#include "Network.h"
#include "NoisyChannel.h"
#include "Packet.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

inline void runBerExperiment(int packetsPerSnr = 5000) {
	Packet sample("[Minute 18'] GOAL! Galatasaray 1 - 0 Fenerbahce  (Galatasaray)");
	std::vector<uint8_t> original = sample.serialize();
	size_t bitsPerPacket = (original.size() - HEADER_SIZE) * 8;

	std::cout << "\n===== BER vs SNR EXPERIMENT (BPSK, " << packetsPerSnr << " packets per SNR) =====\n";
	std::cout << std::left << std::setw(10) << "SNR(dB)" << std::setw(16) << "Theoretical BER"
		<< std::setw(14) << "Measured BER" << std::setw(12) << "Corrupted"
		<< std::setw(10) << "Detected" << "Undetected\n";

	for (int snrDb = 0; snrDb <= 10; snrDb++) {
		NoisyChannel channel(snrDb);
		long long totalBitErrors = 0;
		int corrupted = 0, detected = 0, undetected = 0;

		for (int i = 0; i < packetsPerSnr; i++) {
			std::vector<uint8_t> frame = original;
			int flipped = channel.transmit(frame, HEADER_SIZE);
			totalBitErrors += flipped;
			if (flipped == 0) continue;

			corrupted++;

			Packet received;
			received.payload = std::string(frame.begin() + HEADER_SIZE, frame.end() - 2);
			received.checksum = static_cast<uint16_t>((frame[frame.size() - 2] << 8) | frame.back());

			if (received.verifyChecksum()) undetected++;
			else detected++;
		}

		double measuredBer = double(totalBitErrors) / (double(packetsPerSnr) * bitsPerPacket);

		std::cout << std::left << std::setw(10) << snrDb
			<< std::scientific << std::setprecision(2) << std::setw(16) << channel.theoreticalBER()
			<< std::setw(14) << measuredBer << std::defaultfloat
			<< std::setw(12) << corrupted << std::setw(10) << detected << undetected << "\n";
	}
}