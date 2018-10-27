#pragma once
#include <std/types.h>
#include <gui/shapes.h>
#include <std/file.h>

enum class WindowClass {
	NONE=0,
	WINDOW,
	WINDOW_FRAME,
	TEXT_BOX,
	MAX	
};


struct Window {
	WindowClass winclass;

	Rect rect={0,0,0,0};
	Rect redraw_rect={0,0,0,0}; // for user-requested redraws
	/*
	NOTE: using a union here so all `Window` objects are the same size, allowing us to allocate them from a single pool.
	*/
	union {
		uint color;
		void *display_data;
	};
	int zindex=0;
	int id=0; // local window ID in owner
	int parent_id=-1; // index of containing window, or -1 if this is the top

	// bit flags:
	uint needs_update:1;
	uint needs_redraw:1; // for user-requested redraws

	Window():
		needs_update(0),
		needs_redraw(0)
	{
		// default to an empty slot
		winclass = WindowClass::NONE;
	}
	Window(Rect newrect):
		rect(newrect),
		needs_update(1),
		needs_redraw(1),
		redraw_rect({0,0,newrect.r-newrect.l, newrect.b-newrect.t})
	{
		winclass = WindowClass::WINDOW;
	}
	void move(int dx, int dy) {
		needs_update=1;
		rect.l += dx;
		rect.r += dx;
		rect.t += dy;
		rect.b += dy;
	}
};

struct WindowFrame: public Window {
	using Window::Window;

	WindowFrame(Rect newrect): Window(newrect) {
		winclass = WindowClass::WINDOW_FRAME;
	}

	void addChild(Window *child) {
		if (child == nullptr) return;
		child->parent_id = id;
		child->needs_update = 1;
		needs_update = 1;
	}
	void removeChild(Window *child) {
		if (child == nullptr) return;
		child->parent_id = -1;
		child->needs_update = 1;
		needs_update = 1;
	}
};

struct TextBox: public Window {
	using Window::Window;

	template <int W=0, int H=0>
	struct Buffer {
		int char_w=W, char_h=H;
		int offset_x=0, offset_y=0;
		int head=0;
		int tail=0;
		int max=W * H;
		char text[W * H];
		Buffer() {
		}
	};
	TextBox(Rect newrect, Buffer<> *buf): Window(newrect) {
		winclass = WindowClass::TEXT_BOX;
		display_data = (void *) buf;
	}

	void backspace() {
		Buffer<> *buf = (Buffer<> *) display_data;
		if (buf->head >= buf->tail) {
			return;
		}
		int old_tail = buf->tail;
		buf->text[buf->tail % buf->max] = ' ';
		buf->tail--;
		buf->text[buf->tail % buf->max] = '_';
		set_redraw_rect(old_tail, buf->tail);
	}

	void set_redraw_rect(int old_cursor, int new_cursor) {
		if (old_cursor == new_cursor) return;
			
		Buffer<> *buf = (Buffer<> *) display_data;

		int new_cursor_x = (new_cursor % buf->char_w) * CHAR_SPACING_X + buf->offset_x;	
		int new_cursor_y = (new_cursor / buf->char_w) * CHAR_SPACING_Y + buf->offset_y;	

		int rect_w = rect.r - rect.l;
		int rect_h = rect.b - rect.t;
		if (new_cursor_y < 0) {
			buf->offset_y -= new_cursor_y;
			redraw_rect = {0,0,rect_w, rect_h};
			needs_redraw=1;
			return;
		}
			
		if ((new_cursor_y + CHAR_SPACING_Y) > rect_h) {
		// if cursor went beyond the bottom of the window, scroll
			buf->offset_y += (rect_h - (new_cursor_y + CHAR_SPACING_Y));
			redraw_rect = {0,0,rect_w, rect_h};
			needs_redraw=1;
			return;
		}
		
		int old_cursor_x = (old_cursor % buf->char_w) * CHAR_SPACING_X + buf->offset_x;	
		int old_cursor_y = (old_cursor / buf->char_w) * CHAR_SPACING_Y + buf->offset_y;	

		if (old_cursor_y < new_cursor_y) {
			redraw_rect.t = old_cursor_y;
			redraw_rect.b = new_cursor_y + CHAR_SPACING_Y;
		} else {
			redraw_rect.t = new_cursor_y;
			redraw_rect.b = old_cursor_y + CHAR_SPACING_Y;
		}


		if (old_cursor_y == new_cursor_y) {
			if (old_cursor_x < new_cursor_x) {
				redraw_rect.l = old_cursor_x;
				redraw_rect.r = new_cursor_x + CHAR_SPACING_X;
			} else {
				redraw_rect.l = new_cursor_x;
				redraw_rect.r = old_cursor_x + CHAR_SPACING_X;
			}
				
		} else {
			redraw_rect.l = 0;
			redraw_rect.r = rect.r - rect.l;
		}
		needs_redraw = 1;

	}
	int print(const char *buffer, uint len) {

		Buffer<> *buf = (Buffer<> *) display_data;

		char *ptr = (char *) buffer;
		if (len > buf->max) {
			ptr = &ptr[len - buf->max];
			len = buf->max;
		}

		bool scrolled=false;


		int old_tail = buf->tail;

		for (int i = 0; i < len; i++) {
			char c = ptr[i];
			if (c == '\n') {
				int line_rest = buf->char_w - (buf->tail % buf->char_w);
				//TODO: should keep track of where each line breaks, otherwise you have to backspace through all these blank spaces:
				for (int j = 0; j < line_rest; j++) {
					buf->text[buf->tail % buf->max] = ' ';
					buf->tail++;
				}
			} else {
				buf->text[buf->tail % buf->max] = c;
				buf->tail++;
			}
		}
		if ((buf->tail - buf->head) >= buf->max) {
			scrolled=true;
			buf->head += buf->char_w;
		}
		if ((buf->tail % buf->char_w) == 0) {
			scrolled=true;
			for (int i = 0; i <	buf->char_w; i++) {
				buf->text[(buf->tail + i) % buf->max] = ' ';
			}
		}

		buf->text[buf->tail % buf->max] = '_';

		set_redraw_rect(old_tail, buf->tail);

		return 1;	
	}
};

