#pragma once
#include <interrupts.h>

INTERRUPT_DECLARATION (timer_interrupt);
void init_scheduler();

