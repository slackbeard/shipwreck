#pragma once

#include <std/types.h>
#include <std/string.h>

#include <std/bitops.h>
#include <std/random.h>
#include <std/atomic.h>




template <int N=0>
struct BitAllocator {

	uint max_blocks;
	uint max_bits;	
	volatile uint bitset_blocks[(N+31)/32];

	BitAllocator() {
		max_bits = N;
		max_blocks = (N+31) / 32;
		memset((void *) bitset_blocks, (uint) 0, max_blocks * 4);
	}

	inline uint get_block_index(uint bit_index) {
		return (uint) bit_index / 32;
	}


	uint lock_block(uint block_index) {
		// try to lock a 32-bit block at once by atomically swapping with 0xFFFFFFFF

		// if this returns -1, then the bit was already locked or there were no free bits to lock
		return (uint) atomic_swap((int *) &bitset_blocks[block_index], -1);
	}
	
	uint lock_next_block(uint *locked_bits, uint start_block_index) {
	// find the next block after `start_block_index` that has a free bit and lock the block
	// returns the index of the locked block,
	// `locked_bits` is set to the block's value
		for (uint i = start_block_index; i < max_blocks; i++) {
			*locked_bits = lock_block(i);
			if (*locked_bits != -1) return i;
		}
		// failure if locked_bits==-1 or if this returns 0
		return 0;
	}

	int lock_next_bit(uint start_block_index) {
		// atomically locks the next free bit after `start_block_index`
		// returns the locked bit index
		for (uint i = start_block_index; i < (start_block_index + max_blocks); i++) {
			int block_index = i % max_blocks;
			uint bits = bitset_blocks[block_index];
			for (int j = 0; (j < 32) && (bits != -1); j++) {
				uint bit_index = bitscan_forward(~bits);
				uint final_index = bit_index + (i * 32);
				if (final_index >= max_bits) return -1;

				uint new_bits = bits | (1 << bit_index);
				uint old_bits = bits;

				bits = __sync_val_compare_and_swap(&bitset_blocks[block_index], bits, new_bits);
				if (old_bits == bits) {
					return bit_index + (block_index * 32);
				}
			}

		}
		return -1;
	}

	int lock_next_bit() {
		// atomically locks a free bit
		// returns the locked bit index
		int start_block_index = prand() % max_blocks;
		return lock_next_bit(start_block_index);
	}
	
	void unlock_bit(uint bit_index) {
		uint block_index = get_block_index(bit_index);
		uint bit_in_block = bit_index & 0x1F;
		bitset_blocks[block_index] &= ~(1 << bit_in_block);
	}

	void unlock_block(uint new_bits, uint block_index) {
	// unlock a previously locked block by settings its bits to a new value (even -1 is valid)
		bitset_blocks[block_index] = new_bits;
	}

	int lock_bitmask(uint block_index, uint bitmask) {
		if (bitmask == 0) return 0;

		// first lock the entire uint by setting it to -1
		int locked_bitmask = atomic_swap((int *) &bitset_blocks[block_index], -1);

		if (locked_bitmask == -1) {
		// then it is either locked or represents no free bit
			return -1;
		}
		
		uint new_locked_bits = (~locked_bitmask) & bitmask;
		bitset_blocks[block_index] = locked_bitmask | bitmask;

		return new_locked_bits;
	}


	void lock_bit_range(uint start, uint size) {

		uint start_index = get_block_index(start);
		uint end_index = get_block_index(start + size);

		// The start and end bits may not land on a 32-bit boundary.
		// Calculate the unaligned bits of the head and tail uints:
		uint head_bits = 32 - (start % 32);
		int head_mask = high_ones(head_bits);
		uint tail_bits = (size + 32 - head_bits) % 32;
		int tail_mask = low_ones(tail_bits);

		if (start_index == end_index) {
			lock_bitmask(start_index, head_mask & tail_mask);
			return;
		}
		
		// head
		lock_bitmask(start_index, head_mask);

		// body
		for (int i = start_index + 1; i < end_index; i++) {
			lock_bitmask(i, 0xFFFFFFFF);
		}

		// tail
		lock_bitmask(end_index, tail_mask);
	}



};

