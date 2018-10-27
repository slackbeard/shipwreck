#pragma once

#include <std/file.h>
#include <devices/vga.h>

struct TerminalCursor {
	uint x, y;
	void wrap() {
		// wrap cursor around if x > lines
		y += (x / TERMINAL_WIDTH);
		x = x % TERMINAL_WIDTH;
		
		y = y % TERMINAL_HEIGHT;
	}
};

struct VGATerminal: public File {
	ushort terminal_color;
	TerminalCursor cursor;

	void init() {
		terminal_color = DARK_GREEN;
		cursor.x = cursor.y = 0;
	}

	void render_char(char c);

	void render_newline();

	int render_text(const char *buffer, uint length);

	virtual int seek(uint offset) {
		// not implemented for streams
		return -1;
	}
	virtual int read(char *buffer, uint length) {
		// not implemented yet - later should read from keyboard buffer 	
		return -1;
	}
	virtual int write(const char *buffer, uint length) {
		return render_text(buffer, length);
	}

};
