#pragma once

#include <std/types.h>
#include <util/debug.h>
#include <interrupts.h>
#include <devices/cpu.h>

/* The Task State Segment is mostly unused except for ESP0,
which holds a value for SS:ESP when a ring3 task gets interrupted */
struct TSS {
	uint prev_tss;
	uint esp0;
	uint ss0;
	uint esp1;
	uint ss1;
	uint esp2;
	uint ss2;
	uint cr3;
	uint eip;
	uint eflags;
	uint eax;
	uint ecx;
	uint edx;
	uint ebx;
	uint esp;
	uint ebp;
	uint esi;
	uint edi;
	uint es;
	uint cs;
	uint ss;
	uint ds;
	uint fs;
	uint gs;
	uint ldt;
	ushort trap;
	ushort iomap_base;
} __attribute__((packed));


struct InterruptParams {
	uint eip;
	uint cs;
	uint eflags;
	uint esp3;
	uint ss3;
	void dump() {
		debug(0, "  cs:eip=",(hex)cs, ":", (hex)eip, " eflags=", (hex) eflags, " ss:esp=", (hex) ss3, ":", (hex) esp3);
	}
};

struct PageFaultParams {
	uint code;
	uint eip;
	uint cs;
	uint eflags;
	uint esp3;
	uint ss3;
	void dump() {
		debug(0, "  code=", (hex)code, " cs:eip=",(hex)cs, ":", (hex)eip, " eflags=", (hex) eflags, " ss:esp=", (hex) ss3, ":", (hex) esp3);
	}
};

enum ThreadRunState {
	NULL=0,	 // uninitialized thread
	RUNNING,
	WAITING,
	DONE
};

struct ThreadState {
	uint edi;
	uint esi;
	uint ebp;
	uint esp;
	uint ebx;
	uint edx;
	uint ecx;
	uint eax;
	union {
		InterruptParams intParams;
		PageFaultParams pfParams;
		uchar intArgs[32];
	};

	void dump();
	
};

struct Thread {
	// cpuState points at the last place we stored the CPU state.
	// User thread states stay in the same place, but kernel threads store CPU state wherever the ESP was in the kernel thread
	ThreadState *cpuState;
	ThreadRunState runState;
	void *stack;
	uint stack_bytes;

	// lock, signal
	Thread *lock_next;
	int *signal_wait;

	void init(void *function, void *stack, uint stack_bytes){}
	void initCPUState();
	void setStack(void *stack, uint stack_bytes);

	void setFunction(void *function);

	void dump();
};

extern Thread *thisThread;

struct KernelThread: public Thread {
	void init(void *function, void *stack, uint stack_bytes);
};

struct UserThread: public Thread {
	void *syscall_stack;
	uint syscall_stack_bytes;
	uint syscall_esp; // simply points at the end of syscall_stack
	void init(void *function, void *stack, uint stack_bytes);
};


UserThread *new_user_thread(void *function, int data, uint stack_bytes=0, void *stack=nullptr);

void yield();
