#include <new>
#include <util/debug.h>
#include <devices/cpu.h>
#include <std/file.h>
#include <syscall.h>
#include <system.h>
#include <page.h>
#include <events.h>
#include <locks.h>
#include <process.h>
#include <memory.h>
#include <filesystem.h>

#pragma push_macro("DEBUG_LEVEL")
#define DEBUG_LEVEL 0


#define SYSCALL_STACK_PAGES 4


// map of syscall codes to their corresponding functions
SyscallPtr syscall_table[SyscallCode::MAX];


int syscall_null(uint data) {
	debug(9, "SYSCALL NULL data=", (hex) data);
	return data;
}

int syscall_alloc(uint pages) {
	sti();
	return (int) virt_alloc_pages(pages);
}

int syscall_alloc_at(SyscallAllocAtParams *params) {
	sti();
// TODO make sure vaddr is not already occupied
	return (int) virt_alloc_pages((uint) params->pages, (void *) params->vaddr);
}


// TODO file IO syscalls are not thread safe

static int alloc_proc_filehandle(File *file) {
	// TODO make this function atomic
	for (int f = 0; f < MAX_PROC_FILES; f++) {
		if (thisProc->files[f] == nullptr) {
			thisProc->files[f] = file;
			return f;
		}
	}
	return -1;
}
static File *get_file_by_handle(int fh) {
	if ((fh < 0) || (fh >= MAX_PROC_FILES)) return nullptr;
	return thisProc->files[fh];
}

int syscall_open(const char *name) {
	sti();
	File *file = filesystem->open(name);
	return alloc_proc_filehandle(file);
}
int syscall_seek(SyscallSeekParams *params) {
	File *file = get_file_by_handle(params->filehandle);
	if (file == nullptr) return -1;

	sti();
	return file->seek(params->offset);
}

int syscall_read(SyscallReadParams *params) {
	File *file = get_file_by_handle(params->filehandle);
	if (file == nullptr) return -1;

	sti();
	return file->read(params->buffer, params->length);
}


int syscall_write(SyscallWriteParams *params) {
	File *file = get_file_by_handle(params->filehandle);

	debug(8, "syscall_write fh=", (hex) params->filehandle, " file=", (hex) file);

	if (file == nullptr) return -1;

	sti();
	return file->write((const char *) params->buffer, params->length);
}


int syscall_new_thread(SyscallThreadParams *params) {
	Thread *thread = new_user_thread(params->function, 0);
	return (int) (thread != nullptr);
}

int syscall_yield() {
	yield();
	return 1;
}

int syscall_get_environment() {
	if (thisProc->userEnv == nullptr) {

		void *env_paddr = get_physical(thisProc->env);

		// user-visible address:
		thisProc->userEnv = (Environment *) next_virtual_pages(1);

		map_to( thisProc->userEnv, env_paddr, PAGE_USER_DATA);
		
	}
	return (int) thisProc->userEnv;
}

int syscall_subscribe(int event) {
	sub_proc_event(thisProc, (UserEvents) event);	
	return 1;
}

int syscall_lock(int lock_h) {
	thisProc->lock(thisProc->get_lock(lock_h));
	return 1;
}

int syscall_unlock(int lock_h) {
	thisProc->unlock(thisProc->get_lock(lock_h));
	return 1;
}

int syscall_monitor(SyscallMonitorParams *params) {
	params->dump();
	monitor(params->monitor_h, params->diff);
	return 1;
}

int syscall_unmonitor(int monitor_h) {
	unmonitor(monitor_h);
	return 1;
}

int syscall_notify(SyscallMonitorParams *params) {
	params->dump();
	notify(params->monitor_h, params->diff);
	return 1;
}

int syscall_update_gui() {

	gui_update_proc(proc_id);

	return 1;
}

int syscall_redraw_gui() {

	gui_redraw_proc(proc_id);

	return 1;
}

extern "C" int do_syscall(uint *user_args) {
// TODO check that address of user_args and bytes after it are safe (PDE and PTE are user-writable)

	// user_args[0] is the userland return address
	uint fn_code = user_args[1];

	if (fn_code >= (int) SyscallCode::MAX) {
		debug(0, "Syscall out of range: ", (hex) fn_code);
		return -1;
	}

	SyscallPtr syscall_fn = syscall_table[fn_code];

	uint syscall_args = user_args[2];

	uint retval = syscall_fn(syscall_args);

	return retval;	
}

