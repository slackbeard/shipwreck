#include <new>
#include <std/types.h>
#include <util/debug.h>
#include <filesystem.h>
#include <events.h>
#include <memory.h>
#include <devices/vga.h>
#include <devices/ps2.h>
#include <gui.h>



void GUIWindow::dump() {
	kprintln("GUIWindow[", pid, ",", windata.id, "]:");
	kprintln("  zindex=", (hex)windata.zindex);
	kprintln("  prev=", (hex)prev, " next=", (hex) next);
	dump_rect(&windata.rect); kprintln();
}

void GUIWindow::update_root(GUIWindow *new_root) {
	wm_root = new_root;
}

void GUIWindow::move(int dx, int dy) {
	windata.move(dx, dy);
}

void GUIWindow::draw(Rect *clip) {
	
	if (GET_ALPHA(windata.color) && wm_root && wm_root->next) {
	// if transparent, first draw everything below the container window:
		gui_redraw_rect(clip, wm_root->next);	
	}
	rect_fill(*clip, windata.color);
	update_pick_buf(clip, this);

}

void GUIWindowFrame::addChild(GUIWindow *child) {
	if (child == nullptr || child == this) return;
	children()->insert(child);
	child->parent = this;
	// offset by parent's x, y:
	child->move(windata.rect.l, windata.rect.t);
}

void GUIWindowFrame::removeChild(GUIWindow *child) {
	if (child == nullptr) return;
	if (child->parent != this) return;

	children()->detach(child);
	child->parent = nullptr;
	child->move(-windata.rect.l, -windata.rect.t);
}

void GUIWindowFrame::update_root(GUIWindow *new_root) {
	GUIWindow::update_root(new_root);

	GUIWindow *child = children()->top;
	for (int i = 0; child && i < MAX_WINDOW_LEVELS; i++, child = child->next)
	{
		child->update_root(new_root);
	}
}

void GUIWindowFrame::move(int dx, int dy) {
	GUIWindow::move(dx, dy);
	GUIWindow *ptr = children()->top;
	for (int i = 0; ptr && i < MAX_WINDOW_LEVELS; ptr = ptr->next, i++) {
		ptr->move(dx, dy);	
	}
}

void GUIWindowFrame::draw(Rect *clip) {
	gui_redraw_rect(clip, children()->top);
}


void GUITextBox::draw(Rect *clip) {

	// render background first
	gui_redraw_rect(clip, wm_root->next);	
	update_pick_buf(clip, this);
	rect_fill(*clip, SET_ALPHA(1) + fast_blend(BLACK, fast_blend(BLACK, GREEN)));
	//rect_line(*clip, BLACK);

	TextBox::Buffer<> *buf = (TextBox::Buffer<> *) windata.display_data;
	if (buf == nullptr) return;

	int virt_x = (windata.rect.l + buf->offset_x);
	int virt_y = (windata.rect.t + buf->offset_y);

	// clip rect translated into text-space (ie. 1 unit is 1 text character)
	Rect text_clip = {
		(clip->l - virt_x) / CHAR_SPACING_X,
		(clip->t - virt_y) / CHAR_SPACING_Y,
		(clip->r - virt_x) / CHAR_SPACING_X,
		(clip->b - virt_y) / CHAR_SPACING_Y
	};

	Rect text_rect = {
		0,
		buf->head / buf->char_w,
		buf->char_w,
		buf->tail / buf->char_w
	};

	if (!rect_intersect(&text_clip, &text_rect)) return;

	for (int y = text_rect.t; y <= text_rect.b; y++) {
		for (int x = text_rect.l; x <= text_rect.r; x++) {
			
			int offset_x = virt_x + x * CHAR_SPACING_X;
			int offset_y = virt_y + y * CHAR_SPACING_Y;

			int text_offset = y * buf->char_w + x;
			char text_char;

			text_char = buf->text[text_offset % buf->max];

			render_char_xy_clipped(
				text_char,
				offset_x, offset_y,
				GREEN,
				clip
			);
		}
	}
}

void GUIDesktop::draw(Rect *clip) {
	update_pick_buf(clip, this);
	ushort *img = (ushort *) windata.display_data;
	for (int y = clip->t; y < clip->b; y++) {
		for (int x = clip->l; x < clip->r; x++) {
			SET_PIXEL(x, y, img[y * VGA_WIDTH + x]);
		}
	}
}


