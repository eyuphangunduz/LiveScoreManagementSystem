#pragma once
#include "Team.h"
#include "Match.h"
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

class LeagueManager {
public:
	std::vector<Team> teams;
	std::vector<Match> matches;
	int currentWeek = 1;

	void initialize() {
		teams.clear();
		std::vector<std::string> names = {
			"Galatasaray", "Fenerbahce", "Besiktas", "Trabzonspor",
			"Basaksehir", "Samsunspor", "Goztepe", "Kasimpasa"
		};
		for (const std::string& name : names) {
			teams.push_back(Team(name));
		}
		generateFixture();
		currentWeek = 1;
	}

	int totalWeeks() const {
		return static_cast<int>(teams.size()) - 1;
	}

	void generateFixture() {
		matches.clear();
		int n = static_cast<int>(teams.size());

		std::vector<int> order(n);
		std::iota(order.begin(), order.end(), 0);

		for (int round = 0; round < n - 1; round++) {
			for (int i = 0; i < n / 2; i++) {
				int home = order[i];
				int away = order[n - 1 - i];
				if (round % 2 == 1) std::swap(home, away);
				matches.push_back(Match(home, away, round + 1));
			}

			int last = order[n - 1];
			for (int k = n - 1; k > 1; k--) {
				order[k] = order[k - 1];
			}
			order[1] = last;
		}
	}

	std::vector<Team> getStandings() const {
		std::vector<Team> sorted = teams;
		std::sort(sorted.begin(), sorted.end(), [](const Team& a, const Team& b) {
			if (a.points != b.points) return a.points > b.points;
			if (a.goalDifference() != b.goalDifference()) return a.goalDifference() > b.goalDifference();
			return a.goalsFor > b.goalsFor;
			});
		return sorted;
	}

	std::string standingsToString() const {
		std::stringstream ss;
		ss << "\n========== LEAGUE STANDINGS ==========\n";
		ss << std::left << std::setw(4) << "#" << std::setw(14) << "Team"
			<< std::right << std::setw(3) << "P" << std::setw(3) << "W"
			<< std::setw(3) << "D" << std::setw(3) << "L"
			<< std::setw(5) << "GD" << std::setw(5) << "Pts" << "\n";

		std::vector<Team> standings = getStandings();
		for (size_t i = 0; i < standings.size(); i++) {
			const Team& t = standings[i];
			ss << std::left << std::setw(4) << (i + 1) << std::setw(14) << t.name
				<< std::right << std::setw(3) << t.played << std::setw(3) << t.won
				<< std::setw(3) << t.drawn << std::setw(3) << t.lost
				<< std::setw(5) << t.goalDifference() << std::setw(5) << t.points << "\n";
		}
		return ss.str();
	}

	std::string fixtureToString(int week) const {
		std::stringstream ss;
		ss << "\n---------- WEEK " << week << " ----------\n";
		for (const Match& m : matches) {
			if (m.week != week) continue;
			ss << std::left << std::setw(14) << teams[m.homeTeamIndex].name;
			if (m.played) ss << " " << m.homeGoals << " - " << m.awayGoals << " ";
			else          ss << "  vs   ";
			ss << teams[m.awayTeamIndex].name << "\n";
		}
		return ss.str();
	}
};