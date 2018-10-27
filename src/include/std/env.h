#pragma once

#include <std/queue.h>
#include <std/file.h>
#include <gui/objects.h>

#define MAX_PROC_MSGS    64
#define MAX_PROC_WINDOWS 16


struct Environment {
/*
The `Environment` is memory that is shared between the kernel and userland processes.

This is used for things like notifying the process of events that it subscribed to - via `msgQueue` - and syncing data with the gui - via `windows[]`, etc.
*/

	MsgQueue<MAX_PROC_MSGS> msgQueue;
	// TODO this needn't be in shared mem
	File *user_filetable[MAX_PROC_FILES];
	Window windows[MAX_PROC_WINDOWS];
	// start of free data. maybe add a *next pointer some day
	uchar free[0];
	Environment() {
		for (int i = 0; i < MAX_PROC_FILES; i++) {
			user_filetable[i] = nullptr;
		}
	}
};

