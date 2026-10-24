#pragma once 

class Match {
public:
	int homeTeamIndex = 0;
	int awayTeamIndex = 0;
	int homeGoals = 0;
	int awayGoals = 0;
	int week = 0;
	bool played = false;

	Match() = default;
	Match(int home, int away, int matchWeek)
		: homeTeamIndex(home), awayTeamIndex(away), week(matchWeek) {}
};