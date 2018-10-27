#pragma once

#include <std/types.h>

static uint rotleft(uint val, uint bits) {
	uint b32 = bits & 0x1F;
	return (val << b32) | (val >> (32 - b32));
}
static uint rotright(uint val, uint bits) {
	uint b32 = bits & 0x1F;
	return (val >> b32) | (val << (32 - b32));
}

static uint bitscan_forward(uint data) {
	int retval = 0;
	__asm__ volatile(
		"bsfl %0, %1\n"
		:"=m" (data), "=r" (retval)
	);
	return retval;
}

static int high_ones(int num_bits) {
	// returns an int with `num_bits` of the most significant bits set to 1
	int invbits = 32 - num_bits;
	int halfbits = invbits >> 1;
	
	int mask = (1 << halfbits);
	mask <<= (invbits - halfbits);
	return -mask;
}

static int low_ones(int num_bits) {
	// returns an int with `num_bits` of the least significant bits set to 1
	int halfbits = num_bits >> 1;
	
	int mask = (1 << halfbits);
	mask <<= (num_bits - halfbits);
	return mask - 1;
}
