#include <std/types.h>
#include <devices/cpu.h>
#include <threads.h>
#include <process.h>
#include <memory.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 1


Thread *thisThread;


void ThreadState::dump() {
	debug(1, " Thread state @ ", (hex) this);
	debug(1, " eax=",(hex)eax," ecx=",(hex)ecx," edx=",(hex)edx, " ebx=", (hex) ebx);
	debug(1, "  esp=",(hex)esp," ebp=",(hex)ebp," esi=",(hex)esi," edi=",(hex)edi);
}

void Thread::initCPUState() {
	// Just recalculates `cpuState` to point at a ThreadState object at the end of the stack
	this->cpuState = (ThreadState *) ((uint) stack + stack_bytes - sizeof(ThreadState));
}
void Thread::setStack(void *stack, uint stack_bytes) {
	this->stack = stack;
	this->stack_bytes = stack_bytes;
}

void Thread::setFunction(void *function) {
	InterruptParams *intParams = (InterruptParams *) &this->cpuState->intArgs;
	intParams->eip = (uint) function;
}


void KernelThread::init(void *function, void *stack, uint stack_bytes) {
	setStack(stack, stack_bytes);
	initCPUState();

	InterruptParams *intParams = (InterruptParams *) &this->cpuState->intArgs;
	// TODO values for CS and DS should be #define's or something
	intParams->cs = 0x08;
	intParams->eip = (uint) function;
	intParams->eflags = 0x202;

	// no need to set the stack pointer, kernel threads continue using the ESP value after the `iret`
}


void UserThread::init(void *function, void *stack, uint stack_bytes) {
	setStack(stack, stack_bytes);
	initCPUState();

	InterruptParams *intParams = (InterruptParams *) &this->cpuState->intArgs;
	// TODO value for user CS (0x1B) should be a #define or something
	intParams->cs = 0x1B;
	intParams->eip = (uint) function;
	intParams->eflags = 0x202;

	// set stack pointer at a safe buffer below cpuState, though intArgs[32] accomodates for extra int args (eg. page fault code) 

	intParams->ss3 = 0x23;
	// TODO remove safe buffer if not needed? 
	intParams->esp3 = (uint) this->cpuState - 32;
}

void Thread::dump() {
	debug(1, "Thread @ ", (hex) this);
	debug(1, "  stack=", (hex) stack, ", stack_bytes=", (hex) stack_bytes);
	debug(1, "  runState=", (hex) runState);

	debug(1, "CPU State: ");
	if (cpuState != nullptr) {
		cpuState->dump();
	} else {
		debug(1, "NULL");
	}
}


UserThread *new_user_thread(void *function, int data, uint stack_bytes, void *stack) {

	for (int i = 0; i < MAX_PROC_THREADS; i++) {
		if (thisProc->threads[i].runState == ThreadRunState::NULL) {
			UserThread *newThread = &thisProc->threads[i];

			if (stack_bytes == 0) stack_bytes = 16 * 4096;
			uint stack_pages = (stack_bytes + 4095) / sizeof(PageFrame);
			if (stack == nullptr) {
				// NOTE if we had called virt_alloc_page before here, we'd have to release the memory if new_user_thread failed.
				stack = virt_alloc_pages(stack_pages);
			}

			newThread->init((void *) function, stack, stack_bytes);
			uint syscall_stack_pages = 16;
			newThread->syscall_stack_bytes = syscall_stack_pages * sizeof(PageFrame);
			newThread->syscall_stack = virt_alloc_pages(syscall_stack_pages, PAGE_KERNEL_DATA);
			newThread->syscall_esp = (uint) newThread->syscall_stack + newThread->syscall_stack_bytes;

			// TODO move to ctor
			newThread->lock_next = nullptr;
			newThread->signal_wait = nullptr;

			newThread->runState = ThreadRunState::RUNNING;
			thisProc->num_running_threads++;
			return newThread;
		}
	}
	return nullptr;
}

void yield() {
	sti();
	do {
		debug(9, "Y");
		__asm__ volatile("hlt");
	} while (thisThread->runState != RUNNING);
}

#pragma pop_macro("DEBUG_LEVEL")
