#pragma once

static inline int atomic_swap(int *dst, int src) {
	int retval = 0;
	__asm__ volatile (
		"xchgl %%eax, %2\n"
		:"=a" (retval)
		:"a"(src), "m" (*dst)
	);
	return retval;

}
#define MAX_ADD_TRIES 10
static uint atomic_add_limit(uint &val, uint diff, uint limit) {
// assuming we never allow `val >= limit`, this will atomically add `diff` to `val` unless it would exceed `limit`.
// returns previous `val` on success, or `limit` on overflow.
	uint myval = val;

	for (int i = 0; i < MAX_ADD_TRIES; i++) {
		if (diff > (limit - myval)) return limit;
		
		uint newval = myval + diff;
		uint oldval = myval;
		myval = __sync_val_compare_and_swap(&val, oldval, newval);
		if (myval == oldval) return oldval;
	}
	return limit;
}
