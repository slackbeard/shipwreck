#pragma once

#include <std/types.h>
#include <std/string.h>
#include <std/file.h>
#include <system.h>

#define MAX_PRINT_CHARS 64

template <typename F, typename... R>
static int print(F first, R... rest) {

	char print_buffer[MAX_PRINT_CHARS];
	
	char *tmp_str = print_buffer; 
	int max_chars = MAX_PRINT_CHARS;
	int len = sprint(tmp_str, max_chars, first, rest...);
	// add null terminator
	print_buffer[len] = 0;

	if (len > 0) sysapi::write(FD_STDOUT, print_buffer, len);

	return len;
}


static int println() {
	return print("\n");
}

template <typename F, typename... R>
static int println(F first, R... rest) {
	return print(first, rest..., "\n");
}

static int getline(char *dst, int max) {
	// read `max` chars into `dst`
	// return num of chars read
	return sysapi::read(FD_STDIN, dst, max);
}
