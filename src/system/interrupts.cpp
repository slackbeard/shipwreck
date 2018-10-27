#include <new>
#include <util/debug.h>
#include <interrupts.h>

void *interrupt_stack;

IDT *idt;

INTERRUPT_DEFINITION(unhandled_exception)
{
	debug(0, "UNCAUGHT EXCEPTION");
	thisThread->cpuState->dump();
	SPINJMP();
};

INTERRUPT_DEFINITION(divide_zero_interrupt)
{
	debug(0, "DIVIDE BY ZERO INTERRUPT");
	SPINJMP();
};


INTERRUPT_DEFINITION(debug_interrupt)
{
	debug(0, "DEBUG INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(nmi_interrupt)
{
	debug(0, "NON MASKABLE INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(breakpoint_interrupt)
{
	debug(0, "BREAKPOINT INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(overflow_interrupt)
{
	debug(0, "OVERFLOW INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(bounds_interrupt)
{
	debug(0, "BOUNDS INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(invalid_opcode_interrupt)
{
	debug(0, "INVALID OPCODE");
	thisThread->cpuState->intParams.dump();	
	thisThread->cpuState->dump();
	SPINJMP();
};

INTERRUPT_DEFINITION(coprocessor_interrupt)
{
	debug(0, "CO PROCESSOR NOT AVAILABLE INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(doublefault_interrupt)
{
	debug(0, "DOUBLE FAULT INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(coprocessor_overrun_interrupt)
{
	debug(0, "COPROCESSOR OVERRUN INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(tss_interrupt)
{
	debug(0, "INVALID TSS INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(segment_interrupt)
{
	debug(0, "SEGMENT NOT PRESENT INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(stack_interrupt)
{
	debug(0, "STACK SEGMENT FAULT INTERRUPT");
	SPINJMP();
};

INTERRUPT_DEFINITION(gpf_interrupt)
{
	debug(0, "GENERAL PROTECTION INTERRUPT");
	thisThread->cpuState->intParams.dump();	
	thisThread->cpuState->dump();
	SPINJMP();
};



void init_interrupts() {
	// virtual 0x500 points to physical 0x500, a free chunk of memory for the IDT
	idt = new ((void *) 0x500) IDT();

	IDTR idtr;
	idtr.base_address = idt;
	idtr.limit = sizeof(idt->table) - 1;

	uint num_interrupts = (sizeof(idt->table) / sizeof(InterruptDescriptor));

	for (int i = 0; i < num_interrupts; i++) {
		idt->table[i].set_handler((void *) unhandled_exception);
	}

	idt->table[0x00].set_handler((void *) divide_zero_interrupt);
	idt->table[0x01].set_handler((void *) debug_interrupt);
	idt->table[0x02].set_handler((void *) nmi_interrupt);
	idt->table[0x03].set_handler((void *) breakpoint_interrupt);
	idt->table[0x04].set_handler((void *) overflow_interrupt);
	idt->table[0x05].set_handler((void *) bounds_interrupt);
	idt->table[0x06].set_handler((void *) invalid_opcode_interrupt);
	idt->table[0x07].set_handler((void *) coprocessor_interrupt);
	idt->table[0x08].set_handler((void *) doublefault_interrupt);
	idt->table[0x09].set_handler((void *) coprocessor_overrun_interrupt);
	idt->table[0x0A].set_handler((void *) tss_interrupt);
	idt->table[0x0B].set_handler((void *) segment_interrupt);
	idt->table[0x0C].set_handler((void *) stack_interrupt);
	idt->table[0x0D].set_handler((void *) gpf_interrupt);


	__asm__ volatile("lidt %0"::"p" (idtr));
	sti();

}
