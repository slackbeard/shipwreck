#pragma once

#include <std/types.h>

#define MAX_PROC_FILES   16

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2


struct File {

	virtual int seek(int offset) {
		//similar to fseek(file, offset, SEEK_SET) 
		return -1;
	}
	virtual int read(char *buffer, uint length) {
		//similar to fread(buffer, 1, length, file)
		return -1;
	}
	virtual int write(const char *buffer, uint length) {
		return -1;
	}
};

