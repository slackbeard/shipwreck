#include <new>
#include <std/string.h>
#include <std/file.h>
#include <devices/cpu.h>
#include <devices/vga.h>
#include <memory.h>
#include <process.h>
#include <syscall.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 0


File nullfile;
volatile uint proc_id;
uint num_procs;
Process *thisProc;
Process *procs;

volatile TSS *tss;

void Process::lock(Lock *lock) {
// wait on mutex. threads wait in FIFO order

	cli();

	// check for waiting threads

	if ((lock->owner == thisThread) || (lock->owner == nullptr)) {
	// if we have the lock or its free:

		lock->owner = thisThread;

		sti();

		return;
	}

	debug(9, "Lock owned by thread@", (hex) lock->owner);

	thisThread->lock_next = nullptr;

	if (lock->next == nullptr) {
	// if no other thread is waiting

		debug(9, "starting wait queue lock->next=", (hex) thisThread);

		lock->next = thisThread;

	} else {
	// else, append this thread to the queue

		Thread *tnode = lock->next;

		// go to second-last node 
		for (;tnode->lock_next != nullptr; tnode = tnode->lock_next);

		tnode->lock_next = thisThread;
		debug(9, "adding to queue: thread=", (hex) thisThread);
	}

	thisThread->runState = WAITING;
	this->num_running_threads--;
	sti();

	yield();
}

void Process::unlock(Lock *lock) {
// release lock and wake the next waiting thread

	cli();

// release lock and wake the next waiting thread


	lock->owner = lock->next;

	if (lock->owner != nullptr) {
	// wake next waiting thread if there is one:

		lock->owner->runState = RUNNING;

		lock->next = lock->owner->lock_next;

		lock->owner->lock_next = nullptr;;

	}

	sti();

}

void Process::send_msg(Message *msg) {
	env->msgQueue.enqueue(msg);	

	(*msg_signal)++;
}

void Process::monitor(int monitor_h, int diff) {
/*

 Monitor signal for a positive value, subtract up to `diff` from the signal

 The currently executing thread will block until `this` process owns `mon->lock` and `mon->signal` is signalled.

 The current thread doesn't need to belong to `this` process, eg. so kernel threads can execute in any process context while using `procs[0]` for monitors etc.

 The lock remains locked after the signal is raised

*/

	cli();

	Monitor *mon = this->get_monitor(monitor_h);

	this->lock(&mon->lock);

	if (mon->lock.owner != thisThread) {
	// if somehow we didn't acquire the lock
	// TODO wait timeout
		return;
	}
	if (mon->signal <= 0) {

		thisThread->signal_wait = &mon->signal;

		thisThread->runState = WAITING;

		this->num_running_threads--;

		sti();

		yield();

		cli();

	}

	mon->signal -= diff;

	// clamp to 0
	if (mon->signal < 0) mon->signal = 0;

	sti();

	// keep ownership of lock
}

void Process::unmonitor(int monitor_h) {
	cli();

	Monitor *mon = thisProc->get_monitor(monitor_h);

	this->unlock(&mon->lock);

	sti();
}

void Process::notify(int monitor_h, int diff) {
// notify only the single thread listening on `monitor`
// diff should probably be >0 for this to make sense
	cli();

	Monitor *mon = this->get_monitor(monitor_h);

	mon->signal += diff;

	if (mon->signal > 0) {
	// only one thread can have a lock on the monitor, so only signal that thread:

		Thread *owner = mon->lock.owner;

		if ((owner != nullptr) && (owner->signal_wait == &mon->signal)){

			owner->signal_wait = nullptr;
			owner->runState = RUNNING;
			this->num_running_threads++;

		}

	}
}

void init_processes() {
	tss = (volatile TSS *) 0xD00;
	tss->ss0 = 0x10;

	int proc_pages = (sizeof(Process) * MAX_PROCS + 4095) / sizeof(PageFrame);
	procs = (Process *) static_alloc_pages(proc_pages);

	// fill in threads
	for (int p = 0; p < MAX_PROCS; p++) {
		procs[p].num_running_threads = 0;
		Monitor *msg_mon = procs[p].get_monitor(MSG_MONITOR);

		// cache these ptrs for event system
		procs[p].msg_lock = &msg_mon->lock;
		procs[p].msg_signal = &msg_mon->signal;

		for (int t = 0; t < MAX_PROC_THREADS; t++) {
			procs[p].threads[t].runState = ThreadRunState::NULL;
			procs[p].thread_index = 0;

			// init file table
			for (int f = 0; f < MAX_PROC_FILES; f++) {
				procs[p].files[f] = nullptr;
			}
		}
	}

	// Current process is the kernel, proc_id 0
	num_procs = 1;
	proc_id = 0;

	thisProc = &procs[proc_id];
	thisProc->cr3 = (uint) get_physical((void *) page_dir);
	// Current thread is the kernel thread, index 0
	thisProc->thread_index = 0;

	thisThread = &thisProc->threads[thisProc->thread_index];
	//TODO put this in a Thread function:
	thisThread->runState = ThreadRunState::RUNNING;
	thisProc->num_running_threads = 1;

	thisProc->env = (Environment *) static_alloc_pages(1);
	thisProc->userEnv = thisProc->env;

	// `thisThreadState` is set upon entering an interrupt, not necessary here:
	//thisThreadState = &threads[0].cpuState;
}




Process *new_user_process() {

	int new_pid = 0;
	for (;new_pid < MAX_PROCS; new_pid++) {
		if (procs[new_pid].cr3 == 0) break;
	}
	if (new_pid == 0) {
		debug(0, "ERROR: new proc_id==0");
		SPINJMP();
		return nullptr;
	}
	if (new_pid == MAX_PROCS) {
		debug(0, "ERROR: max procs reached");
		SPINJMP();
		return nullptr;
	}
	num_procs++;

	PageMapEntry *new_pdir = (PageMapEntry *) virt_alloc_page();
	procs[new_pid].cr3 = (uint) get_physical(new_pdir);

	// Copy mappings of global memory (kernel, mem-mapped devices, etc)
	for (int i = 0; i < 1024; i++) {
		if ((*page_dir)[i].global) {
			new_pdir[i] = (*page_dir)[i];
		} else {
			new_pdir[i].val = 0;
		}
	}

	new_pdir[PDIR_SELF_INDEX].val = (uint) procs[new_pid].cr3 | PAGE_KERNEL_DATA;



	setPageDirectory(procs[new_pid].cr3);
	proc_id = new_pid;
	thisProc = &procs[proc_id];

	// copy stdout file ref from kernel, user prog can also override  locally:
	thisProc->files[FD_STDIN] = &nullfile;
	thisProc->files[FD_STDOUT] = procs[0].files[FD_STDOUT];
	thisProc->files[FD_STDERR] = procs[0].files[FD_STDERR];

	// TODO use a whole 4mb page table, set global flag so it gets shared
	thisProc->env = new (static_alloc_pages(1)) Environment();

	// this gets mapped on demand
	thisProc->userEnv = nullptr;

	return thisProc;
}

#pragma pop_macro("DEBUG_LEVEL")
