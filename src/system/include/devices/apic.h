#pragma once

#include <std/types.h>
#include <util/debug.h>
#include <std/string.h>


#define LAPIC_ADDRESS      0xFEE00000
#define IOAPIC_ADDRESS     0xFEC00000


// totally fake datatype: a regular 32-bit uint that takes up 128 bits
struct __attribute__((packed)) reg128 {
private:
	uint volatile i;
	char reserved[12];
public:
	// default ctor does nothing as this is usually volatile
	reg128() {}

	uint operator =(reg128 other) volatile {
		i = other.i;
		return i;
	}
			
	operator uint() const volatile { 
		return i;
	}

	reg128(uint d) {
		i = d;
	}

};

struct __attribute__((packed)) LAPIC {
	char reserved1[0x20];
	reg128 ID;
	reg128 version;
	char reserved2[0x40];
	reg128 TPR;
	reg128 APR;
	reg128 PPR;
	reg128 EOI;
	reg128 RRD;
	// logical destination register
	reg128 LDR;
	reg128 DFR;

	// spurious interrupt vector:
	reg128 SIVR;

	reg128 ISR[8];  // in-service register
	reg128 TMR[8];  // trigger-mode register

	reg128 IRR[8];  // interrupt request register
	reg128 error_status;

	char reserved3[0x60]; // 290 - 2E0 reserved

	reg128 CMCI;
	reg128 ICR[2];

	reg128 lvt_timer;
	reg128 lvt_thermal;
	// performance counter
	reg128 lvt_perf;

	reg128 lvt_lint0;
	reg128 lvt_lint1;

	reg128 lvt_error;

	reg128 timer_init;
	reg128 timer_count;

	char reserved4[0x40];

	reg128 timer_div;

	char reserved5[4];
	LAPIC() {
		// enable the APIC in the APIC base register (MSR 0x1B)
		uint volatile eax, ebx, ecx, edx;
		ecx = 0x1B;
		__asm__ volatile ("rdmsr":"=a" (eax), "=d" (edx): "c" (ecx));

		eax |= 0x800;
		__asm__ volatile ("wrmsr"::"a" (eax), "d" (edx), "c" (ecx));


		debug(9, "LAPIC Version = ", 
			(hex) version
		);
		debug(9, " lvt_timer @ ", (hex)&lvt_timer, " = ", (hex) lvt_timer);
		debug(9, " init @ ", (hex)&timer_init, " = ", (hex) timer_init);
		debug(9, " div @ ", (hex)&timer_div, " = ", (hex) timer_div);

		debug(9, " SIVR @ ", (hex)&SIVR);
		debug(9, " DFR @ ", (hex)&DFR);
		debug(9, " LDR @ ", (hex)&LDR);
		SIVR = (((uint)SIVR & 0xFFFFFF0F) | 0x100) + 0xF0;

		DFR = 0xFFFFFFFF;
		LDR = ((uint) LDR & 0x00FFFFFF) | 1;
		TPR = 0x0;

		lvt_error = 0xD0;

	}
	void set_timer_vector(uint vector) volatile {
		// set one-shot timer mode, handled by given IDT vector
		lvt_timer = ((uint)lvt_timer & 0xFFF8FF00) | 0x00020000 | vector;
	}

	void set_timer(int timeout) volatile {
		timer_init = timeout;
	}
	void send_eoi() volatile {
		EOI = 1;
	}

};

struct __attribute__((packed)) IOAPIC {
	// REGSEL 
	reg128 regsel;
	// IOWIN  
	reg128 iowin;

	void set_irq_vector(uint IRQ, uint vector) volatile {
		
		uint addr = IRQ * 2 + 0x10;
		this->regsel = addr;

		// lower 32bits
		this->iowin = (this->iowin & 0xFFFE0000) + vector;

		this->regsel = addr + 1;

		// upper 32bits
		this->iowin = (this->iowin & 0xFFFE0000) + 0x00;
	}

	IOAPIC() {
	}

};

void init_ioapic(); 

void init_lapic();

extern volatile LAPIC *lapic;

extern volatile IOAPIC *ioapic;
