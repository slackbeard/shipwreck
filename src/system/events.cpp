#include <util/debug.h>
#include <new>
#include <events.h>
#include <std/pool.h>
#include <memory.h>
#include <interrupts.h>
#include <process.h>
#include <threads.h>
#include <std/queue.h>
#include <devices/vga.h>
#include <devices/ps2.h>
#include <gui.h>

#pragma push_macro("DEBUG_LEVEL")

#define DEBUG_LEVEL 0



KernelThread *kernelEvents;

struct Worker {
	virtual void run() {
	}
};


template <int N=1>
struct CallbackWorker: public Worker {
	SharedMsgQueue<N> event_queue;

	virtual void run() {
		event_queue.gather_new_data();

		if (event_queue.size() <= 0) {
			return;
		}
			
		QueueRange qr = event_queue.dequeue(10);

		for (int i = qr.head; i < qr.tail; i++) {
			Message *msg = (Message *) event_queue.get(i);

			// treat msg_id as a function ptr with `data` param
			((void (*)(int)) msg->msg_id)(msg->data);
		}
		event_queue.release(qr);
		event_queue.garbage_collect();
		(*procs[0].msg_signal) -= (qr.tail - qr.head);
	}
};



/*   Event subscriptions:   */
struct SubscriberNode {
	SubscriberNode *next;
	Process *proc;
	SubscriberNode() {
		next = nullptr;
		proc = nullptr;
	}

};

struct SubscriberList {
	SubscriberNode *head;

	SubscriberList() {
		head=nullptr;
	}

	SubscriberNode *push_head(SubscriberNode *node) {
		node->next = head;
		head = node;
		return node;
	}

	void notify(Message *msg) {
	// notify all subs

		if (head == nullptr) return;
		if ((uint) msg->msg_id >= (uint) UserEvents::MAX) return;

		SubscriberNode *sub = head;
		for (int i = 0; sub && i < MAX_PROCS; sub = sub->next, i++) {
			sub->proc->send_msg(msg);
		}
	}

	void notify(Process *target, Message *msg) {
	// only notify `target` process if its subbed

		if (head == nullptr) return;
		if ((uint) msg->msg_id >= (uint) UserEvents::MAX) return;

		SubscriberNode *sub = head;
		for (int i = 0; sub && i < MAX_PROCS; sub = sub->next, i++) {
			if (target == sub->proc) {
				sub->proc->send_msg(msg);
				break;
			}
		}
	}

	void dump() {
		debug(9, "Subscriber List:");
		int i = 0;
		for (SubscriberNode *node = head; node != nullptr; node = node->next) {
			Process *proc = node->proc;
			debug(9, "[",i,"]: ", (hex) proc);
		}
		debug(9, "End of Subscriber List");
	}
};



template <int N=0>
struct DisplayWorker: public Worker {
	SharedQueue<DisplayMsg, N> event_queue;
	unsigned long long deadline;

	DisplayWorker() {
		unsigned long long tsc = __builtin_ia32_rdtsc();
		deadline = tsc + 0x4000000ULL;
	}

