#include <new>
#include <std/types.h>
#include <std/bitset.h>
#include <std/random.h>
#include <interrupts.h>
#include <process.h>
#include <devices/cpu.h>
#include <memory.h>
#include <page.h>
#include <util/debug.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 0

// how many bits, bytes, and dwords required to map all pages:
#define PAGE_BITMAP_BITS    (1<<20)
#define PAGE_BITMAP_BYTES   PAGE_BITMAP_BITS / 8

// how many attempts to race for static data ptr:
#define MAX_ALLOC_TRIES 10


// this is used for allocating static memory for the kernel:
uint static_alloc_ptr;
uint static_alloc_limit;

// this is used for expanding virtual memory:
uint virt_alloc_ptr;

PageTable *page_tables;
PageTable *page_dir;

// each bit in this bitset represents a 4k page in physical memory. 0=free 1=used.
BitAllocator<> *page_allocator;

// allocate a number of virtual pages starting at a given virtual address
void *virt_alloc_pages(uint pages, void *address, uint attributes) {
	PageFrame *virt_addr_ptr = (PageFrame *) address;

	uint pages_left = pages;

	// start our search at a "random" low index
	uint block_index = prand() % 256;

	// lock next available block
	uint locked_bits;
	debug(8, "page_allocator bits @ ", (hex) &page_allocator->bitset_blocks);
	while ((block_index = page_allocator->lock_next_block(&locked_bits, block_index))) {

		PageFrame *phys_addr_ptr = (PageFrame *) (block_index * 32 * 4096);

		uint testbit = 1;
		while (locked_bits != -1) {

			if (!(locked_bits & testbit)) {
			// a page is free

				map_to(virt_addr_ptr, (void *) phys_addr_ptr, attributes);
				locked_bits |= testbit;
				virt_addr_ptr++;
				pages_left--;

				if (pages_left <= 0) {
	
					page_allocator->unlock_block(locked_bits, block_index);
					debug(9, "Locked page at vaddr=", (hex) address);
					return address;
				}
			}
			phys_addr_ptr++;
			testbit <<= 1;

		}
	}

	return nullptr;
}
void *virt_alloc_pages(uint pages, void *address) {
	// sane defaults
	return virt_alloc_pages(pages, address, PAGE_USER_DATA);
}

//allocate a number of virtual pages, doesn't matter where
void *virt_alloc_pages(uint pages, uint attributes) {
	PageFrame *vaddr = (PageFrame *) next_virtual_pages(pages);
	return virt_alloc_pages(pages, vaddr, attributes);
}

void *virt_alloc_pages(uint pages) {
	// sane defaults
	return virt_alloc_pages(pages, PAGE_USER_DATA);
}

void *virt_alloc_page(uint attributes) {
	return virt_alloc_pages(1, attributes);
}
void *virt_alloc_page() {
	// sane defaults
	return virt_alloc_pages(1, PAGE_USER_DATA);
}

void *virt_alloc_page(void *address, uint attributes) { 
	return virt_alloc_pages(1, address, attributes);
}
void *virt_alloc_page(void *address) { 
	// sane defaults
	return virt_alloc_pages(1, address, PAGE_USER_DATA);
}

//get next available virtual pages, doesn't matter where
void *next_virtual_pages(uint pages) {
	uint newval = atomic_add_limit(
		virt_alloc_ptr,
		(uint) (pages * BYTES_PER_PAGE),
		(uint) -1	
	);
	if (newval == (uint) -1) {
		return nullptr;
	}
	return (void *) newval;
}
void *next_virtual_page() {
	return next_virtual_pages(1);
}


uint get_pde_index(void *vaddr) {
	return (uint) vaddr >> 22;
}
PageMapEntry *get_pde(void *vaddr) {
	// page directory index, table index:
	uint pdir_index = get_pde_index(vaddr);

	return &((*page_dir)[pdir_index]);
}

uint get_pte_index(void *vaddr) {
	return (uint) vaddr >> 12;
}
PageMapEntry *get_pte(void *vaddr) {
	// index into the 4MB page table array
	uint ptbl_index = get_pte_index(vaddr);

	return &((PageMapEntry *)page_tables)[ptbl_index];
}

