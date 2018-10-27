#pragma once
#include <std/types.h>

#define BLACK 0x00
#define GREY  0x3DEF
#define WHITE 0x7FFF

#define RED   0x7C00
#define GREEN 0x03E0
#define BLUE  0x01F

#define YELLOW 0x7FE0
#define PURPLE  0x7C1F
#define CYAN  0x3FF


#define DARK_RED   0x3C00
#define DARK_GREEN 0x01E0
#define DARK_BLUE  0x0F

#define GET_ALPHA(C) ((C) & (0x7 << 16))
#define SET_ALPHA(C) (((C) & 0x7) << 16)

// TODO i never actually use these:
struct ColorRGB {
	uint blue:5;
	uint green:5;
	uint red:5;
	operator ushort() const {
		return *(ushort *) this;
	}
};
struct ColorRGBA: public ColorRGB {
	uint alpha:3;
	uint rest:13;
};

static ushort fast_blend(ushort a, ushort b) {
	a = a & 0b111101111011111;
	b = b & 0b111101111011111;
	return (a + b) >> 1;
}
static ushort fast_blend_factor(ushort a, ushort b, uint factor) {
	uint maskbits = (1 << factor) - 1;
	uint mask = ~((maskbits) + (maskbits << 5) + (maskbits << 10));
	a = a & mask;
	b = b & mask;
	return ((a * maskbits) + b) >> factor;
}	

