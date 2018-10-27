#pragma once

#define BYTES_PER_PAGE 4096

// page attributes
#define PAGE_PRESENT       1
#define PAGE_WRITE         2
#define PAGE_USER          4
#define PAGE_WRITETHROUGH  8
#define PAGE_CACHEDISABLED 16
#define PAGE_ACCESSED      32
#define PAGE_LARGE         128
#define PAGE_GLOBAL        256

#define PAGE_KERNEL_DATA          (PAGE_WRITE | PAGE_PRESENT)
#define PAGE_GLOBAL_DATA          (PAGE_GLOBAL | PAGE_WRITE | PAGE_PRESENT)
#define PAGE_GLOBAL_RO            (PAGE_GLOBAL | PAGE_PRESENT)
#define PAGE_USER_DATA            (PAGE_USER | PAGE_WRITE | PAGE_PRESENT)

// Mark a page as PAGE_COW to make it copy on write
#define PAGE_COW                  (PAGE_CACHEDISABLED | PAGE_USER | PAGE_PRESENT)

typedef char PageFrame[BYTES_PER_PAGE];

union PageMapEntry {
	uint val;
	struct {
		uint present:1;
		uint write:1;
		uint user:1;
		uint writethrough:1;
		uint cachedisable:1;
		uint accessed:1;
		uint dirty:1;
		uint pagesize:1;
		uint global:1;
		uint unused:3;
		uint page:20;
	};
};

typedef PageMapEntry PageTable[1024];

typedef PageTable *PageTablePtr;