uint lock_next_page(){
	// locks the next available page in the bitset
	// returns the locked bit index

	// lock the next free bit after `start_block_index`

	// start our search at a pseudo-random low index
	uint start_block_index = prand() % 256;

	uint locked_bits;
	uint locked_index = page_allocator->lock_next_block(&locked_bits, start_block_index);
	// return 0 on failure
	if (locked_bits == -1) return 0;

	// find available bit in this block
	uint bit_index = bitscan_forward(~locked_bits);
	uint bitmask = 1 << bit_index;
	// mark it as used
	locked_bits |= bitmask;
	page_allocator->unlock_block(locked_bits, locked_index);
	// return the bit index that was locked
	uint locked_bit_index = (locked_index * 32) + bit_index;
	return locked_bit_index;
}
void *phys_alloc_page() {
	// allocates the next available physical page
	return (void *) (lock_next_page() * sizeof(PageFrame));
}

PageMapEntry *ensure_pte(void *vaddr, uint attributes) {
	/*
		 Gets a page map entry, creating one if necessary
	*/

	PageMapEntry *pde = get_pde(vaddr);

	// if the page table hasn't been allocated, allocate one:
	if (pde->present == 0) {

		pde->val = (uint) phys_alloc_page() | attributes;

	}	

	// ptr to page table entry containing vaddr:
	return get_pte(vaddr);
}	


void *map_to(void *vaddr, void *p_addr, uint attributes) {

	// ptr to page map entries containing vaddr:
	PageMapEntry *pte = ensure_pte(vaddr, attributes);

	pte->val = (uint) p_addr | attributes;
	
	return vaddr;
}

void *map_to(void *vaddr, void *p_addr) {
	return map_to(vaddr, p_addr, PAGE_USER_DATA);
}



void *get_physical(void *vaddr) {

	// get the page table entry for virtual address, return null if none
	PageMapEntry *pte = get_pte(vaddr);
	if (pte == nullptr) return nullptr;

	return (void *) ((uint) pte->val & 0xFFFFF000);
}

void *static_alloc_pages(uint pages) {
	uint newval = atomic_add_limit(
		static_alloc_ptr,
		(uint) (pages * BYTES_PER_PAGE),
		static_alloc_limit
	);
	if (newval == static_alloc_limit) {
		return nullptr;
	}

	return (void *) newval;
}


PageFrame *page_copy_ptr;

INTERRUPT_DEFINITION(page_fault_handler) {
	ushort old_es;
	const ushort new_es = 0x10;
	uint cr2;
	__asm__ volatile(
		"movw %%es, %[old_es]\n"
		"movw %[new_es], %%es\n"
		"movl %%cr2, %[my_cr2]"
		:[old_es] "=r" (old_es),
		 [my_cr2] "=r" (cr2)
		:[new_es] "r" (new_es)
	);

	PageFaultParams *intArgs = &thisThread->cpuState->pfParams;

	if (intArgs->code == 0x07) {
		// `code` would be 6 if page was not present, so its safe to get the PTE:
		PageMapEntry *pte = get_pte((void *) cr2);
	
		if ((pte->val & 0xFFF) == PAGE_COW) {
		// Page is copy-on-write

			// `page_copy_ptr` lets us see both src and dst pages for copying:
			int *cr2_page = (int *) ((uint) cr2 & 0xFFFFF000);
			void *new_phys_page = phys_alloc_page(); 
			map_to(page_copy_ptr, new_phys_page);

			// copy page data:
			// TODO this is slow. schedule this in a syscall thread or something
			for (int i = 0; i< 1024; i++) {
				((int *) page_copy_ptr)[i] = cr2_page[i];
			}

			pte->page = (uint) new_phys_page >> 12;
			// set page to writable:
			pte->write = 1;

			// shift int stack to remove error code
			thisThread->cpuState->intParams = *(InterruptParams *) &thisThread->cpuState->pfParams.eip; 
			
			__asm__ volatile(
				"movw %[old_es], %%es\n"
				"mfence\n"
				"invlpg %[cowPage]\n"
				::
				[old_es] "r" (old_es),
				[cowPage] "m" (*(int *)cr2)
			);
			return;	
		}
	}
	debug(0, "PAGE INTERRUPT");
	debug(0, "CR2=", (hex) cr2, " procID=", (hex) proc_id, " threadID=", (hex) thisProc->thread_index);
	debug(0, " thread stack@", (hex) thisThread->stack, " bytes=", (hex) thisThread->stack_bytes);

	intArgs->dump();
	thisThread->cpuState->dump();
	debug(0, "KILL THREAD: proc_id=", (hex) proc_id, " thread_id=", (hex) thisProc->thread_index);
	// TODO kill thread and continue
	SPINJMP();
}

