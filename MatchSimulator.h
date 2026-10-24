#pragma once
#include "Match.h"
#include <string>
#include <random>
#include <functional>

class MatchSimulator {
public:
	using EventCallback = std::function<void(const std::string&)>;

	MatchSimulator() : rng(std::random_device{}()) {}

	void simulateMatch(Match& match, const std::string& homeName,
		const std::string& awayName, const EventCallback& onEvent) {
		match.homeGoals = 0;
		match.awayGoals = 0;

		onEvent("[Kick-off] " + homeName + " vs " + awayName);

		for (int minute = 1; minute <= 90; minute++) {
			simulateMinute(match, minute, homeName, awayName, onEvent);
			if (minute == 45) {
				onEvent("[Half-time] " + scoreLine(match, homeName, awayName));
			}
		}

		match.played = true;
		onEvent("[Full-time] " + scoreLine(match, homeName, awayName));
	}

private:
	std::mt19937 rng;

	static constexpr double HOME_GOAL_CHANCE = 0.016;
	static constexpr double AWAY_GOAL_CHANCE = 0.013;

	void simulateMinute(Match& match, int minute, const std::string& homeName,
		const std::string& awayName, const EventCallback& onEvent) {
		std::uniform_real_distribution<double> chance(0.0, 1.0);
		double roll = chance(rng);

		if (roll < HOME_GOAL_CHANCE) {
			match.homeGoals++;
			onEvent("[Minute " + std::to_string(minute) + "'] GOAL! "
				+ scoreLine(match, homeName, awayName) + "  (" + homeName + ")");
		}
		else if (roll < HOME_GOAL_CHANCE + AWAY_GOAL_CHANCE) {
			match.awayGoals++;
			onEvent("[Minute " + std::to_string(minute) + "'] GOAL! "
				+ scoreLine(match, homeName, awayName) + "  (" + awayName + ")");
		}
	}

	static std::string scoreLine(const Match& match, const std::string& homeName,
		const std::string& awayName) {
		return homeName + " " + std::to_string(match.homeGoals) + " - "
			+ std::to_string(match.awayGoals) + " " + awayName;
	}
};