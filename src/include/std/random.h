#pragma once

#include <std/types.h>
#include <std/bitops.h>

static uint prand_seed;
static uint prand() {
	const char data[] = {'b','l','a','h'};
	prand_seed = rotleft(prand_seed, prand_seed) ^ *(uint *) &data;
	return prand_seed;
}
