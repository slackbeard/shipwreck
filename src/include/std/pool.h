#pragma once

#include <std/types.h>
#include <std/bitset.h>

template <typename T, int N=0>
struct PoolAllocator {
	int max=N;
	BitAllocator<N> allocator;
	T pool[N];
	PoolAllocator() {
		max=N;
	}

	int alloc_index() {
		return allocator.lock_next_bit();
	}

	T *alloc() {
		int new_index = alloc_index();
		if (new_index < 0) return nullptr;
		return &pool[new_index];
	}
	int index_of(T *ptr) {
		return ((uint) ptr - (uint) &pool) / sizeof(T);
	}
	
	void free(T *ptr) {
		int index = index_of(ptr);
		if (index < 0 || index >= max) return;

		allocator.unlock_bit(index);
	}
};
