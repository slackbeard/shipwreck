#pragma once 

#include <std/types.h>
#include <std/events.h>
#include <threads.h>
#include <util/debug.h>
#include <process.h>
#include <gui.h>

#define MAX_EVENTS 16
#define MAX_WORKERS 8
#define MAX_SUBSCRIPTIONS 64


enum class DisplayEvents {
	REDRAW_SCREEN=1,
	REDRAW_MOUSE,
	REDRAW_WINDOW,
	REDRAW_RECT,
	MAX
};


struct DisplayMsg {
	DisplayEvents event;
	union {
		Rect update_rect;
		GUIWindow *redraw_win;
	};
	void dump() {
		debug_print("DispMsg:event=", (hex) event, " win=", (hex) redraw_win);
	}
};
void new_user_event(int msg_id, int data);
void new_callback_event(void (*handler)(int), int data);
void new_display_msg(DisplayMsg msg);

int sub_proc_event(Process *proc, UserEvents event);

void enter_kernel_worker(); 

void init_events();

extern KernelThread *kernelEvents;
