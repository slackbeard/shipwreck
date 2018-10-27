#pragma once

#include <threads.h>

struct Lock {
	Thread *owner;
	Thread *next;

};

struct Monitor {
	Lock lock;
	int signal;
};


void notify(int monitor_h, int diff=1);

void monitor(int monitor_h, int diff=1);

void unmonitor(int monitor_h);

