#pragma once

#include <chrono>

class Time {
private:
	std::chrono::system_clock::time_point  startTimeDir;
	std::chrono::system_clock::time_point  nowTimeDir;

	int nowTime;
	int oneSec;
	int oneBeatTime;

public:
	~Time();

	Time(const float bpm = 150.f);

	int getOneSec();
	int getOneBeatTime();
	int getNowTime();

	void init();
	void update();
};
