#include <new>
#include <util/debug.h>
#include <memory.h>
#include <interrupts.h>
#include <devices/apic.h>
#include <devices/io.h>
#include <events.h>
#include <devices/vga.h>
#include <devices/ps2.h>

#define CLICK_BOX 4

void delay_for_io(uint count=0x10) {
	for (int i = 0; i < count; i++) {
		__asm__ volatile("nop");
	}
}	

void keyboard_packet(uchar scancode) {
	debug(9, "Key: ", (hex)scancode);
	char ascii = scan_to_ascii(scancode);
	if (ascii) {
		if (scancode & 0x80) {
			new_user_event((int) UserEvents::KEY_CHAR_UP, ascii);
		} else {
			new_user_event((int) UserEvents::KEY_CHAR_DOWN, ascii);
		}
	}
}


uchar mouse_buffer[3];
uint mouse_buffer_index;
uint num_mouse_packets;

static unsigned long long old_tsc;



MouseState mouse;

void mouse_packet(uchar data) {

	mouse_buffer[mouse_buffer_index] = data;
	mouse_buffer_index = (mouse_buffer_index + 1) % 3;

	if (++num_mouse_packets >= 3) {
		// if we have all 3 packets of mouse data:
		uchar b0 = mouse_buffer[mouse_buffer_index];
		uchar b1 = mouse_buffer[(mouse_buffer_index + 1) % 3];
		uchar b2 = data;

		// validate first packet to check alignment:
		if ((b0 & 0xC0) || !(b0 & 0x08)) {
			if (b0 & 0xC0) {
				debug(9, "ERROR: OVERFLOW SET");
			} else {
				debug(9, "ERROR: PACKETS NOT ALIGNED"); 
			}
			
			debug(9, "Mouse packets: ", (hex) b0, ", ", (hex) b1, ", ", (hex) b2);
			// setting num packets = 2 effectively shifts the data by 1 packet
			// so we try this again when we get the next packet
			num_mouse_packets = 2;
			return;
		}

		int deltaX, deltaY;
		deltaX = ((int) b1) - (((int) b0 << 4) & 0x100);
		deltaY = (((int) b0 << 3) & 0x100) - ((int) b2);

		mouse.x += deltaX;
		mouse.y += deltaY;

		if (mouse.x >= (int) VGA_WIDTH) mouse.x = VGA_WIDTH - 1;
		if (mouse.x < 0) mouse.x = 0;
		if (mouse.y >= (int) VGA_HEIGHT) mouse.y = VGA_HEIGHT - 1;
		if (mouse.y < 0) mouse.y = 0;

		unsigned long long tsc = __builtin_ia32_rdtsc();
		unsigned long long diff_tsc = tsc - old_tsc;

		MouseEvent m_event;
		m_event.x = mouse.x;
		m_event.y = mouse.y;

		// TODO maybe clicking logic doesn't belong in here
		if (b0 & 0x01) {
			m_event.lbutton = 1;

			rect_fill({650, 0, 660, 10}, WHITE);
			if (!mouse.lbutton) {
				
				new_user_event((int) UserEvents::MOUSE_LEFT_DOWN, (int) m_event);
				mouse.lbutton = 1;
				mouse.lclick_x = mouse.x;
				mouse.lclick_y = mouse.y;
			}
		} else { 

			rect_fill({650, 0, 660, 10}, BLACK);

			if (mouse.ldrag) {
				mouse.ldrag = 0;
				new_user_event((int) UserEvents::MOUSE_LEFT_DRAG_END, (int) m_event);
			}

			if (mouse.lbutton) {
				mouse.lbutton = 0;
				new_user_event((int) UserEvents::MOUSE_LEFT_UP, (int) m_event);
				uint dx = (mouse.x - mouse.lclick_x) + CLICK_BOX;
				uint dy = (mouse.y - mouse.lclick_y) + CLICK_BOX;
				if ((dx < (CLICK_BOX * 2)) && (dy < (CLICK_BOX * 2))) {
					m_event.x = mouse.lclick_x;
					m_event.y = mouse.lclick_y;
					new_user_event((int) UserEvents::MOUSE_LEFT_CLICK, (int) m_event);
					
				}

			} 
		}

		// TODO dispatch right mouse button events
		if (b0 & 0x02) {
			m_event.rbutton = 1;
			rect_fill({660, 0, 670, 10}, WHITE);
		}
		else {
			rect_fill({660, 0, 670, 10}, BLACK);
		}

		if (diff_tsc > 0x04000000) {
			old_tsc = tsc;
			if ((mouse.old_x != mouse.x) || (mouse.old_y != mouse.y)) {
				new_user_event((int) UserEvents::MOUSE_MOVE, (int) m_event);
				if (mouse.lbutton && !mouse.ldrag) {
					mouse.ldrag = 1;

					uint dx = (mouse.x - mouse.lclick_x) + CLICK_BOX;
					uint dy = (mouse.y - mouse.lclick_y) + CLICK_BOX;
					if ((dx >= (CLICK_BOX * 2)) || (dy >= (CLICK_BOX * 2))) {
						new_user_event((int) UserEvents::MOUSE_LEFT_DRAG_START, (int) m_event);
					}
				}
			}
		}

		// reset buffer:
		num_mouse_packets = 0;
		mouse_buffer_index = 0;
	}

}	
	
