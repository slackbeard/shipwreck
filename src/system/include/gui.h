#pragma once

#include <devices/vga.h>
#include <util/debug.h>
#include <std/types.h>
#include <process.h>
#include <memory.h>
#include <gui/objects.h>

#define POINTER_WIDTH 16
#define POINTER_HEIGHT 16

#define MAX_WINDOWS 256
#define MAX_WINDOW_LEVELS 256

struct GUIWindow;

struct GUIWindowStack {
	GUIWindow *top;
	GUIWindow *insert(GUIWindow *new_win);
	GUIWindow *push(GUIWindow *new_win);
	GUIWindow *detach(GUIWindow *win);
};


struct GUIWindowFrame;

struct GUIWindow {
	Window windata;	

	int pid=0; // owner process ID

	GUIWindow *next=nullptr;  // next lowest z-index
	GUIWindow *prev=nullptr;  // next highest z-index

	GUIWindowFrame *parent=nullptr; // container of this window, if any
	GUIWindow *wm_root=nullptr; // top-level container of this window


	void dump();

	GUIWindow(Rect newrect): windata(newrect){}
	GUIWindow(){}


	virtual void update_root(GUIWindow *new_root);
	virtual void draw(Rect *clip);
	virtual void move(int dx, int dy);
};

struct GUITextBox: public GUIWindow {
	using GUIWindow::GUIWindow;

	virtual void draw(Rect *clip);
};

struct GUIDesktop: public GUIWindow {
	using GUIWindow::GUIWindow;

	virtual void draw(Rect *clip);
};

struct GUIWindowFrame: public GUIWindow {
	using GUIWindow::GUIWindow;

	GUIWindowStack *children() {
		return (GUIWindowStack *) &windata.display_data;
	}

	virtual void draw(Rect *clip);
	virtual void move(int dx, int dy);
	virtual void update_root(GUIWindow *new_root);

	void addChild(GUIWindow *child);
	void removeChild(GUIWindow *child);
};

static void dump_rect(Rect *rect) {
	kprint("[", rect->l, ",", rect->t, ",", rect->r, ",", rect->b, "]");
}


static void dump_winstack(GUIWindowStack *winstack) {
	GUIWindow *ptr=winstack->top;
	for (int i = 0;
		i < 10 && ptr;
		i++, ptr=ptr->next)
	{

		kprintln(" w[",i,"]@",(hex) ptr," pid=", ptr->pid," id=", ptr->windata.id);
	}
}

struct WindowManager {

	GUIWindowStack winstack;
	GUIWindow *desktop;

	GUIWindow *drag_window=nullptr;

	uchar *pick_buffer;

	GUIWindow win_index[MAX_WINDOWS];

	WindowManager() {

		uint pick_buf_pages = (VGA_WIDTH * VGA_HEIGHT + 4095) / 4096;
		pick_buffer = (uchar *) static_alloc_pages(pick_buf_pages);

	}
	GUIWindow *getGUIWindow(int index) {
		if (index < 0 || index >= MAX_WINDOWS) return nullptr;
		return &win_index[index];
	}

};

struct MousePointer {
	// location of background image
	uint bg_x=0, bg_y=0;
	ushort image[POINTER_WIDTH][POINTER_HEIGHT];
	ushort background[POINTER_WIDTH][POINTER_HEIGHT];
};

extern MousePointer *mousePointer;

extern WindowManager *winmgr;

void update_pick_buf(Rect *clip, GUIWindow *win);

void gui_redraw_rect(Rect *clip, GUIWindow *top);

void gui_update_proc(int pid);

void gui_redraw_proc(int pid);

int load_background_tga(ushort *img_data);

void init_gui(int width, int height);
