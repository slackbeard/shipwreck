#include <util/debug.h>
#include <gui/shapes.h>
#include <devices/vga.h>

Rect screen_rect;

void clip_to_screen(int &x, int &y) {
	clamp_in_to_rect(x, y, &screen_rect);
}


void pset(int x, int y, uint col) {
	clip_to_screen(x, y);
	uint alpha = GET_ALPHA(col);

	if (alpha) {
		SET_PIXEL(x, y, fast_blend_factor(GET_PIXEL(x, y), col, alpha>>16));
	} else {
		SET_PIXEL(x, y, col);
	}
}

void row_line(int y, int left, int right, uint col) {
	for (int x = left; x < right; x++) {
		SET_PIXEL(x, y, col);
	}
}

void column_line(int x, int top, int bottom, uint col) {
	for (int y = top; y < bottom; y++) {
		SET_PIXEL(x, y, col);
	}
}
	
void rect_fill(Rect rect, uint col) {
	clip_to_screen(rect.l, rect.t);
	clip_to_screen(rect.r, rect.b);
	uint alpha = col & 0x00070000;

	if (alpha) {
		for (int y = rect.t; y < rect.b; y++) {
			for (int x = rect.l; x < rect.r; x++) {
				SET_PIXEL(x, y, fast_blend_factor(GET_PIXEL(x, y), col, alpha>>16));
			}
		}
	} else {
		for (int y = rect.t; y < rect.b; y++) {
			for (int x = rect.l; x < rect.r; x++) {
				SET_PIXEL(x, y, col);
			}
		}
	}
}

			
void rect_line(Rect rect, uint col) {
	clip_to_screen(rect.l, rect.t);
	clip_to_screen(rect.r, rect.b);

	
	row_line(rect.t, rect.l, rect.r, col);
	row_line(rect.b, rect.l, rect.r, col);
	column_line(rect.l, rect.t, rect.b, col);
	column_line(rect.r, rect.t, rect.b, col);
}
	

char *ASCII_MAP;
extern "C" void initTextRender() {
	screen_rect = {0, 0, (int) VGA_WIDTH, (int) VGA_HEIGHT};

	// Read in ASCII bitmap
	int int_vector = 0x43;
	uint *ivt = (uint *) 0;
	uint seg = ivt[int_vector] >> 16;
	uint offset = ivt[int_vector] & 0xFFFF;
	ASCII_MAP = (char *) ((seg << 4) + offset);
}


extern "C" int render_text_xy(const char *str, uint len, int x, int y, ushort color) {
	int i = 0;
	int pixel_x = x;
	int pixel_y = y;
	for (; i < len; i++) {
		char c = str[i];

		render_char_xy(c, pixel_x, pixel_y, color);
		pixel_x += CHAR_SPACING_X;
	}
	return i;
}

extern "C" void render_char_xy_clipped(char c, int x, int y, ushort color, Rect *clip) {
	Rect textrect = {0,0,CHAR_WIDTH,CHAR_HEIGHT};
	Rect cliprect={
		clip->l - x,
		clip->t - y,
		clip->r - x,
		clip->b - y
	};
	if (!rect_intersect(&cliprect, &textrect)) return;

	int map_index = (int) c * CHAR_HEIGHT;
	for (int cy = textrect.t; cy < textrect.b; cy++) {
		char bits = ASCII_MAP[map_index + cy] << textrect.l;
		for (int cx = textrect.l; cx < textrect.r; cx++) {
			if (bits & 0x80) SET_PIXEL(x + cx, y + cy, color);
			bits <<= 1;
		}
	}
}

extern "C" void render_char_xy(char c, int x, int y, ushort color) {
	render_char_xy_clipped(c, x, y, color, &screen_rect);
}

extern "C" int render_text_xy_clipped(const char *str, uint len, int x, int y, ushort color, Rect *clip) {
	int i = 0;
	int pixel_x = x;
	int pixel_y = y;
	for (; i < len; i++) {
		char c = str[i];

		render_char_xy_clipped(c, pixel_x, pixel_y, color, clip);
		pixel_x += CHAR_SPACING_X;
	}
	return i;
}


/***** bitmaps ******/
void get_pixels(int start_x, int start_y, int w, int h, void *dst) {

	auto buf = (ushort (*)[w][h]) dst;

	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {

			(*buf)[x][y] = GET_PIXEL(x+start_x, y+start_y);
		}
	}
}


void put_pixels(int start_x, int start_y, int w, int h, void *src) {

	auto buf = (ushort (*)[w][h]) src;

	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {
			if ((*buf)[x][y] != 1) {
				SET_PIXEL(x+start_x, y+start_y, (*buf)[x][y]);
			}
		}
	}
}
