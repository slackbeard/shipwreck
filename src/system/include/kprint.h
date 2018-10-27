#pragma once

#include <std/types.h>
#include <std/string.h>
#include <std/file.h>

extern File *terminal_device;

#define MAX_PRINT_CHARS 64

template <typename F, typename... R>
static int kprint(F first, R... rest) {
	char print_buffer[MAX_PRINT_CHARS];
	
	char *tmp_str = print_buffer; 

	int max_chars = MAX_PRINT_CHARS;
	int len = sprint(tmp_str, max_chars, first, rest...);
	// add null terminator
	print_buffer[len] = 0;
	if (len > 0) {
		return terminal_device->write(print_buffer, len);
	}
	return len;
}


static int kprintln() {
	return kprint("\n");
}

template <typename F, typename... R>
static int kprintln(F first, R... rest) {
	return kprint(first, rest..., "\n");
}

