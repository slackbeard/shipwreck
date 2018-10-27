#pragma once

#include <std/types.h>
#include <util/debug.h>
#include <threads.h>


extern void *interrupt_stack;

#define INTERRUPT_DECLARATION(NAME) void __attribute__((naked)) NAME() 
#define INTERRUPT_DEFINITION(NAME) \
void __attribute__((naked)) NAME() {\
	__asm__ volatile(\
		"pushal\n"\
		"movw $0x10, %dx\n"\
		"movw %dx, %ds\n"\
		"movw %dx, %es\n"\
	);\
	__asm__ volatile(\
		"movl %%esp, %[thisThreadState]\n"\
		/* TODO: this resets esp to interrupt_stack every time, so if ints were to nest they'd corrupt the stack.*/\
		"movl %[interruptStack], %%esp\n"\
		"calll " #NAME "_inner\n"\
	::[thisThreadState] "m"(thisThread->cpuState), [interruptStack] "m" (interrupt_stack));\
	/* Separate asm statement to force `thisThreadState` to be reloaded:*/\
	__asm__ volatile(\
		/* `thisThreadState` may have been modified inside the handler function above */\
		"movl %[thisThreadState], %%esp\n"\
		"cmpl $0x1b, 0x24(%%esp)\n"\
		"jne over_fix_ds_" #NAME "\n"\
		"movw $0x23, %%dx\n"\
		"movw %%dx, %%ds\n"\
		"movw %%dx, %%es\n"\
		"over_fix_ds_" #NAME ":\n"\
		"popal\n"\
		"iret\n"\
	:: [thisThreadState] "m" (thisThread->cpuState));\
}\
extern "C" void NAME ## _inner()



struct InterruptDescriptor {
	ushort volatile low_offset;
	ushort volatile code_segment;
	uchar volatile zero;
	uchar volatile type;
	ushort volatile high_offset;

	InterruptDescriptor(): 
		low_offset(0),
		code_segment(0),
		zero(0),
		type(0),
		high_offset(0)
	{
	}

	void set_address(void *addr) volatile {
		low_offset = (ushort) ((uint) addr & 0xFFFF);
		high_offset = (ushort) ((uint) addr >> 16);
	}

	void set_handler(void *address, uint cs=0x08) volatile {
		// interrupt gate type, present and in ring 0
		type = 0b10001110;

		this->code_segment = cs;

		set_address(address);
	}
};

struct IDT {
	InterruptDescriptor table[256];
	IDT() { }
};

struct __attribute__((packed)) IDTR {
	ushort limit:16;
	IDT *base_address;
};

void init_interrupts();

// global IDT is defined in kernel.cpp
extern IDT *idt;

