//***************************************************************************************
// GameTimer.h by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#ifndef GAMETIMER_H
#define GAMETIMER_H

#include <memory>

class GameTimer
{
public:
	GameTimer();

public:
	static GameTimer* Get();

public:
	float TotalTime(); // in seconds
	float DeltaTime(); // in seconds

	void Init();
	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

private:
	static GameTimer* Timer;

	double mSecondsPerCount = 0.0;
	double mDeltaTime = 0.0;

	__int64 mBaseTime = 0;
	__int64 mPausedTime = 0;
	__int64 mStopTime = 0;
	__int64 mPrevTime = 0;
	__int64 mCurrTime = 0;

	bool mStopped = false;
};

#endif // GAMETIMER_H