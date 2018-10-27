#pragma once

typedef char * pchar;
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;

typedef __SIZE_TYPE__ size_t;

struct hex { 
	int i; 
	template <typename T>
	hex(T i) {
		this->i = (int) i;
	}
	operator int() {
		return i;
	}
	operator uint() {
		return (uint)i;
	}
};

