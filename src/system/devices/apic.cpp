#include <new>
#include <devices/apic.h>
#include <devices/cpu.h>

volatile LAPIC *lapic;

void init_lapic() {
	lapic = new ((void *) LAPIC_ADDRESS) LAPIC();

}

volatile IOAPIC *ioapic;

void init_ioapic() {
	
	ioapic = new ((void *) IOAPIC_ADDRESS) IOAPIC();

	ioapic->regsel = 0x01;
	debug(9, " IOAPIC Version = ",(hex) ioapic->iowin);

}