	virtual void run() {
		unsigned long long tsc = __builtin_ia32_rdtsc();

		// throttle this worker:
		if (tsc < deadline) {
			return;
		}
		// reset new deadline:
		deadline = tsc + 0x4000000ULL;

		event_queue.gather_new_data();

		if (event_queue.size() <= 0) {
			return;
		}

		put_pixels(mousePointer->bg_x, mousePointer->bg_y,
			POINTER_WIDTH, POINTER_HEIGHT, mousePointer->background);

		QueueRange qr = this->event_queue.dequeue(10);


		for (int i = qr.head; i < qr.tail; i++) {
			//Message *msg = (Message *) &this->event_queue.queue[i % this->event_queue.max_size];
			DisplayMsg *msg = (DisplayMsg *) this->event_queue.get(i);

			switch (msg->event) {
				case DisplayEvents::REDRAW_SCREEN: {
					gui_redraw_rect(&winmgr->desktop->windata.rect, winmgr->winstack.top);
					break;
				}
				case DisplayEvents::REDRAW_WINDOW: {
					GUIWindow *win = (GUIWindow *) msg->redraw_win;
					if (win != nullptr) {
						Rect clip = winmgr->desktop->windata.rect;
						if (rect_intersect(&win->windata.rect, &clip)) {
							gui_redraw_rect(&clip, win);
						}
					}
					break;
				}
				case DisplayEvents::REDRAW_RECT: {
					Rect clip = winmgr->desktop->windata.rect;
					if (rect_intersect(&msg->update_rect, &clip)) {
						gui_redraw_rect(&clip, winmgr->winstack.top);
					}
					break;
				}
				default: break;
			}

		}

		this->event_queue.release(qr);
		this->event_queue.garbage_collect();
		(*procs[0].msg_signal) -= (qr.tail - qr.head);

		get_pixels(mouse.x, mouse.y,
			POINTER_WIDTH, POINTER_HEIGHT, mousePointer->background);
		mousePointer->bg_x = mouse.x;
		mousePointer->bg_y = mouse.y;

		put_pixels(mouse.x, mouse.y,
			POINTER_WIDTH, POINTER_HEIGHT, mousePointer->image);

	}
};

template <int N=0>
struct UserEventWorker: public Worker {
	SharedMsgQueue<N> event_queue;
	// pool of subscriber nodes to allocate from
	PoolAllocator<SubscriberNode, MAX_SUBSCRIPTIONS> sub_pool;

	// list of processes that subscribe to each event type:
	SubscriberList event_subs[(const int) UserEvents::MAX];

	int sub_proc_event(Process *proc, UserEvents event) {
	// subscribe a process to an event type
		SubscriberNode *node = sub_pool.alloc();
		if (node == nullptr) return -1;
		node->proc = proc;
		event_subs[(int) event].push_head(node);
		return 0;
	}
	
	virtual void run() {
		event_queue.gather_new_data();

		if (event_queue.size() <= 0) {
			return;
		}
		QueueRange qr = this->event_queue.dequeue(10);
		for (int i = qr.head; i < qr.tail; i++) {

			Message *msg = (Message *) this->event_queue.get(i);


			switch ((UserEvents) msg->msg_id) {

			case (UserEvents::MOUSE_MOVE): {
				MouseEvent *m_event = (MouseEvent *) &msg->data;

				int dx = mouse.x - mouse.old_x;
				int dy = mouse.y - mouse.old_y;
				mouse.old_x = mouse.x;
				mouse.old_y = mouse.y;

				if (dx == 0 && dy == 0) break;

				if (mouse.ldrag) {
					GUIWindow *top = winmgr->winstack.top;
					if (winmgr->drag_window && winmgr->drag_window != winmgr->desktop) {
					//if (top != winmgr->desktop) {
						
						Rect old = winmgr->drag_window->windata.rect;

						Rect redraw_rects[3];
						winmgr->drag_window->move(dx, dy);

						int num_rects = rect_union(&old, &winmgr->drag_window->windata.rect, redraw_rects);

						for (int i = 0; i < num_rects; i++) {
							Rect *rect = &redraw_rects[i];
							if ((rect->l == rect->r) || (rect->t == rect->b)) continue;
							DisplayMsg dmsg;
							dmsg.event = DisplayEvents::REDRAW_RECT;
							dmsg.update_rect = *rect;
							new_display_msg(dmsg);
						}
					}
				}
				new_display_msg({DisplayEvents::REDRAW_MOUSE});
				break;
			}
				
			case (UserEvents::MOUSE_LEFT_DOWN): {

				MouseEvent *m_event = (MouseEvent *) &msg->data;

				debug(0, "MOUSE DOWN: ", (int) m_event->x, ", ", (int) m_event->y);

				// figure out which window was clicked:
				int i = VGA_PIXEL_OFFSET(m_event->x, m_event->y);

				int win_id = (int) winmgr->pick_buffer[i];

				GUIWindow *gw = winmgr->getGUIWindow(win_id);

				if (gw && win_id != 0) {
					// Clicked window is not the desktop 

					winmgr->drag_window = gw->wm_root;

					if (gw->wm_root != winmgr->winstack.top) {
						// if the window is not already on top, bring to top:
						winmgr->winstack.detach(gw->wm_root);
						winmgr->winstack.push(gw->wm_root);


						DisplayMsg msg;
						msg.event = DisplayEvents::REDRAW_WINDOW;
						msg.redraw_win = gw->wm_root;
						new_display_msg(msg);
					}
				} else {
					winmgr->drag_window = nullptr;
				}
				break;
			}
			case (UserEvents::MOUSE_LEFT_UP): {
				winmgr->drag_window = nullptr;
				break;
			}
			case (UserEvents::MOUSE_LEFT_CLICK): {
				break;
			}
			case (UserEvents::MOUSE_LEFT_DRAG_START): {
				break;
			}
			default:
				break;
			}
			GUIWindow *topwin = winmgr->winstack.top;
			if (topwin && topwin->pid != 0) {
				Process *proc = &procs[topwin->pid];
				
				event_subs[msg->msg_id].notify(proc, msg);
			}
		}
		this->event_queue.release(qr);
		this->event_queue.garbage_collect();
		(*procs[0].msg_signal) -= (qr.tail - qr.head);
	}
};

