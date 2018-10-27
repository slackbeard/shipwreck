#include <util/debug.h>
#include <std/types.h>
#include <locks.h>
#include <threads.h>
#include <process.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 0


void unmonitor(int monitor_h) {
	thisProc->unmonitor(monitor_h);
}

void monitor(int monitor_h, int diff) {
	thisProc->monitor(monitor_h, diff);
}

void notify(int monitor_h, int diff) {
	thisProc->notify(monitor_h, diff);
}

#pragma pop_macro("DEBUG_LEVEL")
