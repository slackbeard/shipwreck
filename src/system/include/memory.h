#pragma once

#include <std/types.h>
#include <std/bitset.h>
#include <page.h>

/*
 `PDIR_SELF_INDEX` is the index of the page table that points to the page directory;
i.e. that page table maps the entire array of page tables into a contiguous 4MB chunk of virtual memory
 This could be any value from 1 .. 3FF
*/
#define PDIR_SELF_INDEX 1


// This is a 4MB flattened-out array of all the page tables
extern PageTable *page_tables;
// ptr to the page directory itself, could be one of the page tables
extern PageTable *page_dir;
//extern uint virt_alloc_ptr;

extern uint static_alloc_ptr;
extern uint static_alloc_limit;

extern BitAllocator<> *page_allocator;

void *phys_alloc_pages(uint pages);
void *phys_alloc_page(void);
	
//allocate a number of virtual pages at a specific address with given attributes
void *virt_alloc_pages(uint pages, void *address, uint attributes);
void *virt_alloc_pages(uint pages, void *address);

//allocate a number of virtual pages, doesn't matter where
void *virt_alloc_pages(uint pages, uint attributes);
void *virt_alloc_pages(uint pages);

// allocate single page at address with attributes
void *virt_alloc_page(void *address, uint attributes);
void *virt_alloc_page(void *address);
void *virt_alloc_page(uint attributes);
void *virt_alloc_page(void);


void *next_virtual_pages(uint pages);
void *next_virtual_page(void);


void *map_to(void *vaddr, void *p_addr, uint attributes);
void *map_to(void *vaddr, void *p_addr);

uint get_pde_index(void *vaddr);
uint get_pte_index(void *vaddr);

extern PageMapEntry *get_pte(void *addr);
extern PageMapEntry *get_pde(void *addr);

PageMapEntry *ensure_pte(void *vaddr, uint attributes=PAGE_KERNEL_DATA);


void *get_physical(void *vaddr); 

void *static_alloc_pages(uint pages);

int initialize_memory(void *phys_base_free);



