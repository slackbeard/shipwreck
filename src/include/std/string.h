#pragma once

#include <std/types.h>

extern "C" int strnlen(const char *s, size_t len); 

extern "C" int strncmp(const char *a, const char *b, size_t len ); 

extern "C" void *memcpy(void *dst, void *src, size_t num);

extern "C" void *memset(void *buffer, int value, size_t num_chars); 


/** String print functions **/

static int sprint(pchar &dst, int &len, char c) {
	if (len <= 0) return 0;
	
	dst[0] = c;
	dst = &dst[1];
	len--;
	return 1;
}

static int sprint(pchar &dst, int &len, uchar c) {
	return sprint(dst, len, (char)c);
}

static int sprint(pchar &dst, int &len, const char *str) {
	if (len <= 0) return 0;
	int i = 0;
	for (; str[i] && (i < len); i++) {
		dst[i] = str[i];
	}
	len -= i;
	dst = &dst[i];
	return i;
}

static int sprint(pchar &dst, int &len, char *str) {
	return sprint(dst, len, (const char *) str);
}

static int _sprint_radix(pchar &dst, int &len, uint num, int radix) {
	if (len <= 0) return 0;

	const char radix_digits [] = "0123456789ABCDEF";
	char tmp_str[32];
	int digits_printed = 0;
	do {
		int digit = num % radix;
		num /= radix;
		tmp_str[digits_printed] = radix_digits[digit];
		digits_printed++;
	} while (num);

	if (len < digits_printed) digits_printed = len;

	for (int i = 0; i < digits_printed; i++) {
		dst[i] = tmp_str[digits_printed - i - 1];
	}
	len -= digits_printed;
	dst = &dst[digits_printed];

	return digits_printed;
}
static int _sprint_dec(pchar &dst, int &len, uint num) {
	return _sprint_radix(dst, len, num, 10);
}
static int _sprint_hex(pchar &dst, int &len, uint num) {
	return _sprint_radix(dst, len, num, 16);
}
	

static int sprint(pchar &dst, int &len, void *ptr) {
	return _sprint_radix(dst, len, (uint) ptr, 16);
}

static int sprint(pchar &dst, int &len, uint num) {
	return _sprint_dec(dst, len, num);
}

static int sprint(pchar &dst, int &len, short num) {
	return _sprint_dec(dst, len, (int) num);
}

static int sprint(pchar &dst, int &len, ushort num) {
	return _sprint_dec(dst, len, (uint) num);
}

static int sprint(pchar &dst, int &len, int num) {
	if (num < 0) {
		int prefix = sprint(dst, len, '-');
		return prefix + _sprint_dec(dst, len, (uint) -num);
	} 
	return _sprint_dec(dst, len, (uint) num);
}

static int sprint(pchar &dst, int &len, hex num) {
	if (len <= 0) return 0;
	int prefix = sprint(dst, len, "0x");
	return prefix + _sprint_hex(dst, len, num.i);
}

static int sprint(pchar &dst, int &len) {
	return 0;
}

template <typename F, typename... R>
static int sprint(pchar &dst, int &len, F first, R... rest) {
	int first_len = sprint(dst, len, first);
	int rest_len = sprint(dst, len, rest...);
	return first_len + rest_len;
}
