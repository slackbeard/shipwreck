#pragma once
#include <std/types.h>
#include <gui/shapes.h>
#include <gui/colors.h>

// This is a static location for VGA video memory.
// - VGA video memory is usually located at physical address 0xE0000000, but if it isn't we can always map it to that spot in virtual memory, and thus always use this address to access it:
#define VGA_FRAMEBUFFER_ADDRESS 0xE0000000



#define TERMINAL_WIDTH  80
#define TERMINAL_HEIGHT 25


#define VGA_PIXEL_OFFSET(X, Y) (int)( (Y) * VGA_WIDTH + (X) )

#define SET_PIXEL(X, Y, C) (((ushort *) VGA_FRAMEBUFFER_ADDRESS)[VGA_PIXEL_OFFSET(X,Y)]=(ushort)(C))

#define GET_PIXEL(X, Y) (((ushort *) VGA_FRAMEBUFFER_ADDRESS)[VGA_PIXEL_OFFSET(X,Y)])

extern uint VGA_WIDTH, VGA_HEIGHT;

//extern ushort terminal_color;
//extern int cursor_x, cursor_y;
extern Rect screen_rect;
extern char *ASCII_MAP;


void clip_to_screen(int &x, int &y);


/* ---------------------- DRAWING ---------------------- */
void pset(int x, int y, uint col=WHITE);

void row_line(int y, int left, int right, uint col=WHITE);

void column_line(int x, int top, int bottom, uint col=WHITE);
	
void rect_fill(Rect rect, uint col=WHITE);

			
void rect_line(Rect rect, uint col=WHITE);
	

// this must be "extern" because it gets called by kernel_stub.nasm
extern "C" void initTextRender();

extern "C" void render_char_xy(char c, int x, int y, ushort color=DARK_GREEN);

extern "C" int render_text_xy(const char *str, uint len, int x, int y, ushort color=DARK_GREEN);

extern "C" void render_char_xy_clipped(char c, int x, int y, ushort color, Rect *clip);

extern "C" int render_text_xy_clipped(const char *str, uint len, int x, int y, ushort color, Rect *clip);


void get_pixels(int start_x, int start_y, int w, int h, void *dst);
void put_pixels(int start_x, int start_y, int w, int h, void *src);
