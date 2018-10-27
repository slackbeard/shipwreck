#include <devices/terminal.h>
#include <devices/vga.h>

File *terminal_device;
int MAX_PRINT_CHARS;
char *kprint_buffer;

void VGATerminal::render_char(char c) {
	// render a character at the cursor, updating the cursor
	cursor.wrap();
	
	int pixel_x = cursor.x * CHAR_SPACING_X;
	int pixel_y = cursor.y * CHAR_SPACING_Y;

	rect_fill({
		pixel_x,
		pixel_y,
		pixel_x + CHAR_SPACING_X, 
		pixel_y + CHAR_SPACING_Y
	}, BLACK);

	render_char_xy(c, pixel_x, pixel_y, terminal_color);

	cursor.x++;
	cursor.wrap();
}

void VGATerminal::render_newline() {
	cursor.wrap();
	for (int i = cursor.x; i < TERMINAL_WIDTH-1; i++) {
		render_char(' ');
	}
	
	cursor.x = 0;
	cursor.y++;

	uint old_cursor_y = cursor.y;
	ushort old_terminal_color = terminal_color;

	terminal_color = DARK_RED;

	for (int i = 1; i < TERMINAL_WIDTH; i++) {
		render_char('=');
	}
	terminal_color = old_terminal_color;
	cursor.x = 0;
	cursor.y = old_cursor_y;
	
	cursor.wrap();
}

int VGATerminal::render_text(const char *buffer, uint length) {
	int i = 0;
	for (; i < length; i++) {
		char c = buffer[i];

		if (c == 0) break;

		if (c == '\n') {
			render_newline();
			continue;
		}
		render_char(c);
	}	
	return i;
}

