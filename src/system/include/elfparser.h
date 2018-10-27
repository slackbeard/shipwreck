#pragma once 

// functions in this header run in user-mode so we use print() instead of kprint 

#pragma push_macro("KERNEL_CODE")
#undef KERNEL_CODE

#pragma push_macro("debug_print")
#define debug_print print

#include <system.h>
#include <console.h>

// Note: elf.h is usually in the include path, e.g. /usr/include/elf.h,
// maybe I'll copy it to the project later
#include <elf.h>
#include <std/string.h>

#define BYTES_PER_PAGE 4096

// From docs.oracle.com:
static unsigned long
elf_Hash(const unsigned char *name)
{
	unsigned long h = 0, g;
 
	while (*name)
	{
		 h = (h << 4) + (unsigned long) *name++;
		 if ((g = (h & 0xf0000000)))
			  h ^= g >> 24;
		 h &= ~g;
	}
	return h;
}

/*
	A module built from an ELF shared object.
*/
#define MAX_SHARED_LIBS 0x20
struct ElfModule {
	// Address of module in virtual memory
	char *module_base_vaddr;

	// size of module in virtual memory
	uint module_size;

	// Header
	Elf32_Ehdr header;

	// Program headers (segments)
	// TODO: hard limit for now fixme later if needed
	Elf32_Phdr phdrs[ 0x20 ];

	// Dynamic segment
	Elf32_Dyn *dynamic;
	// num dynamic entries:
	uint num_dynamic;

	// relocations
	Elf32_Rel *relocs;
	uint num_relocs;

	Elf32_Rel *jmp_relocs;
	uint num_jmp_relocs;

	// dynamic symbols
	Elf32_Sym *dynsym;
	uint sym_size;

	// hash table
	uint num_buckets;
	uint num_chains;
	uint *buckets;
	uint *chains;

	// string table
	char *strtab;
	uint *pltgot;

	void initialize() {
	// initialize an ElfModule object (just zero out memory)	
		memset((void *) this, (uint) 0, sizeof(ElfModule));
	}

	Elf32_Ehdr *readElfHeader(uint filehandle) {

		debug(9, "readElfHeader(): header@", (hex) &header, ", size: ", sizeof(Elf32_Ehdr));

		uint sects_read = sysapi::read(filehandle, (char *) &header, sizeof(Elf32_Ehdr));

		debug(9, "Read ", sects_read, " sectors of header to ", (hex) &header);
		debug(9, "Elf signature: ", header.e_ident[1], header.e_ident[2], header.e_ident[3]);

		return &header;
	}

	void parseSegments(uint filehandle) {
		//find the program headers
		if (!sysapi::seek(filehandle, header.e_phoff)) {
			debug(0, "e_phoff outside file bounds");
			debug(0, "FREEZING");
			SPINJMP();
		}
		
		
		int total = sizeof(Elf32_Phdr) * header.e_phnum;
		uint sects_read = sysapi::read(filehandle, (char *)&phdrs, total);

		// Now that we have the program headers, we can scan them for LOAD segments.
		// Each LOAD segment gets mapped into virtual memory
		mapLoadableSegments(filehandle);

		findDynamicSection(filehandle);
	}

