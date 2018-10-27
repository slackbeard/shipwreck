#pragma once

//#include <util/debug.h>
#include <std/types.h>
#include <std/events.h>
#include <std/env.h>

enum SyscallCode {
	SYSCALL_NULL=0,

	SYSCALL_ALLOC,
	SYSCALL_ALLOC_AT,

	SYSCALL_OPEN,
	SYSCALL_SEEK,
	SYSCALL_READ,
	SYSCALL_WRITE,

	SYSCALL_NEW_THREAD,
	SYSCALL_YIELD,

	SYSCALL_GET_ENV,
	SYSCALL_SUBSCRIBE,

	SYSCALL_LOCK,
	SYSCALL_UNLOCK,

	SYSCALL_MONITOR,
	SYSCALL_UNMONITOR,
	SYSCALL_NOTIFY,

	SYSCALL_UPDATE_GUI,
	SYSCALL_REDRAW_GUI,

	MAX
};

struct SyscallAllocAtParams {
	uint pages;
	void *vaddr;
};
struct SyscallSeekParams {
	uint filehandle;
	int offset;
	void dump() {
		//debug(9, "SyscallSeekParams: filehandle=", (hex) filehandle, " offset=",(hex) offset);
	}
};

struct SyscallReadParams {
	uint filehandle;
	char *buffer;
	uint length;
	void dump() {
		//debug(9, "SyscallReadParams: filehandle=", (hex) filehandle, " buffer@", (hex) buffer, " length=", length);
	}
};

struct SyscallWriteParams {
	uint filehandle;
	const char *buffer;
	uint length;
	void dump() {
		//debug(9, "SyscallWriteParams: filehandle=", (hex) filehandle, " buffer@", (hex) buffer, " length=", length);
	}
};

struct SyscallThreadParams {
	void *function;
	void dump() {
		//debug(9, "SyscallThreadParams: function=", (hex) function);
	}
};

struct SyscallMonitorParams {
	int monitor_h;
	int diff;
	void dump() {
		//debug(9, "SyscallMonitorParams: monitor_h=", (hex) monitor_h, " diff=", (hex) diff);
	}
};


namespace sysapi {
	extern Environment *process_env;

	__attribute__((noinline))
	static int syscall(uint fn_code, void *data_ptr){
		int retval;
		__asm__ volatile(
			"movl %[data_ptr], %%edi\n"
			"pushl %%edi\n"
			"movl %[fn_code], %%edi\n"
			"pushl %%edi\n"
			"pushl $(1f)\n"
			"movl %%esp, %%edi\n"
			"sysenter\n"
			"1:\n"
			"addl $0x08, %%esp"
		:"=a" (retval)
		:[fn_code]"r" (fn_code),
		[data_ptr]"r" (data_ptr)
		:"ebp","ebx","edi");
		return retval;
	}

	extern void *alloc(int pages); 
	extern void *alloc_at(int pages, void *vaddr); 

	extern int open(const char *name);
	extern int seek(int fh, int offset);

	static int read(int fh, char *buf, uint len) {
		// check if file driver is overridden in userland:
		File *fdriver = process_env->user_filetable[fh % MAX_PROC_FILES];
		if (fdriver != nullptr) {
			return fdriver->read(buf, len);
		} else {
			SyscallReadParams params;
			params.filehandle = fh;
			params.buffer = buf;
			params.length = len;
			return syscall(SYSCALL_READ, &params);
		}
	}

	static int write(int fh, const char *buf, uint len) {
		// check if file driver is overridden in userland:
		File *fdriver = process_env->user_filetable[fh % MAX_PROC_FILES];
		if (fdriver != nullptr) {
			return fdriver->write(buf, len);
		} else {
			SyscallWriteParams params;
			params.filehandle = fh;
			params.buffer = buf;
			params.length = len;
			return syscall(SYSCALL_WRITE, &params);
		}
	}

	extern int new_thread(void *function);
	extern int yield();

	extern Environment *get_environment();
	extern int subscribe(UserEvents event);

	extern int lock(int lock_h);
	extern int unlock(int lock_h);

	extern int monitor(int monitor_h, int diff=1);
	extern int unmonitor(int monitor_h);
	extern int notify(int monitor_h, int diff=1);

	extern int update_gui();
	extern int redraw_gui();
};
