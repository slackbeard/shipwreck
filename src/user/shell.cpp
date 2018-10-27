#include <new>
#include <util/debug.h>
#include <std/types.h>
#include <std/events.h>
#include <std/queue.h>
#include <gui/colors.h>
#include <gui/objects.h>
#include <gui/terminal.h>
#include <system.h>
#include <console.h>


struct GUIModel {
	WindowFrame *main;
	Window *bg;
	Window *border;
	TextBox *text;
	Window *windows;
	GUITerminal terminal;

	void key_handler(char key);
	GUIModel();

};
GUIModel *gui;

void event_thread();

void display_help() {
	println("Shell commands:");	
	println("help      Display this help message");
}

extern "C" void _start() {
	sysapi::process_env = sysapi::get_environment();
	GUIModel mygui;
	gui = &mygui;

	sysapi::process_env->user_filetable[FD_STDIN] = &gui->terminal;
	sysapi::process_env->user_filetable[FD_STDOUT] = &gui->terminal;


	sysapi::new_thread((void *) event_thread);

	sysapi::update_gui();
	sysapi::redraw_gui();


	const int max_cmd_chars = 64;
	char command[max_cmd_chars];
	
	println("Shell accepting commands:");

	while (1) {
		print(">");
		int len = getline((char *) command, max_cmd_chars-1);
		if (len <= 0) {
			println("Failed to read command. Exiting");
			break;
		}

		if (command[len-1] != '\n') {
			println("Command line overflow. Resetting");
			continue;
		}

		command[len-1] = 0;
		if (strncmp((char *) command, "help", 4) == 0) {
			display_help();
		} else {
			println("Unrecognized command: ", command);
		}
	}
	SPINJMP();
}

void event_thread() {

	MsgQueue<> *msgQueue = (MsgQueue<> *) &sysapi::process_env->msgQueue;

	// subscribe us to keyboard events 
	// (input events are subscribed by default but I'm leaving this for testing:)
	sysapi::subscribe(UserEvents::KEY_CHAR_DOWN);
	while (true) {
		sysapi::monitor(MSG_MONITOR,10);

		QueueRange qr = msgQueue->dequeue(10);


		for (int i = qr.head; i < qr.tail; i++) {
			Message *msg = msgQueue->get(i);
			
			if (msg->msg_id == (int) UserEvents::KEY_CHAR_DOWN) gui->key_handler(msg->data);
		}

		msgQueue->release(qr.tail - qr.head);
		
	}

	// never gets here
	SPINJMP();
}


void GUIModel::key_handler(char key) {
	return terminal.key_handler(key);
}

GUIModel::GUIModel(): 
	windows(sysapi::process_env->windows)
{

	main = new (&windows[0]) WindowFrame(
		{100, 100, 500, 400}
	);
	main->id = 0;

	border = new (&windows[1]) Window(
		{0, 0, 400, 300}
	);

	border->id = 1;
	border->color = fast_blend(WHITE, fast_blend(BLUE, RED));
	border->zindex = 0;

	bg = new (&windows[2]) Window(
		{10, 10, 390, 290}
	);

	bg->id = 2;
	bg->zindex = 1;
	bg->color = SET_ALPHA(1) + fast_blend(GREY, fast_blend(BLUE, RED));


	TextBox::Buffer<> *buf = (TextBox::Buffer<> *) new (sysapi::process_env->free) TextBox::Buffer<45,20>();

	buf->offset_y = 0;
	buf->offset_x = 0;


	text = new (&windows[3]) TextBox(
		{20, 20, 380, 280},
		buf
	);
	text->id = 3;
	text->zindex = 2;
	terminal.textbox = text;

	main->addChild(border);
	main->addChild(bg);
	main->addChild(text);

}
