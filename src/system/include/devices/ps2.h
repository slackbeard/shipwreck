#pragma once 
#include <interrupts.h>

#define KEYBOARD_IRQ 1
#define MOUSE_IRQ    12

#define KEYBOARD_IDT 0x20
#define MOUSE_IDT    0x21

#define KEY_PAGE_UP      0x49
#define KEY_ARROW_UP     0x48
#define KEY_ARROW_LEFT   0x4B
#define KEY_ARROW_RIGHT  0x4D
#define KEY_ARROW_DOWN   0x50
#define KEY_PAGE_DOWN    0x51

static char scan_to_ascii(uchar scancode) {
/* the following lookup table is based on Bran's Kernel Development Tutorial: http://www.osdever.net/bkerndev/Docs/keyboard.htm */
	static uchar keymap[] =
	{
		0,  '\x1b'/* esc */, '1', '2', '3', '4', '5', '6', '7', '8',	
	  '9', '0', '-', '=', '\b', /* backspace */
	  '\t',			/* Tab */
	  'q', 'w', 'e', 'r',
	  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
		0,			/* 29   - Control */
	  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	 '\'', '`',   0,		/* Left shift */
	 '\\', 'z', 'x', 'c', 'v', 'b', 'n',
	  'm', ',', '.', '/',   0,				/* Right shift */
	  '*',
		0,	/* Alt */
	  ' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,   0,   0,   0,   0,   0,   0,   0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		0,	/* Up Arrow */
		0,	/* Page Up */
	  '-',
		0,	/* Left Arrow */
		0,
		0,	/* Right Arrow */
	  '+'
	};

	static const uchar *shift_num_row = (const uchar *) "~!@#$%^&*()_+";

	static bool extended = false;
	static bool shift = false;
	if (scancode == 0xE0) {
		extended = true;
		return 0;
	}
	if (extended) {
	// for extended codes (KEY_PAGE_UP, etc) use scancode itself
		return scancode;
	}
	// if (left or right) shift key pressed or released, or caps lock pressed
	if (scancode == 0x2A || scancode == 0x36
		|| scancode == 0xAA || scancode == 0xB6
		|| scancode == 0x3A) {
		shift = !shift;
	}
	
	uchar ascii;

	if (scancode >= 0 && scancode < sizeof(keymap)) {
		ascii = keymap[scancode];
	} else {
		ascii = 0;
	}
	if (shift) {
		if (ascii >= 'a' && ascii <= 'z') {
			ascii = ascii - 'a' + 'A';
		} else if (scancode >= '`' && scancode <= '=') {
			ascii = shift_num_row[scancode - '`'];
		}
	}
	return ascii;
}

struct MouseState {
	int x, y;
	int old_x, old_y;
	int lclick_x, lclick_y;
	struct {
		uint lbutton:1;
		uint ldrag:1;
	};

	MouseState() {
		lbutton = 0;
		lclick_x = 0;
		lclick_y = 0;
	}

};
extern MouseState mouse;

INTERRUPT_DECLARATION(mouse_interrupt);
INTERRUPT_DECLARATION(keyboard_interrupt);
int init_ps2(int w, int h);
