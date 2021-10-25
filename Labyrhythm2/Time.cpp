#include "Time.h"

using namespace std::chrono;

Time::~Time() {

}

Time::Time(const float bpm) :
	nowTime(0),
	oneSec(1000),
	oneBeatTime(60 * oneSec / bpm),
	startTimeDir(system_clock::now()),
	nowTimeDir(system_clock::now()) {

}

int Time::getOneSec() { return oneSec; }
int Time::getOneBeatTime() { return oneBeatTime; }
int Time::getNowTime() { return nowTime; }

void Time::init() {
	startTimeDir = system_clock::now();
}

void Time::update() {
	nowTimeDir = system_clock::now();
	nowTime = duration_cast<milliseconds>(nowTimeDir - startTimeDir).count();
}
