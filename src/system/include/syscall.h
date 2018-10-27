#pragma once

#include <std/types.h>
#include <system.h>


typedef uint (*SyscallPtr) (uint data);

extern SyscallPtr syscall_table[SyscallCode::MAX];


void enter_kernel();
void init_syscall();

