#include <std/types.h>
#include <util/debug.h>
#include <kprint.h>

// devices
#include <devices/io.h>
#include <devices/cpu.h>
#include <devices/apic.h>
#include <devices/ata.h>
#include <devices/ps2.h>
#include <devices/vga.h>
// virtual devices:
#include <devices/tar.h>
#include <devices/terminal.h>

#include <page.h>
#include <memory.h>
#include <interrupts.h>
#include <process.h>
#include <events.h>
#include <syscall.h>
#include <scheduler.h>

#include <gui.h>
#include <gui/objects.h>

// some code in the kernel runs in usermode:
#include <system.h>
#include <elfparser.h>

// spawn a user process
void spawn(int data);

struct GUITerminal: public File {
	TextBox *textbox;
	GUITerminal() { }

	virtual int write(const char *buffer, uint len);
};
struct GUIModel {
	TextBox *text;
	GUIDesktop *desktop;
	WindowFrame *winmain;
	Window *background;
	GUITerminal terminal;

	GUIModel ();
};

// Entry point is named '_start' only as a formality, it can be anything since boot_stub.nasm calls it by name.
extern "C" void _start(uint arg1) {

	// make objects on the stack for now - we switch stacks later and leave this memory alone:
	VGATerminal terminal;
	terminal.init();
	terminal_device = &terminal;
	debug(0, "Shipwreck OS initializing...");

	// Disable PIC
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);

	// Set up interrupt table and default handlers
	init_interrupts();
	cli();


	// Initialize page tables and allocation table:
	debug(0, "Initializing memory:");
	initialize_memory((void *) 0x100000);
	// NOTE do not print() or debug() until video memory is mapped


	// map video memory
	{
		PageFrame *vid_memory = (PageFrame *)0xE0000000;
		// ensure the page table is marked as PAGE_GLOBAL_DATA:
		ensure_pte(vid_memory, PAGE_GLOBAL_DATA);

		for (int i = 0; i < 1024; i++) {
			map_to(&vid_memory[i], &vid_memory[i], PAGE_GLOBAL_DATA);
		}
	}

	// LAPIC memory-mapped IO address:
	ensure_pte((void *) LAPIC_ADDRESS, PAGE_GLOBAL_DATA);
	map_to((void *) LAPIC_ADDRESS, (void *) LAPIC_ADDRESS);

	// IOAPIC memory-mapped IO address:
	ensure_pte((void *) IOAPIC_ADDRESS, PAGE_GLOBAL_DATA);
	map_to((void *) IOAPIC_ADDRESS, (void *) IOAPIC_ADDRESS);


	// Allocate some space for interrupt handlers
	interrupt_stack = (void *) ((int) static_alloc_pages(1) + 4096);

	sti();

	debug(9, "Interrupt stack@", (hex) interrupt_stack);

	// Initialize Task register (TSS is setup in `boot_stub.nasm`)
	__asm__ volatile(
		"movw $0x28, %%dx\n"
		"ltr %%dx\n"
		:::"edx");

	debug(0, "Initializing IDE driver...");

	// initialize disk driver
	ATADevice hdd;
	TarFS tarfs(&hdd);

	// global filesystem object
	filesystem = &tarfs;

	// Init process table
	init_processes();

	debug(0, "Initializing GUI...");
	// Init GUI here so we can create a GUI terminal
	init_gui(VGA_WIDTH, VGA_HEIGHT);

	GUIModel gui;
	terminal_device = &gui.terminal;
	procs[0].files[FD_STDOUT] = terminal_device;

	// NOTE: gui won't render until the scheduler is started
	kprintln("Kernel messages:");
	
	// Init event system
	init_events();

	// syscall API via sysenter/sysexit
	init_syscall();

	const char *filename = "shell";
	new_callback_event(&spawn, (int) filename);

	// init LAPIC & timer
	init_lapic();

	// Set the timer handler:
	init_scheduler();

	// fast tick
	lapic->timer_div = (uint) 0x08;
	lapic->timer_init = (uint) 0x0FFFF;

	init_ioapic();

	init_ps2(VGA_WIDTH, VGA_HEIGHT);

	debug(9, "Done initializing kernel. Static mem left: ", (hex) (static_alloc_limit - static_alloc_ptr));

	new_display_msg({.event=DisplayEvents::REDRAW_SCREEN});

	thisThread->runState = WAITING;
	SPINHLT();

}

void user_process() {
// this loads an elf image into the current space from usermode

	// TODO hard-coded filename for now. send as thread arg later
	elf_exec("shell");

	SPINJMP();
}


void spawn(int data) {
	// TODO `data` not used for now but should be passed to user thread

	cli();
	Process *newproc = new_user_process();
	UserThread *userThread = new_user_thread((void *) user_process, 0);
	sti();

	//userThread->dump();
}

int GUITerminal::write(const char *buffer, uint len) {
	int retval = this->textbox->print(buffer, len);
	gui_redraw_proc(0);
	return retval;
}

GUIModel::GUIModel () {
	Window *windows = thisProc->env->windows;

	GUIWindow *gw;

	gw = winmgr->getGUIWindow(0);
	desktop = new (gw) GUIDesktop({0, 0, (int) VGA_WIDTH, (int) VGA_HEIGHT});

	gw->wm_root = gw;
	winmgr->desktop = desktop;


	int desktop_img_len = VGA_WIDTH * VGA_HEIGHT * 2;
	int desktop_img_pages = (desktop_img_len + 4095) / 4096;

	ushort *desktop_img_data = (ushort *) static_alloc_pages(desktop_img_pages);

	// checker pattern for testing:
	for (int y = 0; y < VGA_HEIGHT; y++) {
		for (int x = 0; x < VGA_WIDTH; x++) {
			if ((x ^ y) & 0x20) {
				desktop_img_data[y * VGA_WIDTH + x] = fast_blend(GREY, fast_blend(GREY, BLUE));	
			} else {
				desktop_img_data[y * VGA_WIDTH + x] = fast_blend(GREY, BLUE);
			}
		}
	}

	// load cool wallpaper:
	// NOTE: to speed up boot times, comment this out:
	debug(0, "Loading desktop.tga...");
	load_background_tga(desktop_img_data);

	desktop->pid=0;
	desktop->windata.id=0;
	desktop->windata.display_data = (void *) desktop_img_data;

	/* Main window */

	winmain = new (&windows[1]) WindowFrame(
		{200, 150, 600, 450}
	);
	winmain->id = 1;


	/* Window background */

	background = new (&windows[2]) Window(
		{0, 0, 400, 300}
	);

	background->id = 1;
	background->color = fast_blend(GREY, fast_blend(YELLOW, RED));
	background->zindex = 0;

	winmain->addChild(background);


	/* Text terminal */
	TextBox::Buffer<> *buf = (TextBox::Buffer<> *) new (procs[0].env->free) TextBox::Buffer<45,10>;
	text = new (&windows[3]) TextBox(
		{20, 20, 380, 280},
		buf
	);
	text->id = 3;
	text->zindex = 2;
	terminal.textbox = text;

	winmain->addChild(text);

	/* put window heirarchy together: */
	winmain->addChild(text);

	/* push desktop GUI object onto WM stack */
	winmgr->winstack.push(desktop);

	/* sync kernel's GUI data with WM */
	gui_update_proc(0);

	// redraw whole screen:
	Rect screen = {0, 0, (int) VGA_WIDTH, (int) VGA_HEIGHT};

	gui_redraw_rect(&screen, winmgr->winstack.top);

}
