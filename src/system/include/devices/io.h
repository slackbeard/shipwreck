#pragma once

#include <std/types.h>

static void outb(uint port, uchar data) {
	__asm__ volatile("outb %%al, %%dx"::"a"(data),"d"(port));
}

static void outw(uint port, ushort data) {
	__asm__ volatile("outw %%ax, %%dx"::"a"(data),"d"(port));
}

static void outd(uint port, uint data) {
	__asm__ volatile("outl %%eax, %%dx"::"a"(data),"d"(port));
}

static uchar inb(uint port) {
	uchar data;
	__asm__ volatile("inb %%dx, %%al":"=a"(data):"d"(port):"memory");
	return data;
}

static ushort inw(uint port) {
	ushort data;
	__asm__ volatile("inw %%dx, %%ax":"=a"(data):"d"(port):"memory");
	return data;
}

