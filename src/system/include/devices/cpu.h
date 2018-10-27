#pragma once 

#include <std/types.h>

#define cli() __asm__ volatile("cli")
#define sti() __asm__ volatile("sti")

static void setPageDirectory(uint pdir) {
	// Set the page directory location (cr3)
	__asm__ volatile("mov %0, %%cr3"::"a"(pdir):"memory");
}

static void enablePaging() {
	// Enable paging in the machine status word (cr0):
	uint cr0;
	__asm__ volatile("mov %%cr0, %0":"=r"(cr0)::"memory");
	__asm__ volatile("mov %0, %%cr0"::"r"(cr0 | 0x80000000):"memory");
}