int initialize_memory(void *phys_base_free) {
// phys_base_free must point to free physical memory to fit:
// sizeof(PageAllocator) + 2 * 4096 bytes
// the page_allocator takes up ~130kb
// the other 2 pages are for the first page table and page directory
// returns 1 for success, 0 for failure

	// allocate physical pages sequentially for now
	uint phys_alloc_ptr = (uint) phys_base_free;


	// allocate the page dir and the first page table (still in linear mode so we use a physical address)
	page_tables = (PageTable *) phys_alloc_ptr;
	// locate physical address of page directory
	page_dir = &page_tables[PDIR_SELF_INDEX];

	phys_alloc_ptr += 2 * sizeof(PageFrame);


	uint bitset_blocks = phys_alloc_ptr;
	// in case we need it again:
	phys_alloc_ptr += PAGE_BITMAP_BYTES;

	page_allocator = (BitAllocator<> *) new ((void *)bitset_blocks) BitAllocator<PAGE_BITMAP_BITS>();

	{
		uint bit_start = bitset_blocks / sizeof(PageFrame);
		uint bit_end = (bitset_blocks + PAGE_BITMAP_BYTES + (sizeof(PageFrame) - 1)) / sizeof(PageFrame);
		page_allocator->lock_bit_range(bit_start, bit_end - bit_start);
	}


	// empty page dir entries
	for (int t = 0; t < 1024; t++) {
		(*page_dir)[t].val = 0;
	}

	// table 0 is for the low 4mb of physical memory (including the kernel), table 1 points at the page directory itself (PDIR_SELF_INDEX = 1)
	(*page_dir)[0].val = (uint) &page_tables[0] | PAGE_GLOBAL_DATA | PAGE_USER;
	// set page directory entry `PDIR_SELF_INDEX` to phys of page directory itself
	(*page_dir)[PDIR_SELF_INDEX].val = (uint) page_dir | PAGE_KERNEL_DATA;


	// store physical address of page dir to set cr3 later
	uint cr3 = (uint) page_dir;

	PageFrame *memory = (PageFrame *)0;

	// reserve low 4mb of phys memory for kernel
	page_allocator->lock_bit_range(0, 1024);

	// identity-map low 4mb
	// everything up to now is user-readable:
	int pte_i = 0;
	for (; pte_i < get_pte_index((void *)phys_alloc_ptr); pte_i++) {

		map_to(&memory[pte_i], &memory[pte_i], PAGE_GLOBAL_DATA | PAGE_USER);
	}
	
	// rest of 4mb is not user-readable
	for (; pte_i < 1024; pte_i++) {

		map_to(&memory[pte_i], &memory[pte_i], PAGE_GLOBAL_DATA);
	}

	debug(8, "Done mapping lower 4MB. page_dir=", (hex) page_dir);

	/* Enable paging */
	setPageDirectory(cr3);
	enablePaging();

	/* Now that paging is on, we deal in virtual addresses */
	// figure out the virtual addresses of the page table array
	PageTable *v_ptables = (PageTable * ) (PDIR_SELF_INDEX * sizeof(PageTable) * 1024);

	// static heap is everything from the last phys_alloc_ptr up to the page tables
	static_alloc_ptr = (uint) (phys_alloc_ptr + 4095) & 0xFFFFF000;
	static_alloc_limit = (uint) v_ptables;

	/* virtual memory becomes available after the page table array */
	virt_alloc_ptr = (uint) v_ptables + sizeof(PageTable) * 1024;

	page_tables = v_ptables;
	page_dir = &v_ptables[PDIR_SELF_INDEX];

	// allocate a virtual address for copying copy-on-write pages
	page_copy_ptr = (PageFrame *) static_alloc_pages(1);

	// set page fault handler
	idt->table[0x0E].set_handler((void *) page_fault_handler);

	// success
	return 1;
}



#pragma pop_macro("DEBUG_LEVEL")