int ps2_input() {
	int i;
	uchar ps2_status;
	// NOTE: if this loop is too long, the ps2 handler will not return before the scheduler int
	for (i = 0; i < 0x10; i++) {
		ps2_status = inb(0x64);
		if (ps2_status & 0x01) break;
	}
	if (i == 0x10) return 0;

	for (i = 0; i < 0x10; i++) {
		if (!(ps2_status & 0x01)) break;

		// wait
		for (int i = 0; i < 0x10; i++) {
			__asm__ volatile("nop");
		}

		uchar ps2_data = inb(0x60);
		if (ps2_status & 0x20)  {
			mouse_packet(ps2_data);
		} else {
			keyboard_packet(ps2_data);
		}


		// wait
		for (int i = 0; i < 0x10; i++) {
			__asm__ volatile("nop");
		}
		ps2_status = inb(0x64);
	}
	return 0;

}
	
int waitKBread() {
	delay_for_io();
	for (int i = 0; i < 0x10000; i++) {
		if (inb(0x64) & 0x01) return 1;
	}
	delay_for_io();
	return 0;
}


int waitKBwrite() {
	delay_for_io();
	for (int i = 0; i < 0x10000; i++) {
		if (!(inb(0x64) & 0x02)) return 1;
	}
	delay_for_io();
	return 0;
}

int waitKBdata() {
	delay_for_io();
	for (int i = 0; i < 0x10000; i++) {
		if (!(inb(0x64) & 0x20)) return 1;
	}
	delay_for_io();
	return 0;
}

int sendKBCmd(uchar cmd) {
	if (!waitKBwrite()) {
		debug(9, "Error in sendKBCmd() : Keyboard timed out!!!\n");
		return 0;
	}
	outb(0x64, cmd);
	return 1;
}

uchar readKB() {
	waitKBread();
	return inb(0x60);
}

int sendKBData(uchar data) {
	if (!waitKBdata()) {
		debug(9, "Error in sendKBData() : waitKBdata timed out!!\n");
		return 0;
	}

	// read and discard
	inb(0x60);

	if (!waitKBwrite()) {
		debug(9, "Error in sendKBData() : waitKBwrite timed out!!\n");
		return 0;
	}
	outb(0x60, data);
	waitKBwrite();
	for (int i = 0; i < 3; i++) {
		waitKBread();
	}
	return 1;
}

int confirmKB() {
	uchar response = readKB();
	if (response != 0xFA) {
		debug(5, "RESPONSE != ACK: ", (hex) response);
		return 0;
	}
	debug(9, "RESPONSE = ACK");
	return 1;
}

int init_ps2(int width, int height) {

	idt->table[KEYBOARD_IDT].set_handler( (void *) keyboard_interrupt );
	idt->table[MOUSE_IDT].set_handler( (void *) mouse_interrupt );

	ioapic->set_irq_vector(KEYBOARD_IRQ, KEYBOARD_IDT);
	ioapic->set_irq_vector(MOUSE_IRQ, MOUSE_IDT);

	mouse_buffer_index = 0;
	num_mouse_packets = 0;
	old_tsc = __builtin_ia32_rdtsc();



	// Disable mouse interrupt (should already be disabled)
	sendKBCmd(0xD4);
	sendKBData(0xF5);
	confirmKB();

	// reset mouse
	sendKBCmd(0xD4);
	sendKBData(0xFF);
	confirmKB();
	if (readKB() != 0xAA){
		debug(0, "Mouse failed its BAC test\n");
		return 0;
	}

	// enable secondary PS/2 device (mouse)
	sendKBCmd(0xA8);
	// doesn't generate ACK but read just in case
	readKB();

	// read controller configuration byte
	sendKBCmd(0x20);
	uchar cmdbyte = readKB();

	// set bits 0-1 (enable PS/2 interrupts) in controller configuration byte
	// clear "mouse clock disable" bit (bit 5) 
	sendKBCmd(0x60);
	cmdbyte = (cmdbyte | 0x03) & 0xEF;
	sendKBData(cmdbyte);
	confirmKB();	

	// Enable mouse interrupts
	sendKBCmd(0xD4);
	sendKBData(0xF4);
	// doesn't generate ACK but read just in case
	readKB();

	debug(9, "MOUSE ENABLED");
	return 1;
}

	
INTERRUPT_DEFINITION(mouse_interrupt) {
	ps2_input();
	lapic->send_eoi();
}


INTERRUPT_DEFINITION(keyboard_interrupt) {
	ps2_input();
	lapic->send_eoi();
}