GUIWindow *GUIWindowStack::insert(GUIWindow *new_win) {
// insert window according to its zindex order

	if (new_win == nullptr) return nullptr;

	if (!top || (top->windata.zindex < new_win->windata.zindex)) {
		if (top) top->prev = new_win;
		new_win->prev = nullptr;
		new_win->next = top;
		top = new_win;
		return new_win;
	}

	GUIWindow *ptr = top;	
	for (int i = 0; ptr->next && i < MAX_WINDOW_LEVELS; ptr = ptr->next, i++) {
		if (ptr->next->windata.zindex < new_win->windata.zindex) {
			break;	
		}
	}

	if (ptr->next) {
		ptr->next->prev = new_win;
	}
	new_win->next = ptr->next;
	new_win->prev = ptr;
	ptr->next = new_win;

	return new_win;
}

GUIWindow *GUIWindowStack::push(GUIWindow *new_win) {
// insert window at the top of zindex

	if (new_win == nullptr) return nullptr;

	if (top == nullptr) {
		new_win->prev = nullptr;
		new_win->next = nullptr;
		top = new_win;
		return new_win;
	}

	// possibly unnecessary:
	//new_win->windata.zindex = top->windata.zindex + 1;

	new_win->next = top;
	new_win->prev = nullptr;
	top->prev = new_win;

	top = new_win;
	return new_win;
}

GUIWindow *GUIWindowStack::detach(GUIWindow *win) {

	if (win == nullptr) return nullptr;
	if (win == top) top = win->next;

	if (win->next) {
		win->next->prev = win->prev;
	}
	if (win->prev) {
		win->prev->next = win->next;
	}
	win->next = nullptr;
	win->prev = nullptr;

	return win;
}


void update_pick_buf(Rect *clip, GUIWindow *win) {
	
	// TODO REFACTOR put this calculation in a macro or something
	uchar gid = ((win->pid & 0xF) << 4) | (win->windata.id & 0xF);
	for (int y = clip->t; y < clip->b; y++) {
		for (int x = clip->l; x < clip->r; x++) {
			int i = VGA_PIXEL_OFFSET(x, y);
			winmgr->pick_buffer[i] = gid;
		}
	}
}


void gui_redraw_rect(Rect *clip, GUIWindow *top) {

	GUIWindow *win_node = top;

	Rect sub_rect = *clip; 

	for (int i = 0; win_node && i < MAX_WINDOW_LEVELS; win_node = win_node->next, i++) {
	// find the next highest z-ordered window that intersects this clip rect
		if (rect_intersect(&win_node->windata.rect, &sub_rect)) {
			// sub_rect is now the portion of top window in this clip rect

			break;
		}
	}

	if (!win_node) {

		return;
	}


	if (sub_rect.t > clip->t) {
		// recursively draw area above this window
		Rect new_clip = {clip->l, clip->t, clip->r, sub_rect.t};

		gui_redraw_rect(&new_clip, win_node->next); 

	}
	if (sub_rect.l > clip->l) {
		// recursively draw area left of this window
		Rect new_clip = {clip->l, sub_rect.t, sub_rect.l, sub_rect.b};

		gui_redraw_rect(&new_clip, win_node->next); 

	}

	// draw clipped rect of this window to the display

	win_node->draw(&sub_rect);


	if (sub_rect.r < clip->r) {
		// recursively draw area right of this window
		Rect new_clip = {sub_rect.r, sub_rect.t, clip->r, sub_rect.b};

		gui_redraw_rect(&new_clip, win_node->next); 

	}
	if (sub_rect.b < clip->b) {
		// recursively draw area below this window
		Rect new_clip = {clip->l, sub_rect.b, clip->r, clip->b};

		gui_redraw_rect(&new_clip, win_node->next); 

	}

}

