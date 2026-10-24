#pragma once
#include <string>

class Team {
public:
	std::string name;
	int played = 0;
	int won = 0;
	int drawn = 0;
	int lost = 0;
	int goalsFor = 0;
	int goalsAgainst = 0;
	int points = 0;

	Team() = default;
	explicit Team(const std::string& teamName) : name(teamName) {}

	void updateStats(int scored, int conceded) {
		played++;
		goalsFor += scored;
		goalsAgainst += conceded;

		if (scored > conceded) {
			won++;
			points += 3;
		}
		else if (scored == conceded) {
			drawn++;
			points += 1;
		}
		else {
			lost++;
		}
	}

	int goalDifference() const {
		return goalsFor - goalsAgainst;
	}

	void reset() {
		played = won = drawn = lost = 0;
		goalsFor = goalsAgainst = points = 0;
	}
};