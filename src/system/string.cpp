#include <std/string.h>

extern "C" int strnlen(const char *s, size_t len) {
	int i = 0;
	for (; i < len; i++) {
		if (!s[i]) return i;
	}
	return i;
}

extern "C" int strncmp(const char *a, const char *b, size_t len ) {
	int a_len = strnlen(a, len);

	uint dwords = a_len / 4;

	uint *ad = (uint *) a;
	uint *bd = (uint *) b;
	int i = 0;
	uint diff;

	for (; i < dwords; i++) {
		diff = ad[i] - bd[i];
		if (diff) {
			return diff;
		}
	}

	i *= 4; //get char position for iterating over the remainder
	for (; i < len; i++) {
		diff = (int) (a[i] - b[i]);
		if (diff) {
			return diff;
		}
		if (a[i] == 0) break;
	}

	return 0;
}

extern "C" void *memcpy(void *dst, void *src, size_t num) {
	uint num_dwords = num / 4;
	uint i = 0;
	//process in 4-byte chunks
	for (; i < num_dwords; i++) {
		((uint *)dst)[i] = ((uint *)src)[i];
	}
	i *= 4; //multiply by dword size to get byte number

	//remainder
	for (; i < num; i++) {
		((char *) dst)[i] = ((char *)src)[i];
	}
	return dst;
}	

extern "C" void *memset(void *buffer, int value, size_t num_chars) {
		
	// 4 copies of that byte
	uint ch4 = (value & 0xFF);
	ch4 |= (ch4 << 16);
	ch4 |= (ch4 << 8);

	//process first in 4-byte chunks
	uint num_dwords = num_chars >> 2;
	int i=0;
	for (; i < num_dwords; i++) {
		((uint *)buffer)[i] = ch4;
	}

	i <<= 2;

	//remainder
	for (; i < num_chars; i++) {
		((char *) buffer)[i] = (char) value;
	}
	return buffer;
}