void enter_kernel() {
	__asm__ volatile(

		"movl $0x10, %%edx\n"
		"movw %%dx, %%ds\n"
		"movw %%dx, %%es\n"

		"movl %[syscall_esp], %%esp\n"

		"pushl %%edi\n" // old esp

		"call do_syscall\n"
	::[syscall_esp]"m"(thisProc->threads[thisProc->thread_index].syscall_esp));

	__asm__ volatile(
		//"addl $0x04, %esp\n"
		"movl $0x23, %edx\n"
		"movw %dx, %ds\n"
		"movw %dx, %es\n"

		"pop %ecx\n"
		"movl $after_sysexit, %edx\n"
		"sti\n"
		"sysexit\n"
		"after_sysexit:\n"
		"ret"
	);

}

void init_syscall() {

	// ESP value for when `sysenter` first enters ring 0
	void *syscall_stack;
	int syscall_stack_bytes = sizeof(PageFrame) * SYSCALL_STACK_PAGES;
	syscall_stack = static_alloc_pages(SYSCALL_STACK_PAGES);

	volatile uint edx;
	volatile uint eax;
	volatile uint ecx;
	// Set the ring 0 cs in the IA_SYSENTER_CS MSR (0x174)
	edx = 0x0;
	eax = 0x08;
	ecx = 0x174;
	__asm__ volatile("wrmsr"::"a"(eax), "d"(edx), "c"(ecx));

	// Set the ring 0 esp in the IA_SYSENTER_ESP MSR (0x175)
	edx = 0x0;
	eax = (uint) syscall_stack + syscall_stack_bytes;
	ecx = 0x175;
	__asm__ volatile("wrmsr"::"a"(eax), "d"(edx), "c"(ecx));

	// Set the ring 0 eip in the IA_SYSENTER_EIP MSR (0x176)
	edx = 0x0;
	eax = (uint) &enter_kernel;
	ecx = 0x176;
	__asm__ volatile("wrmsr"::"a"(eax), "d"(edx), "c"(ecx));


	// set up syscall table

	// SYSCALL_NULL is just for testing:
	syscall_table[(int) SYSCALL_NULL] = (SyscallPtr) syscall_null;

	syscall_table[(int) SYSCALL_ALLOC] = (SyscallPtr) syscall_alloc;
	syscall_table[(int) SYSCALL_ALLOC_AT] = (SyscallPtr) syscall_alloc_at;

	syscall_table[(int) SYSCALL_OPEN] = (SyscallPtr) syscall_open;
	syscall_table[(int) SYSCALL_SEEK] = (SyscallPtr) syscall_seek;
	syscall_table[(int) SYSCALL_READ] = (SyscallPtr) syscall_read;
	syscall_table[(int) SYSCALL_WRITE] = (SyscallPtr) syscall_write;

	syscall_table[(int) SYSCALL_NEW_THREAD] = (SyscallPtr) syscall_new_thread;
	syscall_table[(int) SYSCALL_YIELD] = (SyscallPtr) syscall_yield;


	syscall_table[(int) SYSCALL_SUBSCRIBE] = (SyscallPtr) syscall_subscribe;
	syscall_table[(int) SYSCALL_GET_ENV] = (SyscallPtr) syscall_get_environment;


	syscall_table[(int) SYSCALL_LOCK] = (SyscallPtr) syscall_lock;
	syscall_table[(int) SYSCALL_UNLOCK] = (SyscallPtr) syscall_unlock;

	syscall_table[(int) SYSCALL_MONITOR] = (SyscallPtr) syscall_monitor;
	syscall_table[(int) SYSCALL_UNMONITOR] = (SyscallPtr) syscall_unmonitor;
	syscall_table[(int) SYSCALL_NOTIFY] = (SyscallPtr) syscall_notify;

	syscall_table[(int) SYSCALL_UPDATE_GUI] = (SyscallPtr) syscall_update_gui;
	syscall_table[(int) SYSCALL_REDRAW_GUI] = (SyscallPtr) syscall_redraw_gui;

}

#pragma pop_macro("DEBUG_LEVEL")