void gui_update_proc(int pid) {
	debug(8, "INSIDE syscall_update_gui");

	Process *proc = &procs[pid];

	for (int i = 0; i < MAX_PROC_WINDOWS; i++) {
		Window *user_win = &proc->env->windows[i];
		Rect *r = &user_win->rect;
		debug(8, " win[",i,"] winclass=", (int) user_win->winclass, " rect=", r->l, ",", r->t, ",", r->r, ",", r->b);
		if (user_win->winclass == WindowClass::NONE) {
			continue;
		}
		if (user_win->needs_update) {
			user_win->needs_update=0;
			// TODO REFACTOR put this calculation in a macro or something
			debug(8, "   needs update");
			int gid = i + (pid << 4);
			GUIWindow *gw = winmgr->getGUIWindow(gid);	

			debug(8, "   GID=",gid, " gw@", (hex) gw);

			if (user_win->winclass != gw->windata.winclass)
			{
			debug(8, "   Class updated, creating new object: ");
				switch (user_win->winclass) {
					case (WindowClass::WINDOW): {
						debug(8, "Window");
						new (gw) GUIWindow();
						break;
					}
					case (WindowClass::WINDOW_FRAME): {
						new (gw) GUIWindowFrame();
						debug(8, "WindowFrame");
						break;
					}
					case (WindowClass::TEXT_BOX): {
						new (gw) GUITextBox();
						debug(8, "TextBox");
						break;
					}
					default: {
						debug(8, "None");
						user_win->winclass = WindowClass::NONE;
						continue;
					}
				}
				// update ownership of this window
				gw->pid = pid;
			}

			// copy the rest from userland -> WM window
			gw->windata = *user_win;

			// adjust pointers for userland -> WM space
			if (user_win->winclass == WindowClass::TEXT_BOX) 
			{
				uint data_offset = ((uint) user_win->display_data - (uint) proc->userEnv);

				gw->windata.display_data = (void *) ((uint) proc->env + data_offset);
			}


			// fix hierarchy
			if (gw->parent != nullptr) {
			// detach from current parent, will re-add later
				gw->parent->removeChild(gw);	
			}

			if (user_win->parent_id != -1) {

				int parent_gid = user_win->parent_id + (pid << 4);

				GUIWindowFrame *new_parent = (GUIWindowFrame *) winmgr->getGUIWindow(parent_gid);	

				if (new_parent->windata.winclass != WindowClass::WINDOW_FRAME) {
					kprintln("New parent window (parent_gid=",parent_gid,") is not a WINDOW_FRAME");
					return;
				}
					
				new_parent->addChild(gw);	

			} else if ((gw->windata.zindex != user_win->zindex) 
				&& (gw->parent != nullptr))
			{
				debug(8, "   Z-index changed");
				debug(8, "   Re-inserting into winstack");

				GUIWindowFrame *parent = gw->parent;

				parent->removeChild(gw);	

				parent->addChild(gw);	
			}

		}
	}
	debug(8, "Done copying user window data");
	// fix window root references:

	debug(8, "Updating window heirarchy:");
	for (int i = 0; i < MAX_PROC_WINDOWS; i++) {
		int gid = i + (pid << 4);
		GUIWindow *gw = winmgr->getGUIWindow(gid);	
		if (gw->windata.winclass == WindowClass::NONE) continue;

		if (gw->parent == nullptr) {
			debug(8, "  win[",i,"] is a wm_root. gw@", (hex) gw); 
			
		// if this window frame has no parent, it's the top (root) container.

			// assign all of its children's `wm_root` to this window:
			gw->update_root(gw);
			debug(8, "  Done updating win[",i,"] root ptrs");

			if (gid != 0 && gw->next == nullptr) {
			// if it's not yet in the window manager's window stack, add it:
			debug(8, "   gw->next = nullptr, adding to winstack");
				winmgr->winstack.push(gw->wm_root);
			}
		}
	}
}

void gui_redraw_proc(int pid) {

	Process *proc = &procs[pid];

	for (int i = 0; i < MAX_PROC_WINDOWS; i++) {
		Window *user_win = &proc->env->windows[i];
		if (user_win->winclass == WindowClass::NONE) {
			continue;
		}
		if (user_win->needs_redraw) {
			user_win->needs_redraw=0;

			int gid = i + (pid << 4);
			GUIWindow *gw = winmgr->getGUIWindow(gid);	

			// TODO REDRAW_RECT is wasteful for this because it will also redraw windows stacked above this one
			
			Rect update_rect={	
				user_win->redraw_rect.l + gw->windata.rect.l,
				user_win->redraw_rect.t + gw->windata.rect.t,
				user_win->redraw_rect.r + gw->windata.rect.l,
				user_win->redraw_rect.b + gw->windata.rect.t
			};
			
			new_display_msg({
				.event=DisplayEvents::REDRAW_RECT,
				.update_rect=update_rect
			});
		}
	}
}

