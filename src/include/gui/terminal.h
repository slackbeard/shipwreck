#pragma once

#include <std/types.h>
#include <std/file.h>
#include <std/events.h>
#include <gui/colors.h>
#include <gui/objects.h>

#define MAX_INPUT_BUFFER 64


struct GUITerminal: public File {

	// stdin:
	char input_buf[MAX_INPUT_BUFFER];
	int input_buf_head = 0;
	int input_buf_tail = 0;

	// stdout:
	TextBox *textbox;

	GUITerminal() { }
	void key_handler(char key) {

		// since there is only one msg handler thread
		// I won't use a lock for the producer here

		int input_buf_size = input_buf_tail - input_buf_head;


		if (input_buf_size >= MAX_INPUT_BUFFER) {
			return;
		}

		input_buf[input_buf_tail % MAX_INPUT_BUFFER] = key;

		input_buf_tail++;

		sysapi::notify(INPUT_MONITOR, 1);
	}

	char getchar() {

		// wait exclusively on input event
		sysapi::monitor(INPUT_MONITOR, 1);

		char retval = input_buf[input_buf_head];

		input_buf_head++;

		sysapi::unmonitor(INPUT_MONITOR);

		return retval;
	}

	virtual int read(char *buffer, uint max) {
		// read a line up to `max` chars into `buffer`
		// return num of chars read

		int len=0;

		char nextchar = 0;
		do {

			// wait exclusively on input event
			sysapi::monitor(INPUT_MONITOR, 1);

			nextchar = input_buf[input_buf_head % MAX_INPUT_BUFFER];
			input_buf_head++;


			if (nextchar == '\b') {
				if (len > 0) {
					textbox->backspace();
					sysapi::redraw_gui();
					buffer[--len] = ' ';
				}
			} else 	{
				if (nextchar != '\n') {
					if (len >= max-1) {
						len = max-1;
						textbox->backspace();
						buffer[--len] = ' ';
					}
					if (len >= MAX_INPUT_BUFFER-1) {
						len = MAX_INPUT_BUFFER-1;
						textbox->backspace();
						buffer[--len] = ' ';
					}
				}
				buffer[len] = nextchar;
				write(&buffer[len], 1);
				len++;
			}

			
		} while (nextchar != '\n');

		sysapi::unmonitor(INPUT_MONITOR);

		return len;
	}
	virtual int write(const char *buffer, uint len) {
		int retval = textbox->print(buffer, len);
		sysapi::redraw_gui();
		return retval;
	}
};
