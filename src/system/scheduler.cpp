#include <interrupts.h>
#include <scheduler.h>
#include <events.h>
#include <locks.h>
#include <threads.h>
#include <process.h>
#include <devices/apic.h>
#include <devices/vga.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 0

#define TIMER_IDT 0xE0

static int get_next_proc() {

	// check for kernel messages first
	procs[0].notify(MSG_MONITOR, 0);


	for (int i = 1; i < num_procs; i++) {
		int next_proc_id = (proc_id + i) % num_procs;
		Process *proc = &procs[next_proc_id];


		// if there's a message thread and there are messages, unblock the waiting thread
		proc->notify(MSG_MONITOR, 0);
		/*
		Alternatively, in case the semaphore drifts from the value its tracking (# of msgs) we could adjust for the error: 

		int diff = proc->env->msgQueue.size() - *proc->msg_signal;
		proc->notify(MSG_MONITOR, diff);
		*/


		debug(8, "    get_next_proc pid=", (hex) next_proc_id, " msgs=", (hex) *proc->msg_signal);
		debug(8, "    running_threads=",(hex) proc->num_running_threads);

		if (proc->num_running_threads > 0) return next_proc_id;
	}


	// if no process has running threads, use the kernel process (procs[0])
	// which contains the system idle thread (procs[0].threads[0])
	return 0;
}

static int set_proc(int next_proc_id) {
	int prev_proc_id = proc_id;

	proc_id = next_proc_id;

	thisProc = &procs[proc_id];

	if (proc_id != 0) {
		// context switch:

		uint old_cr3;
		__asm__ volatile(
			"movl %%cr3, %[old_cr3]"
		:[old_cr3] "=r" (old_cr3));

		// only set new cr3 if its different than current value
		if (old_cr3 != thisProc->cr3) {
			__asm__ volatile(
				"movl %[newProcCR3], %%cr3"
			::[newProcCR3] "r" (thisProc->cr3));
		}

	}

	return prev_proc_id;
}

static int get_next_thread() {

	// switch to next thread, round-robin
	int index_start = 1 + thisProc->thread_index;
	int index_end = index_start + MAX_PROC_THREADS;

	for (int i = index_start; i < index_end; i++) {
		int next_index = i % MAX_PROC_THREADS;
		Thread *nextThread = &thisProc->threads[next_index];

		if (nextThread->runState == RUNNING) {

			return next_index;

		}


	}

	// if we cannot find a running thread, just stick with the current thread (which would be in a WAITING loop from a call to yield() )
	thisProc->num_running_threads = 0;

	// if we're in the system thread, return the system idle thread id (0):
	if (proc_id == 0) return 0;

	return thisProc->thread_index;
}

static int set_thread(int next_index) {

	int prev_index = thisProc->thread_index;

	thisProc->thread_index = next_index;

	thisThread = &thisProc->threads[next_index];

	//point ESP0 at end of thread stack
	tss->esp0 = (uint) thisThread->stack + thisThread->stack_bytes;

	return prev_index;
}

static unsigned long long old_tsc;

void kprint_process_tag() {
	// print PID, thread ID and RTC delta at the top right of the screen

	unsigned long long tsc = __builtin_ia32_rdtsc();
	unsigned long long diff_tsc = tsc - old_tsc;
	old_tsc = tsc;

	int tmp_buf_size = 64;
	char tmp_buf[tmp_buf_size];
	char *tmp_str = tmp_buf;
	int len = sprint(tmp_str, tmp_buf_size, "P(", (int) proc_id, ":", (int) thisProc->thread_index, ":DT=", (hex)(uint) diff_tsc, ")");
	tmp_buf[len] = 0;
	rect_fill({850, 30, 850 + len * CHAR_SPACING_X, 30 + CHAR_SPACING_Y}, BLACK);
	render_text_xy(tmp_buf, len, 850, 30, GREY);
}

INTERRUPT_DEFINITION(timer_interrupt) {
	lapic->send_eoi();

// TODO thread timers for sleep() and WAIT timeouts

	int pid = get_next_proc();

	set_proc(pid);
	int tid = get_next_thread();

	set_thread(tid);

	kprint_process_tag();
}

void init_scheduler() {
	old_tsc = __builtin_ia32_rdtsc();

	idt->table[TIMER_IDT].set_handler( (void *) timer_interrupt );

	lapic->set_timer_vector(TIMER_IDT);
}

#pragma pop_macro("DEBUG_LEVEL")