struct __attribute__((packed)) TGAHeader {
   char  idlength;
   char  colourmaptype;
   char  datatypecode;
   short int colourmaporigin;
   short int colourmaplength;
   char  colourmapdepth;
   short int x_origin;
   short int y_origin;
   short width;
   short height;
   char  bitsperpixel;
   char  imagedescriptor;
};

struct __attribute__((packed)) Color24 {
	uchar b, g, r;
};

int load_background_tga(ushort *img_data) {
	TGAHeader header;

	Color24 raw_data[512];

	File *img_file = filesystem->open("desktop.tga");

	img_file->read((char *) &header, sizeof(header));

	if (header.datatypecode != 2) {
		kprintln("Unsupported TGA file");
		return -1;
	}
	int pi=0; //pixel index
	int read_len=0; // bytes read in each iteration
	// x,y scale from disk image -> screen image
	int sx = VGA_WIDTH / header.width;
	int sy = VGA_HEIGHT / header.height;
	do {
		read_len = img_file->read((char *) raw_data, sizeof(raw_data));

		for (int i = 0; i < (read_len/3); i++, pi++) {
			int y = pi / header.width;
			int x = pi % header.width;
			
			// convert colors from 24-bit to 16-bit:
			ColorRGB col16;
			col16.blue = raw_data[i].b >> 3;
			col16.green = raw_data[i].g >> 3;
			col16.red = raw_data[i].r >> 3;

			for (int dx = x * sx; dx < (x + 1) * sx; dx++) {
				for (int dy = y * sy; dy < (y + 1) * sy; dy++) {
					img_data[dy * VGA_WIDTH + dx] = (ushort) col16;
				}
			}
		}
	} while (read_len);
	return 0;
}


WindowManager *winmgr;

MousePointer *mousePointer;

static void generate_cursor_image(ushort (&buf)[POINTER_WIDTH][POINTER_HEIGHT]) {

	const int w = POINTER_WIDTH;
	const int h = POINTER_HEIGHT;
	int w2 = w>>1;
	int h2 = h>>1;
	for (int x = 0; x < w; x++) {
		for (int y = 0; y < h; y++) {
			buf[x][y] = 1;// BLACK;
		}
	}

	int j = 0;
	for (int i = 0; i < w; i++) {
		buf[i][j] = GREY;
		buf[j][i] = GREY;
		if ((i & 1)) j++;
	}	
	for (int i = 0; i < w2; i++) {
		buf[i+w2][w2] = GREY;
		buf[w2][i+w2] = GREY;
	}

	int start_x[w];
	int end_x[w];
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (buf[x][y] != 1) {
				start_x[y] = x;
				break;
			}
		}
		for (int x = (w-1); x >= 0; x--) {
			if (buf[x][y] != 1) {
				end_x[y] = x;
				break;
			}
		}
		for (int x = start_x[y]; x < end_x[y]; x++) {
			if (buf[x][y] == 1) {
				buf[x][y] = WHITE;
			}
		}
	}
}

void init_gui(int width, int height) {

	uint winmgr_pages = (sizeof(WindowManager) + 4095) / 4096;
	winmgr = new (static_alloc_pages(winmgr_pages)) WindowManager();
	
	uint mp_pages = (sizeof(MousePointer) + 4095) / 4096;
	mousePointer = new (static_alloc_pages(mp_pages)) MousePointer();

	mouse.x = width / 4;
	mouse.y = height / 4;

	mouse.old_x = mouse.x;
	mouse.old_y = mouse.y;

	mousePointer->bg_x = mouse.x;
	mousePointer->bg_y = mouse.y;

	generate_cursor_image(mousePointer->image);

	get_pixels(mouse.x, mouse.y, POINTER_WIDTH, POINTER_HEIGHT, mousePointer->background);
}