	void mapLoadableSegments(uint filehandle) {
		// We need to find the start and end of the module's virtual memory
		uint min_address = 0xFFFFFFFF;
		uint max_address = 0;

		for (int i = 0; i < header.e_phnum; i++) {
			uint v_base = phdrs[i].p_vaddr;
			uint v_end = v_base + phdrs[i].p_memsz;
			if (v_base < min_address) {
				min_address = v_base;
			} else if (v_end > max_address) {
				max_address = v_end;
			}
		}

		module_size = max_address - min_address;

		uint module_pages = (module_size + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE + 1;

		module_base_vaddr = (char *) sysapi::alloc(module_pages);

		debug(9, "Module mapped @ ", 
			// first byte in module virtual space
			(hex)module_base_vaddr, 
			" -> ", 
			// last byte in module virtual space
			(hex) (& module_base_vaddr[module_pages * BYTES_PER_PAGE])
		);

		//Map all LOAD segments into virtual memory:
		for (int i = 0; i < header.e_phnum; i++) {
			if (phdrs[i].p_type == PT_LOAD) {
				int voffset = phdrs[i].p_vaddr ;
				char *vaddr =  &module_base_vaddr[voffset]; 
				uint offset = (uint) phdrs[i].p_offset;
				uint size = (uint) phdrs[i].p_filesz;

				debug(9, "LOAD Segment @ offset ", (hex) offset, ", v.addr = ", (hex) vaddr, ", size = ", (hex) size);
				sysapi::seek(filehandle, offset);
				sysapi::read(filehandle, vaddr, size);
			}
		}
	}

	void findDynamicSection(uint filehandle) {
		// Find the dynamic segment:
		dynamic = nullptr;
		for (int i = 0; i < header.e_phnum; i++) {
			if (phdrs[i].p_type == PT_DYNAMIC) {
				//Found the dynamic segment
				Elf32_Phdr *dyn_seg = &phdrs[i];

				dynamic = (Elf32_Dyn *) &module_base_vaddr[dyn_seg->p_vaddr];
				num_dynamic = dyn_seg->p_memsz / sizeof(Elf32_Dyn);

				debug(9, "Found dynamic section @ ", (hex) dynamic, ", num dynamic entries: ", num_dynamic);
				break;
			}
		}	

		if (dynamic == nullptr) {
			debug(0, "DYNAMIC SEGMENT NOT FOUND!!!");
			debug(0, "FREEZING");
			SPINJMP();
		}
	}

	void parseDynamicSegment(uint filehandle) {
		if (dynsym) return;

		if (dynamic == nullptr) {
			debug(0, "No dynamic section");
			return;
		}
		dynsym = nullptr;
		for (int i = 0; i < num_dynamic; i++) {
			uint type = dynamic[i].d_tag;
			switch (type) {
				// regular relocations:
				case DT_REL: {
					int voffset = dynamic[i].d_un.d_val;
					relocs = (Elf32_Rel *) &module_base_vaddr[voffset];

					debug(9, "Found relocations @ ", (hex) relocs);
					break;
				}
				// Number of relocations
				case DT_RELSZ: {
					num_relocs = dynamic[i].d_un.d_val / sizeof(Elf32_Rel);
					debug(9, "Num relocs: ", num_relocs);
					break;
				}
				// Relocations for plt
				case DT_JMPREL: {
					int voffset = dynamic[i].d_un.d_val;
					jmp_relocs = (Elf32_Rel *) &module_base_vaddr[voffset];

					debug(9, "Found jump relocations @ ", (hex) jmp_relocs);
					break;
				}
				// Number of relocations for plt
				case DT_PLTRELSZ: {
					num_jmp_relocs = dynamic[i].d_un.d_val / sizeof(Elf32_Rel);
					debug(9, "Num jmp relocs: ", num_jmp_relocs);
					break;
				}
				// dynsym symbol table
				case DT_SYMTAB: {
					int voffset = dynamic[i].d_un.d_val;
					dynsym = (Elf32_Sym *) &module_base_vaddr[voffset];
					debug(9, "Found dynamic symbols @ ", (hex) dynsym);
					break;
				}
				// size of a symbol entry
				case DT_SYMENT: {
					sym_size = dynamic[i].d_un.d_val;
					debug(9, "symbol entry size: ", sym_size);
					break;
				}
				// dynamic string table
				case DT_STRTAB: {
					int voffset = dynamic[i].d_un.d_val;
					strtab = &module_base_vaddr[voffset];
					debug(9, "String table: ",(hex)strtab);
					break;
				}
				// hash table
				case DT_HASH: {
					int voffset = dynamic[i].d_un.d_val;
					uint *hashtable = (uint *) &module_base_vaddr[voffset];
					num_buckets = hashtable[0];
					num_chains = hashtable[1];
					buckets = &hashtable[2];
					chains = &hashtable[num_buckets + 2];
					break;
				}
				case DT_PLTGOT: {
					int voffset = dynamic[i].d_un.d_val;
					pltgot = (uint *) &module_base_vaddr[voffset];
					debug(9, "GOT @ ", (hex) pltgot);
					break;
				}			
				default:
					break;
			}

		}
		if (dynsym == nullptr) {
			debug(0, "NO DYNAMIC SYM SECTION FOUND!");
			debug(0, "FREEZING:");
			SPINJMP();
		}
	}

	Elf32_Sym *getSymbol(const char *name) {
		debug(9, "getSymbol: name=", (char *) name);

		int sym_index = getSymbolIndex(name);
		if (sym_index < 0) {
			debug(0, "Symbol '", name, "' NOT FOUND");
			return nullptr;
		}

		Elf32_Sym *sym = &dynsym[sym_index];
		
		debug(9, "Found symbol: "
			 "sym index=", (hex) sym_index, ", "
			"symbol@", (hex) sym, ", "
			"value=", (hex) sym->st_value);

		return sym;
	}

	int getSymbolIndex(const char *name) {

		uint bucket = elf_Hash((const uchar *) name) % num_buckets;

		int sym_index = buckets[bucket];

		do {

			Elf32_Sym *sym = &dynsym[sym_index];

			// string index for the symbol name
			uint st_name = sym->st_name;

			char *sym_name = &strtab[st_name];
			
			if (strncmp(sym_name, name, 0x40) == 0) {
				return sym_index;
			}

			sym_index = chains[sym_index];
		} while (sym_index != 0); // end of chain

		debug(0, "Symbol ", name, " NOT FOUND");
		return -1;
	}

	void *getFunctionPtr(const char *name) {

		Elf32_Sym *load_sym = getSymbol(name);
		if (load_sym != nullptr) {
			debug(9, "Found function: '", name, "' @ symbol: ", (hex) load_sym);

			uint offset = load_sym->st_value;
			return (void *) &module_base_vaddr[ offset ];
		}
		debug(0, "Function not found: '", name, "'");
		return nullptr;
	}

};

ElfModule sharedLibs[MAX_SHARED_LIBS];
uint numSharedLibs;

void __attribute__ ((stdcall)) lookup_symbol_args(int arg0);

static void adjustRelocations(Elf32_Rel *relocs, int num_relocs, uint base_vaddr) {
// Add base_vaddr to an array of relocation entries
	if (base_vaddr == 0 || relocs == nullptr) return;

	for (int i = 0; i < num_relocs; i++) {
		relocs[i].r_offset += base_vaddr;

		uint *valptr = (uint *) relocs[i].r_offset;

		*valptr += base_vaddr;

	}
}

static ElfModule *loadModule(uint filehandle, ElfModule *elfimg) {
// parses an ELF file into an ElfModule object:
				
	elfimg->initialize();

	elfimg->readElfHeader(filehandle);

	elfimg->parseSegments(filehandle);

	elfimg->parseDynamicSegment(filehandle);

	adjustRelocations(elfimg->relocs, elfimg->num_relocs, (uint) elfimg->module_base_vaddr);
	adjustRelocations(elfimg->jmp_relocs, elfimg->num_jmp_relocs, (uint) elfimg->module_base_vaddr);

	return elfimg;
}

static ElfModule *loadNeededLibs(uint filehandle, ElfModule *elfimg) {

	Elf32_Dyn *dynamic = elfimg->dynamic;
	char *strtab = elfimg->strtab;

	for (int i = 0; i < elfimg->num_dynamic; i++) {
		uint type = dynamic[i].d_tag;
		switch (type) {
			case DT_NEEDED: {
			// TODO test this with more than 1 shared lib
				uint index = dynamic[i].d_un.d_val;
				const char *libname = (const char *) &strtab[index];
				debug(9, "FOUND NEEDED LIB: index=", (hex) index, ", strtab @ ", (hex) strtab, " name='", libname);

			    uint lib_fh = sysapi::open(libname);	
				debug(9, "LOADED LIB fh=", lib_fh);
				
				ElfModule *module = &sharedLibs[numSharedLibs];
				numSharedLibs++;
				debug(9, "LOADING SHARED OBJ:");
				loadModule(lib_fh, module);

					
				break;
			}
			default: break;
		}
	}
	return elfimg;
}


#pragma push_macro("DEBUG_LEVEL")
#define DEBUG_LEVEL 0

static int elf_exec(const char *name) {
	ElfModule executable;

	int filehandle = sysapi::open(name);

	executable.initialize();

	executable.readElfHeader(filehandle);

	//parse the program headers (segments) 
	Elf32_Ehdr *header = &executable.header;

	//find the program headers
	if (!sysapi::seek(filehandle, header->e_phoff)) {
		debug(0, "e_phoff outside file bounds");
		debug(0, "FREEZING");
		SPINJMP();
	}

	int total = sizeof(Elf32_Phdr) * header->e_phnum;
	uint sects_read = sysapi::read(filehandle, (char *)&executable.phdrs, total);

	debug(9, "program headers @ ",  (hex) &executable.phdrs);

	//Map all LOAD segments into virtual memory:
	for (int i = 0; i < header->e_phnum; i++) {

		debug(9, "PROGRAM HEADER [", (hex) i, "] type = ", (hex) executable.phdrs[i].p_type);
		if (executable.phdrs[i].p_type == PT_LOAD) {
			uint voffset = executable.phdrs[i].p_vaddr;
			uint offset = (uint) executable.phdrs[i].p_offset;
			uint size = (uint) executable.phdrs[i].p_filesz;

			debug(9, "LOAD Segment @ offset ", (hex) offset, ", v.addr = ", (hex) voffset, ", size = ", (hex) size);
			uint pages = ((voffset & 0xFFF) + size + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE;
			void *vaddr = (void *)(voffset & 0xFFFFF000);

			sysapi::alloc_at(pages, vaddr);

			sysapi::seek(filehandle, offset);

			sysapi::read(filehandle, (char *) voffset, size);
		}
	}
	debug(9, "Done loading program headers (segments)");
	executable.findDynamicSection(filehandle);

	executable.parseDynamicSegment(filehandle);

	loadNeededLibs(filehandle, &executable);

	debug(9, "Lookup symbol @ ", (hex) lookup_symbol_args);
	executable.pltgot[1] = (uint) &executable;
	executable.pltgot[2] = (uint) lookup_symbol_args;
	debug(9, "Entry point @ ", (hex) header->e_entry);

	debug(9, "About to call entry...");
	((void (*)())header->e_entry)();

	//enter_user_mode((void *) header->e_entry, user_stack);
	//never gets here
	SPINJMP();
	return 0;
}

	

#pragma pop_macro("DEBUG_LEVEL")


void __attribute__ ((stdcall)) lookup_symbol_args(int arg0) {

	uint ebp = (uint) __builtin_frame_address(0);

	uint *args = &((uint *) ebp)[1];

	ElfModule *elfimg = (ElfModule *) args[0];
	uint sym_reloc_index = args[1] / sizeof(Elf32_Rel);


	if (sym_reloc_index >= elfimg->num_jmp_relocs) {
		debug(0, "ERROR: reloc index > num relocs (", (hex) elfimg->num_jmp_relocs);
		debug(0, "FREEZING");
		SPINJMP();
	}

	Elf32_Rel *reloc = &elfimg->jmp_relocs[sym_reloc_index];
	int sym_index = ELF32_R_SYM(reloc->r_info);
	
	// TODO check sym index against dynsym section size somehow	
	Elf32_Sym *symbol = &elfimg->dynsym[sym_index];

	char *sym_name = (char *) &elfimg->strtab[symbol->st_name];

	// TODO search for symbol in ALL shared libs
	void *functionPtr = sharedLibs[0].getFunctionPtr(sym_name);

	// patch the GOT entry for this symbol so we don't invoke the lookup everytime
	*(int *) reloc->r_offset = (int) functionPtr;


	/*
	 Fix stack and return. 
	 insert the functionPtr as the return address for this call (which actually overwrites the symbol index arg on the stack) and return from this function.
	*/
	__asm__ volatile(
	"movl %0, 0x04(%%ebp)\n"
	::"r" (functionPtr));

}

#pragma pop_macro("debug_print")
#pragma pop_macro("KERNEL_CODE")
