#pragma once
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

class NoisyChannel {
public:
	explicit NoisyChannel(double snrDb = 100.0) : rng(std::random_device{}()) {
		setSnrDb(snrDb);
	}

	void setSnrDb(double newSnrDb) {
		snrDb = newSnrDb;
		double snrLinear = std::pow(10.0, snrDb / 10.0);
		bitErrorProbability = 0.5 * std::erfc(std::sqrt(snrLinear));
	}

	double getSnrDb() const { return snrDb; }
	double theoreticalBER() const { return bitErrorProbability; }

	int transmit(std::vector<uint8_t>& frame, size_t startByte = 0) {
		std::bernoulli_distribution flip(bitErrorProbability);
		int flippedBits = 0;

		for (size_t i = startByte; i < frame.size(); i++) {
			for (int bit = 0; bit < 8; bit++) {
				if (flip(rng)) {
					frame[i] ^= static_cast<uint8_t>(1 << bit);
					flippedBits++;
				}
			}
		}
		return flippedBits;
	}

private:
	double snrDb = 100.0;
	double bitErrorProbability = 0.0;
	std::mt19937 rng;
};