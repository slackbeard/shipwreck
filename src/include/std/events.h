#pragma once

#include <std/types.h>

#define MSG_MONITOR 0
#define INPUT_MONITOR 1

enum class UserEvents {
	KEY_CHAR_DOWN=1,
	KEY_CHAR_UP,
	MOUSE_MOVE,
	MOUSE_LEFT_DOWN,
	MOUSE_LEFT_UP,
	MOUSE_LEFT_CLICK,
	MOUSE_LEFT_DRAG_START,
	MOUSE_LEFT_DRAG_END,
	MOUSE_RIGHT_DOWN,
	MOUSE_RIGHT_UP,
	MOUSE_RIGHT_CLICK,
	MOUSE_RIGHT_DRAG_START,
	MOUSE_RIGHT_DRAG_END,
	MAX
};

struct MouseEvent {
	int x:12;
	int y:12;
	uint lbutton:1;
	uint rbutton:1;
	operator int() {
		return *(int *) this;
	}
	MouseEvent() {
		x=0;
		y=0;
		lbutton=0;
		rbutton=0;
	}
		
};
