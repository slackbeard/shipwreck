#include <std/types.h>
#include <std/events.h>
#include <std/env.h>
#include <system.h>

namespace sysapi {
	Environment *process_env;

	extern void *alloc(int pages) {
		return (void *) syscall(SYSCALL_ALLOC, (void *) pages);
	}

	extern void *alloc_at(int pages, void *vaddr) {
		SyscallAllocAtParams params;
		params.pages = pages;
		params.vaddr = vaddr;
		return (void *) syscall(SYSCALL_ALLOC_AT, &params);
	}

	extern int open(const char *name) {
		return syscall(SYSCALL_OPEN, (void *) name);
	}

	extern int seek(int fh, int offset) {
		SyscallSeekParams params;
		params.filehandle = fh;
		params.offset = offset;
		return syscall(SYSCALL_SEEK, &params);
	}



	extern int new_thread(void *function) {
		SyscallThreadParams params;
		params.function = function;
		return syscall(SYSCALL_NEW_THREAD, &params);
	}

	extern int yield() {
		return syscall(SYSCALL_YIELD, nullptr);
	}

	extern Environment *get_environment() {
		return (Environment *) syscall(SYSCALL_GET_ENV, nullptr);
	}

	extern int subscribe(UserEvents event) {
		return syscall(SYSCALL_SUBSCRIBE, (void *) event);
	}

	extern int lock(int lock_h) {
		return syscall(SYSCALL_LOCK, (void *) lock_h);
	}

	extern int unlock(int lock_h) {
		return syscall(SYSCALL_UNLOCK, (void *) lock_h);
	}

	extern int monitor(int monitor_h, int diff) {
		SyscallMonitorParams params;
		params.monitor_h = monitor_h;
		params.diff = diff;
		return syscall(SYSCALL_MONITOR, &params);
	}

	extern int unmonitor(int monitor_h) {
		return syscall(SYSCALL_UNMONITOR, (void *) monitor_h);
	}

	extern int notify(int monitor_h, int diff) {
		SyscallMonitorParams params;
		params.monitor_h = monitor_h;
		params.diff = diff;
		return syscall(SYSCALL_NOTIFY, &params);
	}

	extern int update_gui() {
		return syscall(SYSCALL_UPDATE_GUI, nullptr);
	}

	extern int redraw_gui() {
		return syscall(SYSCALL_REDRAW_GUI, nullptr);
	}


};
