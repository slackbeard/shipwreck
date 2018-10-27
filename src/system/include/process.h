#pragma once

#include <std/types.h>
#include <threads.h>
#include <locks.h>
#include <std/file.h>
#include <std/env.h>
#include <std/events.h>

#define MAX_PROC_LOCKS    64
#define MAX_PROC_MONITORS 64

#define MAX_PROC_THREADS 16
#define MAX_PROCS        16


struct Process {
// process control block
	uint cr3;
	uint thread_index;
	uint num_running_threads;

	File *files[MAX_PROC_FILES];

	Lock locks[MAX_PROC_LOCKS];
	Monitor monitors[MAX_PROC_MONITORS];

	// cached ptrs to lock & signal of msg monitor
	Lock *msg_lock;
	int *msg_signal;

	UserThread threads[MAX_PROC_THREADS];	

	// global kernel-space address for environment:
	Environment *env;

	// user-space address for environment:
	Environment *userEnv;

	void lock(Lock *lock);
	void unlock(Lock *lock);

	void send_msg(Message *msg);

	void monitor(int monitor_h, int diff=1);
	void unmonitor(int monitor_h);
	void notify(int monitor_h, int diff=1);

	Lock *get_lock(int l_h) {
		return &locks[l_h % MAX_PROC_LOCKS];
	}

	Monitor *get_monitor(int m_h) {
		return &monitors[m_h % MAX_PROC_MONITORS];
	}

};

extern volatile uint proc_id;
extern uint num_procs;
extern Process *thisProc;
extern Process *procs;
extern volatile TSS *tss;

void init_processes();
Process *new_user_process();