struct EventSystem {
	Worker *workers[MAX_WORKERS];
	int num_workers;

	// built-in workers
	UserEventWorker<MAX_EVENTS> w_user;
	CallbackWorker<MAX_EVENTS> w_callback;
	DisplayWorker<MAX_EVENTS> w_display;

	EventSystem() {
		workers[0] = (Worker *) &w_user;
		workers[1] = (Worker *) &w_callback;
		workers[2] = (Worker *) &w_display;
		num_workers = 3;
	}
};

EventSystem *kevents;

int sub_proc_event(Process *proc, UserEvents event) {
	return kevents->w_user.sub_proc_event(proc, event);
}

void new_user_event(int msg_id, int data) {
	Message msg = (struct Message) {msg_id, data};
	kevents->w_user.event_queue.enqueue(&msg);
	(*procs[0].msg_signal)++;
}

void new_callback_event(void (*handler)(int), int data) {
	Message msg = {(int) handler, data};
	kevents->w_callback.event_queue.enqueue(&msg);
	(*procs[0].msg_signal)++;
}


void new_display_msg(DisplayMsg msg) {
	kevents->w_display.event_queue.enqueue(&msg);
	(*procs[0].msg_signal)++;
}



void enter_event_loop() {
	while (true) {
		procs[0].monitor(MSG_MONITOR, 0);

		for (int i = 0; i< kevents->num_workers; i++) {
			kevents->workers[i]->run();
		}
		yield();
	}
}	




void init_events() {
	uint kevent_pages = (sizeof(EventSystem) + 4095) / 4096;
	kevents = new (static_alloc_pages(kevent_pages)) EventSystem();

	(*procs[0].msg_signal)=0;

	// NOTE if this stack is too small, bad things happen (event_queue above gets overwritten, etc.) 
	uint stack_pages = 4;
	uint stack_bytes = 4096 * stack_pages;
	void *stack = static_alloc_pages(stack_pages);

	kernelEvents = (KernelThread *) &(procs[0].threads[1]);

	kernelEvents->init((void *) enter_event_loop, stack, stack_bytes);
	kernelEvents->runState = RUNNING;

	debug(9, "INITialized kernelEvents");

}

#pragma pop_macro("DEBUG_LEVEL")
