#pragma once

#include <std/types.h>

#ifdef KERNEL_CODE

#include <kprint.h>
#define debug_print kprint

#else

#include <console.h>
#define debug_print print

#endif



#ifdef DEBUG

// each module can use its own DEBUG_LEVEL so long as it saves the previous value by push_macro("DEBUG_LEVEL") and pop_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 1
//#define debug(...) debug_module(DEBUG_LEVEL, __VA_ARGS__)
#define debug(L, ...) {\
	if (DEBUG_LEVEL >= L) {\
		debug_print("[" #L "]: " __VA_ARGS__);\
		debug_print("\n");\
	}\
} 


#else

// do nothing
#define debug(...) {}

#endif


// a hard stop by infinite loop if you don't have interrupts
#define SPINJMP() __asm__ volatile ("jmp ."::)

// a soft stop, respond to interrupts infinitely 
#define SPINHLT() __asm__ volatile ("1: hlt\njmp 1b")

