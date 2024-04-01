/*************************************************************************/
/*  os_windows.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "drivers/gles2/rasterizer_gles2.h"

#include "drivers/unix/memory_pool_static_malloc.h"
#include "drivers/windows/dir_access_windows.h"
#include "drivers/windows/file_access_windows.h"
#include "drivers/windows/mutex_windows.h"
#include "drivers/windows/semaphore_windows.h"
#include "drivers/windows/thread_windows.h"
#include "main/main.h"
#include "os/memory_pool_dynamic_static.h"
#include "os_windows.h"

#include "scene/resources/texture.h"
#include "servers/audio/audio_server_sw.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include "globals.h"
#include "io/marshalls.h"
#include "joystick.h"
#include "lang_table.h"
#include "os/memory_pool_dynamic_prealloc.h"
#include "packet_peer_udp_winsock.h"
#include "stream_peer_winsock.h"
#include "tcp_server_winsock.h"

#include "shlobj.h"
#include <process.h>
#include <regstr.h>

static const WORD MAX_CONSOLE_LINES = 1500;

extern "C" {
#ifdef _MSC_VER
_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
#else
__attribute__((visibility("default"))) DWORD NvOptimusEnablement = 0x00000001;
#endif
}

// Workaround mingw-w64 < 4.0 bug
#ifndef WM_TOUCH
#define WM_TOUCH 576
#endif

extern HINSTANCE godot_hinstance;

void RedirectIOToConsole() {

	int hConHandle;

	intptr_t lStdHandle;

	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	FILE *fp;

	// allocate a console for this app

	AllocConsole();

	// set the screen buffer to be big enough to let us scroll text

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),

			&coninfo);

	coninfo.dwSize.Y = MAX_CONSOLE_LINES;

	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),

			coninfo.dwSize);

	// redirect unbuffered STDOUT to the console

	lStdHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen(hConHandle, "w");

	*stdout = *fp;

	setvbuf(stdout, NULL, _IONBF, 0);

	// redirect unbuffered STDIN to the console

	lStdHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen(hConHandle, "r");

	*stdin = *fp;

	setvbuf(stdin, NULL, _IONBF, 0);

	// redirect unbuffered STDERR to the console

	lStdHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen(hConHandle, "w");

	*stderr = *fp;

	setvbuf(stderr, NULL, _IONBF, 0);

	// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog

	// point to console as well
}

int OS_Windows::get_video_driver_count() const {

	return 1;
}
const char *OS_Windows::get_video_driver_name(int p_driver) const {

	return "GLES2";
}

OS::VideoMode OS_Windows::get_default_video_mode() const {

	return VideoMode(1024, 600, false);
}

int OS_Windows::get_audio_driver_count() const {

	return AudioDriverManagerSW::get_driver_count();
}
const char *OS_Windows::get_audio_driver_name(int p_driver) const {

	AudioDriverSW *driver = AudioDriverManagerSW::get_driver(p_driver);
	ERR_FAIL_COND_V(!driver, "");
	return AudioDriverManagerSW::get_driver(p_driver)->get_name();
}

static MemoryPoolStatic *mempool_static = NULL;
static MemoryPoolDynamic *mempool_dynamic = NULL;

void OS_Windows::initialize_core() {

	crash_handler.initialize();

	last_button_state = 0;

	//RedirectIOToConsole();
	maximized = false;
	minimized = false;
	borderless = false;

	ThreadWindows::make_default();
	SemaphoreWindows::make_default();
	MutexWindows::make_default();

	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_FILESYSTEM);
	//FileAccessBufferedFA<FileAccessWindows>::make_default();
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_FILESYSTEM);

	TCPServerWinsock::make_default();
	StreamPeerWinsock::make_default();
	PacketPeerUDPWinsock::make_default();

	mempool_static = new MemoryPoolStaticMalloc;
#if 1
	mempool_dynamic = memnew(MemoryPoolDynamicStatic);
#else
#define DYNPOOL_SIZE 4 * 1024 * 1024
	void *buffer = malloc(DYNPOOL_SIZE);
	mempool_dynamic = memnew(MemoryPoolDynamicPrealloc(buffer, DYNPOOL_SIZE));

#endif

	// We need to know how often the clock is updated
	if (!QueryPerformanceFrequency((LARGE_INTEGER *)&ticks_per_second))
		ticks_per_second = 1000;
	// If timeAtGameStart is 0 then we get the time since
	// the start of the computer when we call GetGameTime()
	ticks_start = 0;
	ticks_start = get_ticks_usec();

	process_map = memnew((Map<ProcessID, ProcessInfo>));

	IP_Unix::make_default();

	cursor_shape = CURSOR_ARROW;
}

bool OS_Windows::can_draw() const {

	return !minimized;
};

#define MI_WP_SIGNATURE 0xFF515700
#define SIGNATURE_MASK 0xFFFFFF00
#define IsPenEvent(dw) (((dw)&SIGNATURE_MASK) == MI_WP_SIGNATURE)

void OS_Windows::_touch_event(bool p_pressed, int p_x, int p_y, int idx) {

#if WINVER >= 0x0601 // for windows 7
	// Defensive
	if (touch_state.has(idx) == p_pressed)
		return;

	if (p_pressed) {
		touch_state.insert(idx, Point2i(p_x, p_y));
	} else {
		touch_state.erase(idx);
	}
#endif

	InputEvent event;
	event.type = InputEvent::SCREEN_TOUCH;
	event.ID = ++last_id;
	event.screen_touch.index = idx;

	event.screen_touch.pressed = p_pressed;

	event.screen_touch.x = p_x;
	event.screen_touch.y = p_y;

	if (main_loop) {
		input->parse_input_event(event);
	}
};

void OS_Windows::_drag_event(int p_x, int p_y, int idx) {

#if WINVER >= 0x0601 // for windows 7
	Map<int, Point2i>::Element *curr = touch_state.find(idx);
	// Defensive
	if (!curr)
		return;

	if (curr->get() == Point2i(p_x, p_y))
		return;

	curr->get() = Point2i(p_x, p_y);
#endif

	InputEvent event;
	event.type = InputEvent::SCREEN_DRAG;
	event.ID = ++last_id;
	event.screen_drag.index = idx;

	event.screen_drag.x = p_x;
	event.screen_drag.y = p_y;

	if (main_loop)
		input->parse_input_event(event);
};

LRESULT OS_Windows::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) // Check For Windows Messages
	{
		case WM_ACTIVATE: // Watch For Window Activate Message
		{
			minimized = HIWORD(wParam) != 0;
			if (!main_loop) {
				return 0;
			};
			if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE) {

				main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
				alt_mem = false;
				control_mem = false;
				shift_mem = false;
				if (mouse_mode == MOUSE_MODE_CAPTURED) {
					RECT clipRect;
					GetClientRect(hWnd, &clipRect);
					ClientToScreen(hWnd, (POINT *)&clipRect.left);
					ClientToScreen(hWnd, (POINT *)&clipRect.right);
					ClipCursor(&clipRect);
					SetCapture(hWnd);
				}
			} else {
				main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
				alt_mem = false;
			};

			return 0; // Return To The Message Loop
		}

		case WM_KILLFOCUS: {

#if WINVER >= 0x0601 // for windows 7
			// Release every touch to avoid sticky points
			for (Map<int, Point2i>::Element *E = touch_state.front(); E; E = E->next()) {
				_touch_event(false, E->get().x, E->get().y, E->key());
			}
			touch_state.clear();
#endif
		} break;

		case WM_PAINT:

			Main::force_redraw();
			break;

		case WM_SYSCOMMAND: // Intercept System Commands
		{
			switch (wParam) // Check System Calls
			{
				case SC_SCREENSAVE: // Screensaver Trying To Start?
				case SC_MONITORPOWER: // Monitor Trying To Enter Powersave?
					return 0; // Prevent From Happening
				case SC_KEYMENU:
					if ((lParam >> 16) <= 0)
						return 0;
			}
			break; // Exit
		}

		case WM_CLOSE: // Did We Receive A Close Message?
		{
			if (main_loop)
				main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
			//force_quit=true;
			return 0; // Jump Back
		}
		case WM_MOUSELEAVE: {

			old_invalid = true;
			outside = true;
			if (main_loop && mouse_mode != MOUSE_MODE_CAPTURED)
				main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_EXIT);

		} break;
		case WM_MOUSEMOVE: {

			if (outside) {
				//mouse enter

				if (main_loop && mouse_mode != MOUSE_MODE_CAPTURED)
					main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);

				CursorShape c = cursor_shape;
				cursor_shape = CURSOR_MAX;
				set_cursor_shape(c);
				outside = false;

				//Once-Off notification, must call again....
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				TrackMouseEvent(&tme);
			}

			/*
			LPARAM extra = GetMessageExtraInfo();
			if (IsPenEvent(extra)) {

				int idx = extra & 0x7f;
				_drag_event(idx, uMsg, wParam, lParam);
				if (idx != 0) {
					return 0;
				};
				// fallthrough for mouse event
			};
			*/

			InputEvent event;
			event.type = InputEvent::MOUSE_MOTION;
			event.ID = ++last_id;
			InputEventMouseMotion &mm = event.mouse_motion;

			mm.mod.control = (wParam & MK_CONTROL) != 0;
			mm.mod.shift = (wParam & MK_SHIFT) != 0;
			mm.mod.alt = alt_mem;

			mm.button_mask |= (wParam & MK_LBUTTON) ? (1 << 0) : 0;
			mm.button_mask |= (wParam & MK_RBUTTON) ? (1 << 1) : 0;
			mm.button_mask |= (wParam & MK_MBUTTON) ? (1 << 2) : 0;
			last_button_state = mm.button_mask;
			/*mm.button_mask|=(wParam&MK_XBUTTON1)?(1<<5):0;
			mm.button_mask|=(wParam&MK_XBUTTON2)?(1<<6):0;*/
			mm.x = GET_X_LPARAM(lParam);
			mm.y = GET_Y_LPARAM(lParam);

			if (mouse_mode == MOUSE_MODE_CAPTURED) {

				Point2i c(video_mode.width / 2, video_mode.height / 2);
				old_x = c.x;
				old_y = c.y;

				if (Point2i(mm.x, mm.y) == c) {
					center = c;
					return 0;
				}

				Point2i ncenter(mm.x, mm.y);
				center = ncenter;
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
			}

			input->set_mouse_pos(Point2(mm.x, mm.y));
			mm.global_x = mm.x;
			mm.global_y = mm.y;
			mm.speed_x = input->get_mouse_speed().x;
			mm.speed_y = input->get_mouse_speed().y;

			if (old_invalid) {

				old_x = mm.x;
				old_y = mm.y;
				old_invalid = false;
			}

			mm.relative_x = mm.x - old_x;
			mm.relative_y = mm.y - old_y;
			old_x = mm.x;
			old_y = mm.y;
			if (main_loop)
				input->parse_input_event(event);

		} break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
			/*case WM_XBUTTONDOWN:
		case WM_XBUTTONUP: */
			{

				/*
			LPARAM extra = GetMessageExtraInfo();
			if (IsPenEvent(extra)) {

				int idx = extra & 0x7f;
				_touch_event(idx, uMsg, wParam, lParam);
				if (idx != 0) {
					return 0;
				};
				// fallthrough for mouse event
			};
			*/

				InputEvent event;
				event.type = InputEvent::MOUSE_BUTTON;
				event.ID = ++last_id;
				InputEventMouseButton &mb = event.mouse_button;

				switch (uMsg) {
					case WM_LBUTTONDOWN: {
						mb.pressed = true;
						mb.button_index = 1;
					} break;
					case WM_LBUTTONUP: {
						mb.pressed = false;
						mb.button_index = 1;
					} break;
					case WM_MBUTTONDOWN: {
						mb.pressed = true;
						mb.button_index = 3;

					} break;
					case WM_MBUTTONUP: {
						mb.pressed = false;
						mb.button_index = 3;
					} break;
					case WM_RBUTTONDOWN: {
						mb.pressed = true;
						mb.button_index = 2;
					} break;
					case WM_RBUTTONUP: {
						mb.pressed = false;
						mb.button_index = 2;
					} break;
					case WM_LBUTTONDBLCLK: {

						mb.pressed = true;
						mb.button_index = 1;
						mb.doubleclick = true;
					} break;
					case WM_RBUTTONDBLCLK: {

						mb.pressed = true;
						mb.button_index = 2;
						mb.doubleclick = true;
					} break;
					case WM_MBUTTONDBLCLK: {

						mb.pressed = true;
						mb.button_index = 3;
						mb.doubleclick = true;
					} break;
					case WM_MOUSEWHEEL: {

						mb.pressed = true;
						int motion = (short)HIWORD(wParam);
						if (!motion)
							return 0;

						if (motion > 0)
							mb.button_index = BUTTON_WHEEL_UP;
						else
							mb.button_index = BUTTON_WHEEL_DOWN;

					} break;
					case WM_MOUSEHWHEEL: {

						mb.pressed = true;
						int motion = (short)HIWORD(wParam);
						if (!motion)
							return 0;

						if (motion < 0) {
							mb.button_index = BUTTON_WHEEL_LEFT;
							mb.factor = fabs((double)motion / (double)WHEEL_DELTA);
						} else {
							mb.button_index = BUTTON_WHEEL_RIGHT;
							mb.factor = fabs((double)motion / (double)WHEEL_DELTA);
						}
					} break;
					/*
				case WM_XBUTTONDOWN: {
					mb.pressed=true;
					mb.button_index=(HIWORD(wParam)==XBUTTON1)?6:7;
				} break;
				case WM_XBUTTONUP:
					mb.pressed=true;
					mb.button_index=(HIWORD(wParam)==XBUTTON1)?6:7;
				} break;*/
					default: { return 0; }
				}

				mb.mod.control = (wParam & MK_CONTROL) != 0;
				mb.mod.shift = (wParam & MK_SHIFT) != 0;
				mb.mod.alt = alt_mem;
				//mb.mod.alt=(wParam&MK_MENU)!=0;
				mb.button_mask |= (wParam & MK_LBUTTON) ? (1 << 0) : 0;
				mb.button_mask |= (wParam & MK_RBUTTON) ? (1 << 1) : 0;
				mb.button_mask |= (wParam & MK_MBUTTON) ? (1 << 2) : 0;

				last_button_state = mb.button_mask;
				/*
			mb.button_mask|=(wParam&MK_XBUTTON1)?(1<<5):0;
			mb.button_mask|=(wParam&MK_XBUTTON2)?(1<<6):0;*/
				mb.x = GET_X_LPARAM(lParam);
				mb.y = GET_Y_LPARAM(lParam);

				if (mouse_mode == MOUSE_MODE_CAPTURED) {

					mb.x = old_x;
					mb.y = old_y;
				}

				if (uMsg != WM_MOUSEWHEEL && uMsg != WM_MOUSEHWHEEL) {
					if (mb.pressed) {

						if (++pressrc > 0)
							SetCapture(hWnd);
					} else {

						if (--pressrc <= 0) {
							ReleaseCapture();
							pressrc = 0;
						}
					}
				} else if (mouse_mode != MOUSE_MODE_CAPTURED) {
					// for reasons unknown to mankind, wheel comes in screen cordinates
					POINT coords;
					coords.x = mb.x;
					coords.y = mb.y;

					ScreenToClient(hWnd, &coords);

					mb.x = coords.x;
					mb.y = coords.y;
				}

				mb.global_x = mb.x;
				mb.global_y = mb.y;

				if (main_loop) {
					input->parse_input_event(event);
					if (mb.pressed && mb.button_index > 3) {
						//send release for mouse wheel
						mb.pressed = false;
						event.ID = ++last_id;
						input->parse_input_event(event);
					}
				}
			}
			break;

		case WM_SIZE: {
			int window_w = LOWORD(lParam);
			int window_h = HIWORD(lParam);
			if (window_w > 0 && window_h > 0) {
				video_mode.width = window_w;
				video_mode.height = window_h;
			}
			//return 0;								// Jump Back
		} break;

		case WM_ENTERSIZEMOVE: {
			move_timer_id = SetTimer(hWnd, 1, USER_TIMER_MINIMUM, (TIMERPROC)NULL);
		} break;
		case WM_EXITSIZEMOVE: {
			KillTimer(hWnd, move_timer_id);
		} break;
		case WM_TIMER: {
			if (wParam == move_timer_id) {
				process_key_events();
				Main::iteration();
			}
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_KEYDOWN: {

			if (wParam == VK_SHIFT)
				shift_mem = uMsg == WM_KEYDOWN;
			if (wParam == VK_CONTROL)
				control_mem = uMsg == WM_KEYDOWN;
			if (wParam == VK_MENU) {
				alt_mem = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
				if (lParam & (1 << 24))
					gr_mem = alt_mem;
			}

			//if (wParam==VK_WIN) TODO wtf is this?
			//	meta_mem=uMsg==WM_KEYDOWN;

		} //fallthrough
		case WM_CHAR: {

			ERR_BREAK(key_event_pos >= KEY_EVENT_BUFFER_SIZE);

			// Make sure we don't include modifiers for the modifier key itself.
			KeyEvent ke;
			ke.mod_state.shift = (wParam != VK_SHIFT) ? shift_mem : false;
			ke.mod_state.alt = (!(wParam == VK_MENU && (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN))) ? alt_mem : false;
			ke.mod_state.control = (wParam != VK_CONTROL) ? control_mem : false;
			ke.mod_state.meta = meta_mem;
			ke.uMsg = uMsg;

			if (ke.uMsg == WM_SYSKEYDOWN)
				ke.uMsg = WM_KEYDOWN;
			if (ke.uMsg == WM_SYSKEYUP)
				ke.uMsg = WM_KEYUP;

			/*if (ke.uMsg==WM_KEYDOWN && alt_mem && uMsg!=WM_SYSKEYDOWN) {
				//altgr hack for intl keyboards, not sure how good it is
				//windows is weeeeird
				ke.mod_state.alt=false;
				ke.mod_state.control=false;
				print_line("")
			}*/

			ke.wParam = wParam;
			ke.lParam = lParam;
			key_event_buffer[key_event_pos++] = ke;

		} break;
		case WM_INPUTLANGCHANGEREQUEST: {

			print_line("input lang change");
		} break;

#if WINVER >= 0x0601 // for windows 7
		case WM_TOUCH: {

			BOOL bHandled = FALSE;
			UINT cInputs = LOWORD(wParam);
			PTOUCHINPUT pInputs = memnew_arr(TOUCHINPUT, cInputs);
			if (pInputs) {
				if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))) {
					for (UINT i = 0; i < cInputs; i++) {
						TOUCHINPUT ti = pInputs[i];
						//do something with each touch input entry
						if (ti.dwFlags & TOUCHEVENTF_MOVE) {

							_drag_event(ti.x / 100, ti.y / 100, ti.dwID);
						} else if (ti.dwFlags & (TOUCHEVENTF_UP | TOUCHEVENTF_DOWN)) {

							_touch_event(ti.dwFlags & TOUCHEVENTF_DOWN, ti.x / 100, ti.y / 100, ti.dwID);
						};
					}
					bHandled = TRUE;
				} else {
					/* handle the error here */
				}
				memdelete_arr(pInputs);
			} else {
				/* handle the error here, probably out of memory */
			}
			if (bHandled) {
				CloseTouchInputHandle((HTOUCHINPUT)lParam);
				return 0;
			};

		} break;

#endif
		case WM_DEVICECHANGE: {

			joystick->probe_joysticks();
		} break;
		case WM_SETCURSOR: {

			if (LOWORD(lParam) == HTCLIENT) {
				if (mouse_mode == MOUSE_MODE_HIDDEN || mouse_mode == MOUSE_MODE_CAPTURED) {
					//Hide the cursor
					if (hCursor == NULL)
						hCursor = SetCursor(NULL);
					else
						SetCursor(NULL);
				} else {
					if (hCursor != NULL) {
						CursorShape c = cursor_shape;
						cursor_shape = CURSOR_MAX;
						set_cursor_shape(c);
						hCursor = NULL;
					}
				}
			}

		} break;
		case WM_DROPFILES: {

			HDROP hDropInfo = NULL;
			hDropInfo = (HDROP)wParam;
			const int buffsize = 4096;
			wchar_t buf[buffsize];

			int fcount = DragQueryFileW(hDropInfo, 0xFFFFFFFF, NULL, 0);

			Vector<String> files;

			for (int i = 0; i < fcount; i++) {

				DragQueryFileW(hDropInfo, i, buf, buffsize);
				String file = buf;
				files.push_back(file);
			}

			if (files.size() && main_loop) {
				main_loop->drop_files(files, 0);
			}

		} break;

		default: {

			if (user_proc) {

				return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
			};
		};
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	OS_Windows *os_win = static_cast<OS_Windows *>(OS::get_singleton());
	if (os_win)
		return os_win->WndProc(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void OS_Windows::process_key_events() {

	for (int i = 0; i < key_event_pos; i++) {

		KeyEvent &ke = key_event_buffer[i];
		switch (ke.uMsg) {

			case WM_CHAR: {
				if ((i == 0 && ke.uMsg == WM_CHAR) || (i > 0 && key_event_buffer[i - 1].uMsg == WM_CHAR)) {
					InputEvent event;
					event.type = InputEvent::KEY;
					event.ID = ++last_id;
					InputEventKey &k = event.key;

					k.mod = ke.mod_state;
					k.pressed = true;
					k.scancode = KeyMappingWindows::get_keysym(ke.wParam);
					k.unicode = ke.wParam;
					if (k.unicode && gr_mem) {
						k.mod.alt = false;
						k.mod.control = false;
					}

					if (k.unicode < 32)
						k.unicode = 0;

					input->parse_input_event(event);
				}

				//do nothing
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN: {

				InputEvent event;
				event.type = InputEvent::KEY;
				event.ID = ++last_id;
				InputEventKey &k = event.key;

				k.mod = ke.mod_state;
				k.pressed = (ke.uMsg == WM_KEYDOWN);

				if ((ke.lParam & (1 << 24)) && (ke.wParam == VK_RETURN)) {
					// Special case for Numpad Enter key
					k.scancode = KEY_ENTER;
				} else {
					k.scancode = KeyMappingWindows::get_keysym(ke.wParam);
				}

				if (i + 1 < key_event_pos && key_event_buffer[i + 1].uMsg == WM_CHAR)
					k.unicode = key_event_buffer[i + 1].wParam;
				if (k.unicode && gr_mem) {
					k.mod.alt = false;
					k.mod.control = false;
				}

				if (k.unicode < 32)
					k.unicode = 0;

				k.echo = (ke.uMsg == WM_KEYDOWN && (ke.lParam & (1 << 30)));

				input->parse_input_event(event);

			} break;
		}
	}

	key_event_pos = 0;
}

enum _MonitorDpiType {
	MDT_Effective_DPI = 0,
	MDT_Angular_DPI = 1,
	MDT_Raw_DPI = 2,
	MDT_Default = MDT_Effective_DPI
};

static int QueryDpiForMonitor(HMONITOR hmon, _MonitorDpiType dpiType = MDT_Default) {

	int dpiX = 96, dpiY = 96;

	static HMODULE Shcore = NULL;
	typedef HRESULT(WINAPI * GetDPIForMonitor_t)(HMONITOR hmonitor, _MonitorDpiType dpiType, UINT * dpiX, UINT * dpiY);
	static GetDPIForMonitor_t getDPIForMonitor = NULL;

	if (Shcore == NULL) {
		Shcore = LoadLibraryW(L"Shcore.dll");
		getDPIForMonitor = Shcore ? (GetDPIForMonitor_t)GetProcAddress(Shcore, "GetDpiForMonitor") : NULL;

		if ((Shcore == NULL) || (getDPIForMonitor == NULL)) {
			if (Shcore)
				FreeLibrary(Shcore);
			Shcore = (HMODULE)INVALID_HANDLE_VALUE;
		}
	}

	UINT x = 0, y = 0;
	HRESULT hr = E_FAIL;
	bool bSet = false;
	if (hmon && (Shcore != (HMODULE)INVALID_HANDLE_VALUE)) {
		hr = getDPIForMonitor(hmon, dpiType /*MDT_Effective_DPI*/, &x, &y);
		if (SUCCEEDED(hr) && (x > 0) && (y > 0)) {

			dpiX = (int)x;
			dpiY = (int)y;
		}
	} else {
		static int overallX = 0, overallY = 0;
		if (overallX <= 0 || overallY <= 0) {
			HDC hdc = GetDC(NULL);
			if (hdc) {
				overallX = GetDeviceCaps(hdc, LOGPIXELSX);
				overallY = GetDeviceCaps(hdc, LOGPIXELSY);
				ReleaseDC(NULL, hdc);
			}
		}
		if (overallX > 0 && overallY > 0) {
			dpiX = overallX;
			dpiY = overallY;
		}
	}

	return (dpiX + dpiY) / 2;
}

typedef enum _SHC_PROCESS_DPI_AWARENESS {
  SHC_PROCESS_DPI_UNAWARE            = 0,
  SHC_PROCESS_SYSTEM_DPI_AWARE       = 1,
  SHC_PROCESS_PER_MONITOR_DPI_AWARE  = 2
} SHC_PROCESS_DPI_AWARENESS;


void OS_Windows::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {

	main_loop = NULL;
	outside = true;

	WNDCLASSEXW wc;

	if (is_hidpi_allowed()) {
		HMODULE Shcore = LoadLibraryW(L"Shcore.dll");;

		if (Shcore != NULL) {
			typedef HRESULT (WINAPI *SetProcessDpiAwareness_t)(SHC_PROCESS_DPI_AWARENESS);

			SetProcessDpiAwareness_t  SetProcessDpiAwareness = (SetProcessDpiAwareness_t)GetProcAddress(Shcore, "SetProcessDpiAwareness");

			if (SetProcessDpiAwareness) {
				SetProcessDpiAwareness(SHC_PROCESS_SYSTEM_DPI_AWARE);
			}
		}
	}


	video_mode = p_desired;
	//printf("**************** desired %s, mode %s\n", p_desired.fullscreen?"true":"false", video_mode.fullscreen?"true":"false");
	RECT WindowRect;

	WindowRect.left = 0;
	WindowRect.right = video_mode.width;
	WindowRect.top = 0;
	WindowRect.bottom = video_mode.height;

	memset(&wc, 0, sizeof(WNDCLASSEXW));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)::WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	//wc.hInstance = hInstance;
	wc.hInstance = godot_hinstance ? godot_hinstance : GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"Engine";

	if (!RegisterClassExW(&wc)) {
		MessageBox(NULL, "Failed To Register The Window Class.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return; // Return
	}

	pre_fs_valid = true;
	if (video_mode.fullscreen) {

		DEVMODE current;
		memset(&current, 0, sizeof(current));
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current);

		WindowRect.right = current.dmPelsWidth;
		WindowRect.bottom = current.dmPelsHeight;

		/*  DEVMODE dmScreenSettings;
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
		dmScreenSettings.dmSize=sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth	= video_mode.width;
		dmScreenSettings.dmPelsHeight	= video_mode.height;
		dmScreenSettings.dmBitsPerPel	= current.dmBitsPerPel;
		dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		LONG err = ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN);
		if (err!=DISP_CHANGE_SUCCESSFUL) {

			video_mode.fullscreen=false;
		}*/
		pre_fs_valid = false;
	}

	DWORD dwExStyle;
	DWORD dwStyle;

	if (video_mode.fullscreen || video_mode.borderless_window) {

		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP;

	} else {
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPEDWINDOW;
		if (!video_mode.resizable) {
			dwStyle &= ~WS_THICKFRAME;
			dwStyle &= ~WS_MAXIMIZEBOX;
		}
	}

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);

	char *windowid = getenv("GODOT_WINDOWID");
	if (windowid) {

// strtoull on mingw
#ifdef MINGW_ENABLED
		hWnd = (HWND)strtoull(windowid, NULL, 0);
#else
		hWnd = (HWND)_strtoui64(windowid, NULL, 0);
#endif
		SetLastError(0);
		user_proc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)::WndProc);
		DWORD le = GetLastError();
		if (user_proc == 0 && le != 0) {

			printf("Error setting WNDPROC: %li\n", le);
		};
		LONG_PTR proc = GetWindowLongPtr(hWnd, GWLP_WNDPROC);

		RECT rect;
		if (!GetClientRect(hWnd, &rect)) {
			MessageBoxW(NULL, L"Window Creation Error.", L"ERROR", MB_OK | MB_ICONEXCLAMATION);
			return; // Return FALSE
		};
		video_mode.width = rect.right;
		video_mode.height = rect.bottom;
		video_mode.fullscreen = false;
	} else {

		if (!(hWnd = CreateWindowExW(dwExStyle, L"Engine", L"", dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, (GetSystemMetrics(SM_CXSCREEN) - WindowRect.right) / 2, (GetSystemMetrics(SM_CYSCREEN) - WindowRect.bottom) / 2, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, NULL, NULL, hInstance, NULL))) {
			MessageBoxW(NULL, L"Window Creation Error.", L"ERROR", MB_OK | MB_ICONEXCLAMATION);
			return; // Return FALSE
		}
	};

	if (video_mode.always_on_top) {
		SetWindowPos(hWnd, video_mode.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

#if defined(OPENGL_ENABLED) || defined(GLES2_ENABLED) || defined(LEGACYGL_ENABLED)
	gl_context = memnew(ContextGL_Win(hWnd, false));
	gl_context->initialize();
	rasterizer = memnew(RasterizerGLES2);
#else
#ifdef DX9_ENABLED
	rasterizer = memnew(RasterizerDX9(hWnd));
#endif
#endif

	visual_server = memnew(VisualServerRaster(rasterizer));
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {

		visual_server = memnew(VisualServerWrapMT(visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}

	//
	physics_server = memnew(PhysicsServerSW);
	physics_server->init();

	physics_2d_server = Physics2DServerWrapMT::init_server<Physics2DServerSW>();
	physics_2d_server->init();

	if (!is_no_window_mode_enabled()) {
		ShowWindow(hWnd, SW_SHOW); // Show The Window
		SetForegroundWindow(hWnd); // Slightly Higher Priority
		SetFocus(hWnd); // Sets Keyboard Focus To
	}

	/*
		DEVMODE dmScreenSettings;					// Device Mode
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));		// Makes Sure Memory's Cleared
		dmScreenSettings.dmSize=sizeof(dmScreenSettings);		// Size Of The Devmode Structure
		dmScreenSettings.dmPelsWidth	= width;			// Selected Screen Width
		dmScreenSettings.dmPelsHeight	= height;			// Selected Screen Height
		dmScreenSettings.dmBitsPerPel	= bits;				// Selected Bits Per Pixel
		dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
		if (ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN)!=DISP_CHANGE_SUCCESSFUL)




  */
	visual_server->init();

	input = memnew(InputDefault);
	joystick = memnew(joystick_windows(input, &hWnd));

	AudioDriverManagerSW::get_driver(p_audio_driver)->set_singleton();

	if (AudioDriverManagerSW::get_driver(p_audio_driver)->init() != OK) {

		ERR_PRINT("Initializing audio failed.");
	}

	sample_manager = memnew(SampleManagerMallocSW);
	audio_server = memnew(AudioServerSW(sample_manager));

	audio_server->init();

	spatial_sound_server = memnew(SpatialSoundServerSW);
	spatial_sound_server->init();
	spatial_sound_2d_server = memnew(SpatialSound2DServerSW);
	spatial_sound_2d_server->init();

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hWnd;
	tme.dwHoverTime = HOVER_DEFAULT;
	TrackMouseEvent(&tme);

#if WINVER >= 0x0601 // for windows 7
	RegisterTouchWindow(hWnd, 0); // Windows 7
#endif

	_ensure_data_dir();

	DragAcceptFiles(hWnd, true);

	move_timer_id = 1;
}

void OS_Windows::set_clipboard(const String &p_text) {

	if (!OpenClipboard(hWnd)) {
		ERR_EXPLAIN("Unable to open clipboard.");
		ERR_FAIL();
	};
	EmptyClipboard();

	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (p_text.length() + 1) * sizeof(CharType));
	if (mem == NULL) {
		ERR_EXPLAIN("Unable to allocate memory for clipboard contents.");
		ERR_FAIL();
	};
	LPWSTR lptstrCopy = (LPWSTR)GlobalLock(mem);
	memcpy(lptstrCopy, p_text.c_str(), (p_text.length() + 1) * sizeof(CharType));
	//memset((lptstrCopy + p_text.length()), 0, sizeof(CharType));
	GlobalUnlock(mem);

	SetClipboardData(CF_UNICODETEXT, mem);

	// set the CF_TEXT version (not needed?)
	CharString utf8 = p_text.utf8();
	mem = GlobalAlloc(GMEM_MOVEABLE, utf8.length() + 1);
	if (mem == NULL) {
		ERR_EXPLAIN("Unable to allocate memory for clipboard contents.");
		ERR_FAIL();
	};
	LPTSTR ptr = (LPTSTR)GlobalLock(mem);
	memcpy(ptr, utf8.get_data(), utf8.length());
	ptr[utf8.length()] = 0;
	GlobalUnlock(mem);

	SetClipboardData(CF_TEXT, mem);

	CloseClipboard();
};

String OS_Windows::get_clipboard() const {

	String ret;
	if (!OpenClipboard(hWnd)) {
		ERR_EXPLAIN("Unable to open clipboard.");
		ERR_FAIL_V("");
	};

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {

		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != NULL) {

			LPWSTR ptr = (LPWSTR)GlobalLock(mem);
			if (ptr != NULL) {

				ret = String((CharType *)ptr);
				GlobalUnlock(mem);
			};
		};

	} else if (IsClipboardFormatAvailable(CF_TEXT)) {

		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != NULL) {

			LPTSTR ptr = (LPTSTR)GlobalLock(mem);
			if (ptr != NULL) {

				ret.parse_utf8((const char *)ptr);
				GlobalUnlock(mem);
			};
		};
	};

	CloseClipboard();

	return ret;
};

void OS_Windows::delete_main_loop() {

	if (main_loop)
		memdelete(main_loop);
	main_loop = NULL;
}

void OS_Windows::set_main_loop(MainLoop *p_main_loop) {

	input->set_main_loop(p_main_loop);
	main_loop = p_main_loop;
}

void OS_Windows::finalize() {

	if (main_loop)
		memdelete(main_loop);

	main_loop = NULL;

	memdelete(joystick);
	memdelete(input);
#if WINVER >= 0x0601 // for windows 7
	touch_state.clear();
#endif

	visual_server->finish();
	memdelete(visual_server);
#ifdef OPENGL_ENABLED
	if (gl_context)
		memdelete(gl_context);
#endif
	if (rasterizer)
		memdelete(rasterizer);

	if (user_proc) {
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)user_proc);
	};

	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

	//if (debugger_connection_console) {
	//		memdelete(debugger_connection_console);
	//}

	memdelete(sample_manager);

	audio_server->finish();
	memdelete(audio_server);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);
}
void OS_Windows::finalize_core() {

	memdelete(process_map);

	if (mempool_dynamic)
		memdelete(mempool_dynamic);
	delete mempool_static;

	TCPServerWinsock::cleanup();
	StreamPeerWinsock::cleanup();
}

void OS_Windows::vprint(const char *p_format, va_list p_list, bool p_stderr) {

	const unsigned int BUFFER_SIZE = 16384;
	char buf[BUFFER_SIZE + 1]; // +1 for the terminating character
	int len = vsnprintf(buf, BUFFER_SIZE, p_format, p_list);
	if (len <= 0)
		return;
	if (len >= BUFFER_SIZE)
		len = BUFFER_SIZE; // Output is too big, will be truncated
	buf[len] = 0;

	int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
	if (wlen < 0)
		return;

	wchar_t *wbuf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, buf, len, wbuf, wlen);
	wbuf[wlen] = 0;

	if (p_stderr)
		fwprintf(stderr, L"%ls", wbuf);
	else
		wprintf(L"%ls", wbuf);

#ifdef STDOUT_FILE
//vwfprintf(stdo,p_format,p_list);
#endif
	free(wbuf);

	fflush(stdout);
};

void OS_Windows::alert(const String &p_alert, const String &p_title) {

	if (!is_no_window_mode_enabled())
		MessageBoxW(NULL, p_alert.c_str(), p_title.c_str(), MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
	else
		print_line("ALERT: " + p_alert);
}

void OS_Windows::set_mouse_mode(MouseMode p_mode) {

	if (mouse_mode == p_mode)
		return;
	mouse_mode = p_mode;
	if (p_mode == MOUSE_MODE_CAPTURED) {
		RECT clipRect;
		GetClientRect(hWnd, &clipRect);
		ClientToScreen(hWnd, (POINT *)&clipRect.left);
		ClientToScreen(hWnd, (POINT *)&clipRect.right);
		ClipCursor(&clipRect);
		SetCapture(hWnd);
		center = Point2i(video_mode.width / 2, video_mode.height / 2);
		POINT pos = { (int)center.x, (int)center.y };
		ClientToScreen(hWnd, &pos);
		SetCursorPos(pos.x, pos.y);
	} else {
		ReleaseCapture();
		ClipCursor(NULL);
	}

	if (p_mode == MOUSE_MODE_CAPTURED || p_mode == MOUSE_MODE_HIDDEN) {
		hCursor = SetCursor(NULL);
	} else {
		CursorShape c = cursor_shape;
		cursor_shape = CURSOR_MAX;
		set_cursor_shape(c);
	}
}

OS_Windows::MouseMode OS_Windows::get_mouse_mode() const {

	return mouse_mode;
}

void OS_Windows::warp_mouse_pos(const Point2 &p_to) {

	if (mouse_mode == MOUSE_MODE_CAPTURED) {

		old_x = p_to.x;
		old_y = p_to.y;
	} else {

		POINT p;
		p.x = p_to.x;
		p.y = p_to.y;
		ClientToScreen(hWnd, &p);

		SetCursorPos(p.x, p.y);
	}
}

Point2 OS_Windows::get_mouse_pos() const {

	return Point2(old_x, old_y);
}

int OS_Windows::get_mouse_button_state() const {

	return last_button_state;
}

void OS_Windows::set_window_title(const String &p_title) {

	SetWindowTextW(hWnd, p_title.c_str());
}

void OS_Windows::set_video_mode(const VideoMode &p_video_mode, int p_screen) {
}

OS::VideoMode OS_Windows::get_video_mode(int p_screen) const {

	return video_mode;
}
void OS_Windows::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
}

static BOOL CALLBACK _MonitorEnumProcCount(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {

	int *data = (int *)dwData;
	(*data)++;
	return TRUE;
}

int OS_Windows::get_screen_count() const {

	int data = 0;
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcCount, (LPARAM)&data);
	return data;
}

typedef struct {
	int count;
	int screen;
	HMONITOR monitor;
} EnumScreenData;

static BOOL CALLBACK _MonitorEnumProcScreen(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {

	EnumScreenData *data = (EnumScreenData *)dwData;
	if (data->monitor == hMonitor) {
		data->screen = data->count;
	}

	data->count++;
	return TRUE;
}

int OS_Windows::get_current_screen() const {

	EnumScreenData data = { 0, 0, MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST) };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcScreen, (LPARAM)&data);
	return data.screen;
}

void OS_Windows::set_current_screen(int p_screen) {

	Vector2 ofs = get_window_position() - get_screen_position(get_current_screen());
	set_window_position(ofs + get_screen_position(p_screen));
}

typedef struct {
	int count;
	int screen;
	Point2 pos;
} EnumPosData;

static BOOL CALLBACK _MonitorEnumProcPos(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {

	EnumPosData *data = (EnumPosData *)dwData;
	if (data->count == data->screen) {
		data->pos.x = lprcMonitor->left;
		data->pos.y = lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

Point2 OS_Windows::get_screen_position(int p_screen) const {

	EnumPosData data = { 0, p_screen, Point2() };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcPos, (LPARAM)&data);
	return data.pos;
}

typedef struct {
	int count;
	int screen;
	Size2 size;
} EnumSizeData;

static BOOL CALLBACK _MonitorEnumProcSize(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {

	EnumSizeData *data = (EnumSizeData *)dwData;
	if (data->count == data->screen) {
		data->size.x = lprcMonitor->right - lprcMonitor->left;
		data->size.y = lprcMonitor->bottom - lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

Size2 OS_Windows::get_screen_size(int p_screen) const {

	EnumSizeData data = { 0, p_screen, Size2() };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcSize, (LPARAM)&data);
	return data.size;
}

typedef struct {
	int count;
	int screen;
	int dpi;
} EnumDpiData;

static BOOL CALLBACK _MonitorEnumProcDpi(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {

	EnumDpiData *data = (EnumDpiData *)dwData;
	if (data->count == data->screen) {
		data->dpi = QueryDpiForMonitor(hMonitor);
	}

	data->count++;
	return TRUE;
}

int OS_Windows::get_screen_dpi(int p_screen) const {

	EnumDpiData data = { 0, p_screen, 72 };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcDpi, (LPARAM)&data);
	return data.dpi;
}

Point2 OS_Windows::get_window_position() const {

	RECT r;
	GetWindowRect(hWnd, &r);
	return Point2(r.left, r.top);
}
void OS_Windows::set_window_position(const Point2 &p_position) {

	if (video_mode.fullscreen) return;
	RECT r;
	GetWindowRect(hWnd, &r);
	MoveWindow(hWnd, p_position.x, p_position.y, r.right - r.left, r.bottom - r.top, TRUE);
}
Size2 OS_Windows::get_window_size() const {

	RECT r;
	GetClientRect(hWnd, &r);
	return Vector2(r.right - r.left, r.bottom - r.top);
}
Size2 OS_Windows::get_real_window_size() const {

	RECT r;
	GetWindowRect(hWnd, &r);
	return Vector2(r.right - r.left, r.bottom - r.top);
}
void OS_Windows::set_window_size(const Size2 p_size) {

	video_mode.width = p_size.width;
	video_mode.height = p_size.height;

	if (video_mode.fullscreen) {
		return;
	}

	int w = p_size.width;
	int h = p_size.height;

	RECT rect;
	GetWindowRect(hWnd, &rect);

	if (video_mode.borderless_window == false) {
		RECT crect;
		GetClientRect(hWnd, &crect);

		w += (rect.right - rect.left) - (crect.right - crect.left);
		h += (rect.bottom - rect.top) - (crect.bottom - crect.top);
	}

	MoveWindow(hWnd, rect.left, rect.top, w, h, TRUE);
}
void OS_Windows::set_window_fullscreen(bool p_enabled) {

	if (video_mode.fullscreen == p_enabled)
		return;

	if (p_enabled) {

		if (pre_fs_valid) {
			GetWindowRect(hWnd, &pre_fs_rect);
			//print_line("A: "+itos(pre_fs_rect.left)+","+itos(pre_fs_rect.top)+","+itos(pre_fs_rect.right-pre_fs_rect.left)+","+itos(pre_fs_rect.bottom-pre_fs_rect.top));
			//MapWindowPoints(hWnd, GetParent(hWnd), (LPPOINT) &pre_fs_rect, 2);
			//print_line("B: "+itos(pre_fs_rect.left)+","+itos(pre_fs_rect.top)+","+itos(pre_fs_rect.right-pre_fs_rect.left)+","+itos(pre_fs_rect.bottom-pre_fs_rect.top));
		}

		int cs = get_current_screen();
		Point2 pos = get_screen_position(cs);
		Size2 size = get_screen_size(cs);

		video_mode.fullscreen = true;

		_update_window_style(false);

		MoveWindow(hWnd, pos.x, pos.y, size.width, size.height, TRUE);

	} else {

		RECT rect;

		video_mode.fullscreen = false;

		if (pre_fs_valid) {
			rect = pre_fs_rect;
		} else {
			rect.left = 0;
			rect.right = video_mode.width;
			rect.top = 0;
			rect.bottom = video_mode.height;
		}

		_update_window_style(false);

		MoveWindow(hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);

		pre_fs_valid = true;
	}
}
bool OS_Windows::is_window_fullscreen() const {

	return video_mode.fullscreen;
}
void OS_Windows::set_window_resizable(bool p_enabled) {

	if (video_mode.resizable == p_enabled)
		return;

	video_mode.resizable = p_enabled;

	_update_window_style();
}
bool OS_Windows::is_window_resizable() const {

	return video_mode.resizable;
}
void OS_Windows::set_window_minimized(bool p_enabled) {

	if (p_enabled) {
		maximized = false;
		minimized = true;
		ShowWindow(hWnd, SW_MINIMIZE);
	} else {
		ShowWindow(hWnd, SW_RESTORE);
		maximized = false;
		minimized = false;
	}
}
bool OS_Windows::is_window_minimized() const {

	return minimized;
}
void OS_Windows::set_window_maximized(bool p_enabled) {

	if (p_enabled) {
		maximized = true;
		minimized = false;
		ShowWindow(hWnd, SW_MAXIMIZE);
	} else {
		ShowWindow(hWnd, SW_RESTORE);
		maximized = false;
		minimized = false;
	}
}
bool OS_Windows::is_window_maximized() const {

	return maximized;
}

void OS_Windows::set_window_always_on_top(bool p_enabled) {
	if (video_mode.always_on_top == p_enabled)
		return;

	video_mode.always_on_top = p_enabled;

	_update_window_style();
}

bool OS_Windows::is_window_always_on_top() const {
	return video_mode.always_on_top;
}

void OS_Windows::set_borderless_window(int p_borderless) {
	if (video_mode.borderless_window == p_borderless)
		return;

	video_mode.borderless_window = p_borderless;

	_update_window_style();
}

bool OS_Windows::get_borderless_window() {
	return video_mode.borderless_window;
}

void OS_Windows::_update_window_style(bool repaint) {
	if (video_mode.fullscreen || video_mode.borderless_window) {
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);
	} else {
		if (video_mode.resizable) {
			SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		} else {
			SetWindowLongPtr(hWnd, GWL_STYLE, WS_CAPTION | WS_MINIMIZEBOX | WS_POPUPWINDOW | WS_VISIBLE);
		}
	}

	SetWindowPos(hWnd, video_mode.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	if (repaint) {
		RECT rect;
		GetWindowRect(hWnd, &rect);
		MoveWindow(hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
}

void OS_Windows::request_attention() {

	FLASHWINFO info;
	info.cbSize = sizeof(FLASHWINFO);
	info.hwnd = hWnd;
	info.dwFlags = FLASHW_TRAY;
	info.dwTimeout = 0;
	info.uCount = 2;
	FlashWindowEx(&info);
}

void OS_Windows::print_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type) {

	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!hCon || hCon == INVALID_HANDLE_VALUE) {

		const char *err_details;
		if (p_rationale && p_rationale[0])
			err_details = p_rationale;
		else
			err_details = p_code;

		switch (p_type) {
			case ERR_ERROR:
				print("ERROR: %s: %s\n", p_function, err_details);
				print("   At: %s:%i\n", p_file, p_line);
				break;
			case ERR_WARNING:
				print("WARNING: %s: %s\n", p_function, err_details);
				print("     At: %s:%i\n", p_file, p_line);
				break;
			case ERR_SCRIPT:
				print("SCRIPT ERROR: %s: %s\n", p_function, err_details);
				print("          At: %s:%i\n", p_file, p_line);
				break;
		}

	} else {

		CONSOLE_SCREEN_BUFFER_INFO sbi; //original
		GetConsoleScreenBufferInfo(hCon, &sbi);

		WORD current_fg = sbi.wAttributes & (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		WORD current_bg = sbi.wAttributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);

		uint32_t basecol = 0;
		switch (p_type) {
			case ERR_ERROR: basecol = FOREGROUND_RED; break;
			case ERR_WARNING: basecol = FOREGROUND_RED | FOREGROUND_GREEN; break;
			case ERR_SCRIPT: basecol = FOREGROUND_RED | FOREGROUND_BLUE; break;
		}

		basecol |= current_bg;

		if (p_rationale && p_rationale[0]) {

			SetConsoleTextAttribute(hCon, basecol | FOREGROUND_INTENSITY);
			switch (p_type) {
				case ERR_ERROR: print("ERROR: "); break;
				case ERR_WARNING: print("WARNING: "); break;
				case ERR_SCRIPT: print("SCRIPT ERROR: "); break;
			}

			SetConsoleTextAttribute(hCon, current_fg | current_bg | FOREGROUND_INTENSITY);
			print("%s\n", p_rationale);

			SetConsoleTextAttribute(hCon, basecol);
			switch (p_type) {
				case ERR_ERROR: print("   At: "); break;
				case ERR_WARNING: print("     At: "); break;
				case ERR_SCRIPT: print("          At: "); break;
			}

			SetConsoleTextAttribute(hCon, current_fg | current_bg);
			print("%s:%i\n", p_file, p_line);

		} else {

			SetConsoleTextAttribute(hCon, basecol | FOREGROUND_INTENSITY);
			switch (p_type) {
				case ERR_ERROR: print("ERROR: %s: ", p_function); break;
				case ERR_WARNING: print("WARNING: %s: ", p_function); break;
				case ERR_SCRIPT: print("SCRIPT ERROR: %s: ", p_function); break;
			}

			SetConsoleTextAttribute(hCon, current_fg | current_bg | FOREGROUND_INTENSITY);
			print("%s\n", p_code);

			SetConsoleTextAttribute(hCon, basecol);
			switch (p_type) {
				case ERR_ERROR: print("   At: "); break;
				case ERR_WARNING: print("     At: "); break;
				case ERR_SCRIPT: print("          At: "); break;
			}

			SetConsoleTextAttribute(hCon, current_fg | current_bg);
			print("%s:%i\n", p_file, p_line);
		}

		SetConsoleTextAttribute(hCon, sbi.wAttributes);
	}
}

String OS_Windows::get_name() {

	return "Windows";
}

OS::Date OS_Windows::get_date(bool utc) const {

	SYSTEMTIME systemtime;
	if (utc)
		GetSystemTime(&systemtime);
	else
		GetLocalTime(&systemtime);

	Date date;
	date.day = systemtime.wDay;
	date.month = Month(systemtime.wMonth);
	date.weekday = Weekday(systemtime.wDayOfWeek);
	date.year = systemtime.wYear;
	date.dst = false;
	return date;
}
OS::Time OS_Windows::get_time(bool utc) const {

	SYSTEMTIME systemtime;
	if (utc)
		GetSystemTime(&systemtime);
	else
		GetLocalTime(&systemtime);

	Time time;
	time.hour = systemtime.wHour;
	time.min = systemtime.wMinute;
	time.sec = systemtime.wSecond;
	return time;
}

OS::TimeZoneInfo OS_Windows::get_time_zone_info() const {
	TIME_ZONE_INFORMATION info;
	bool daylight = false;
	if (GetTimeZoneInformation(&info) == TIME_ZONE_ID_DAYLIGHT)
		daylight = true;

	TimeZoneInfo ret;
	if (daylight) {
		ret.name = info.DaylightName;
	} else {
		ret.name = info.StandardName;
	}

	ret.bias = info.Bias;
	return ret;
}

uint64_t OS_Windows::get_unix_time() const {

	FILETIME ft;
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	SYSTEMTIME ep;
	ep.wYear = 1970;
	ep.wMonth = 1;
	ep.wDayOfWeek = 4;
	ep.wDay = 1;
	ep.wHour = 0;
	ep.wMinute = 0;
	ep.wSecond = 0;
	ep.wMilliseconds = 0;
	FILETIME fep;
	SystemTimeToFileTime(&ep, &fep);

	return (*(uint64_t *)&ft - *(uint64_t *)&fep) / 10000000;
};

uint64_t OS_Windows::get_system_time_secs() const {

	const uint64_t WINDOWS_TICK = 10000000;
	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;

	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	uint64_t ret;
	ret = ft.dwHighDateTime;
	ret <<= 32;
	ret |= ft.dwLowDateTime;

	return (uint64_t)(ret / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

void OS_Windows::delay_usec(uint32_t p_usec) const {

	if (p_usec < 1000)
		Sleep(1);
	else
		Sleep(p_usec / 1000);
}
uint64_t OS_Windows::get_ticks_usec() const {

	uint64_t ticks;
	uint64_t time;
	// This is the number of clock ticks since start
	if (!QueryPerformanceCounter((LARGE_INTEGER *)&ticks))
		ticks = (UINT64)timeGetTime();
	// Divide by frequency to get the time in seconds
	time = ticks * 1000000L / ticks_per_second;
	// Subtract the time at game start to get
	// the time since the game started
	time -= ticks_start;
	return time;
}

void OS_Windows::process_events() {

	MSG msg;

	last_id = joystick->process_joysticks(last_id);

	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	process_key_events();
}

void OS_Windows::set_cursor_shape(CursorShape p_shape) {

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (cursor_shape == p_shape)
		return;

	if (mouse_mode != MOUSE_MODE_VISIBLE) {
		cursor_shape = p_shape;
		return;
	}

	static const LPCTSTR win_cursors[CURSOR_MAX] = {
		IDC_ARROW,
		IDC_IBEAM,
		IDC_HAND, //finger
		IDC_CROSS,
		IDC_WAIT,
		IDC_APPSTARTING,
		IDC_ARROW,
		IDC_ARROW,
		IDC_NO,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_SIZENESW,
		IDC_SIZENWSE,
		IDC_SIZEALL,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_HELP
	};

	if (cursors[p_shape] != NULL) {
		SetCursor(cursors[p_shape]);
	} else {
		SetCursor(LoadCursor(hInstance, win_cursors[p_shape]));
	}

	cursor_shape = p_shape;
}

void OS_Windows::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (p_cursor.is_valid()) {
		Ref<ImageTexture> texture = p_cursor;
		Ref<AtlasTexture> atlas_texture = p_cursor;
		Size2 texture_size;
		Rect2 atlas_rect;

		if (!texture.is_valid() && atlas_texture.is_valid()) {
			texture = atlas_texture->get_atlas();

			atlas_rect.size.width = texture->get_width();
			atlas_rect.size.height = texture->get_height();
			atlas_rect.pos.x = atlas_texture->get_region().pos.x;
			atlas_rect.pos.y = atlas_texture->get_region().pos.y;

			texture_size.width = atlas_texture->get_region().size.x;
			texture_size.height = atlas_texture->get_region().size.y;
		} else if (texture.is_valid()) {
			texture_size.width = texture->get_width();
			texture_size.height = texture->get_height();
		}

		ERR_FAIL_COND(!texture.is_valid());
		ERR_FAIL_COND(texture_size.width > 256 || texture_size.height > 256);

		Image image = texture->get_data();

		UINT image_size = texture_size.width * texture_size.height;
		UINT size = sizeof(UINT) * image_size;

		// Create the BITMAP with alpha channel
		COLORREF *buffer = (COLORREF *)memalloc(sizeof(COLORREF) * image_size);

		for (UINT index = 0; index < image_size; index++) {
			int row_index = floor(index / texture_size.width) + atlas_rect.pos.y;
			int column_index = (index % int(texture_size.width)) + atlas_rect.pos.x;

			if (atlas_texture.is_valid()) {
				column_index = MIN(column_index, atlas_rect.size.width - 1);
				row_index = MIN(row_index, atlas_rect.size.height - 1);
			}

			*(buffer + index) = image.get_pixel(column_index, row_index).to_ARGB32();
		}

		// Using 4 channels, so 4 * 8 bits
		HBITMAP bitmap = CreateBitmap(texture_size.width, texture_size.height, 1, 4 * 8, buffer);
		COLORREF clrTransparent = -1;

		// Create the AND and XOR masks for the bitmap
		HBITMAP hAndMask = NULL;
		HBITMAP hXorMask = NULL;

		GetMaskBitmaps(bitmap, clrTransparent, hAndMask, hXorMask);

		if (NULL == hAndMask || NULL == hXorMask) {
			memfree(buffer);
			DeleteObject(bitmap);
			return;
		}

		// Finally, create the icon
		ICONINFO iconinfo = { 0 };
		iconinfo.fIcon = FALSE;
		iconinfo.xHotspot = p_hotspot.x;
		iconinfo.yHotspot = p_hotspot.y;
		iconinfo.hbmMask = hAndMask;
		iconinfo.hbmColor = hXorMask;

		cursors[p_shape] = CreateIconIndirect(&iconinfo);

		if (p_shape == CURSOR_ARROW) {
			if (mouse_mode == MOUSE_MODE_VISIBLE) {
				SetCursor(cursors[p_shape]);
			}
		}

		if (hAndMask != NULL) {
			DeleteObject(hAndMask);
		}

		if (hXorMask != NULL) {
			DeleteObject(hXorMask);
		}

		memfree(buffer);
		DeleteObject(bitmap);
	}
}

void OS_Windows::GetMaskBitmaps(HBITMAP hSourceBitmap, COLORREF clrTransparent, OUT HBITMAP &hAndMaskBitmap, OUT HBITMAP &hXorMaskBitmap) {

	// Get the system display DC
	HDC hDC = GetDC(NULL);

	// Create helper DC
	HDC hMainDC = CreateCompatibleDC(hDC);
	HDC hAndMaskDC = CreateCompatibleDC(hDC);
	HDC hXorMaskDC = CreateCompatibleDC(hDC);

	// Get the dimensions of the source bitmap
	BITMAP bm;
	GetObject(hSourceBitmap, sizeof(BITMAP), &bm);

	// Create the mask bitmaps
	hAndMaskBitmap = CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight); // color
	hXorMaskBitmap = CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight); // color

	// Release the system display DC
	ReleaseDC(NULL, hDC);

	// Select the bitmaps to helper DC
	HBITMAP hOldMainBitmap = (HBITMAP)SelectObject(hMainDC, hSourceBitmap);
	HBITMAP hOldAndMaskBitmap = (HBITMAP)SelectObject(hAndMaskDC, hAndMaskBitmap);
	HBITMAP hOldXorMaskBitmap = (HBITMAP)SelectObject(hXorMaskDC, hXorMaskBitmap);

	// Assign the monochrome AND mask bitmap pixels so that a pixels of the source bitmap
	// with 'clrTransparent' will be white pixels of the monochrome bitmap
	SetBkColor(hMainDC, clrTransparent);
	BitBlt(hAndMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hMainDC, 0, 0, SRCCOPY);

	// Assign the color XOR mask bitmap pixels so that a pixels of the source bitmap
	// with 'clrTransparent' will be black and rest the pixels same as corresponding
	// pixels of the source bitmap
	SetBkColor(hXorMaskDC, RGB(0, 0, 0));
	SetTextColor(hXorMaskDC, RGB(255, 255, 255));
	BitBlt(hXorMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hAndMaskDC, 0, 0, SRCCOPY);
	BitBlt(hXorMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hMainDC, 0, 0, SRCAND);

	// Deselect bitmaps from the helper DC
	SelectObject(hMainDC, hOldMainBitmap);
	SelectObject(hAndMaskDC, hOldAndMaskBitmap);
	SelectObject(hXorMaskDC, hOldXorMaskBitmap);

	// Delete the helper DC
	DeleteDC(hXorMaskDC);
	DeleteDC(hAndMaskDC);
	DeleteDC(hMainDC);
}

Error OS_Windows::execute(const String &p_path, const List<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool read_stderr) {

	if (p_blocking && r_pipe) {

		String argss;
		argss = "\"\"" + p_path + "\"";

		for (const List<String>::Element *E = p_arguments.front(); E; E = E->next()) {

			argss += String(" \"") + E->get() + "\"";
		}

		//		print_line("ARGS: "+argss);
		//argss+"\"";
		//argss+=" 2>nul";

		FILE *f = _wpopen(argss.c_str(), L"r");

		ERR_FAIL_COND_V(!f, ERR_CANT_OPEN);

		char buf[65535];
		while (fgets(buf, 65535, f)) {

			(*r_pipe) += buf;
		}

		int rv = _pclose(f);
		if (r_exitcode)
			*r_exitcode = rv;

		return OK;
	}

	String cmdline = "\"" + p_path + "\"";
	const List<String>::Element *I = p_arguments.front();
	while (I) {

		cmdline += " \"" + I->get() + "\"";

		I = I->next();
	};

	//cmdline+="\"";

	ProcessInfo pi;
	ZeroMemory(&pi.si, sizeof(pi.si));
	pi.si.cb = sizeof(pi.si);
	ZeroMemory(&pi.pi, sizeof(pi.pi));
	LPSTARTUPINFOW si_w = (LPSTARTUPINFOW)&pi.si;

	print_line("running cmdline: " + cmdline);
	Vector<CharType> modstr; //windows wants to change this no idea why
	modstr.resize(cmdline.size());
	for (int i = 0; i < cmdline.size(); i++)
		modstr[i] = cmdline[i];
	int ret = CreateProcessW(NULL, modstr.ptr(), NULL, NULL, 0, NORMAL_PRIORITY_CLASS, NULL, NULL, si_w, &pi.pi);
	ERR_FAIL_COND_V(ret == 0, ERR_CANT_FORK);

	if (p_blocking) {

		DWORD ret = WaitForSingleObject(pi.pi.hProcess, INFINITE);
		if (r_exitcode)
			*r_exitcode = ret;

	} else {

		ProcessID pid = pi.pi.dwProcessId;
		if (r_child_id) {
			*r_child_id = pid;
		};
		process_map->insert(pid, pi);
	};
	return OK;
};

Error OS_Windows::kill(const ProcessID &p_pid) {

	HANDLE h;

	if (process_map->has(p_pid)) {
		h = (*process_map)[p_pid].pi.hProcess;
		process_map->erase(p_pid);
	} else {

		ERR_FAIL_COND_V(!process_map->has(p_pid), FAILED);
	};

	int ret = TerminateProcess(h, 0);

	return ret != 0 ? OK : FAILED;
};

int OS_Windows::get_process_ID() const {
	return _getpid();
}

Error OS_Windows::set_cwd(const String &p_cwd) {

	if (_wchdir(p_cwd.c_str()) != 0)
		return ERR_CANT_OPEN;

	return OK;
}

String OS_Windows::get_executable_path() const {

	wchar_t bufname[4096];
	GetModuleFileNameW(NULL, bufname, 4096);
	String s = bufname;
	return s;
}

void OS_Windows::set_icon(const Image &p_icon) {

	Image icon = p_icon;
	if (icon.get_format() != Image::FORMAT_RGBA)
		icon.convert(Image::FORMAT_RGBA);
	int w = icon.get_width();
	int h = icon.get_height();

	/* Create temporary bitmap buffer */
	int icon_len = 40 + h * w * 4;
	Vector<BYTE> v;
	v.resize(icon_len);
	BYTE *icon_bmp = &v[0];

	encode_uint32(40, &icon_bmp[0]);
	encode_uint32(w, &icon_bmp[4]);
	encode_uint32(h * 2, &icon_bmp[8]);
	encode_uint16(1, &icon_bmp[12]);
	encode_uint16(32, &icon_bmp[14]);
	encode_uint32(BI_RGB, &icon_bmp[16]);
	encode_uint32(w * h * 4, &icon_bmp[20]);
	encode_uint32(0, &icon_bmp[24]);
	encode_uint32(0, &icon_bmp[28]);
	encode_uint32(0, &icon_bmp[32]);
	encode_uint32(0, &icon_bmp[36]);

	uint8_t *wr = &icon_bmp[40];
	DVector<uint8_t>::Read r = icon.get_data().read();

	for (int i = 0; i < h; i++) {

		for (int j = 0; j < w; j++) {

			const uint8_t *rpx = &r[((h - i - 1) * w + j) * 4];
			uint8_t *wpx = &wr[(i * w + j) * 4];
			wpx[0] = rpx[2];
			wpx[1] = rpx[1];
			wpx[2] = rpx[0];
			wpx[3] = rpx[3];
		}
	}

	HICON hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);

	/* Set the icon for the window */
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

	/* Set the icon in the task manager (should we do this?) */
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
}

bool OS_Windows::has_environment(const String &p_var) const {

	return getenv(p_var.utf8().get_data()) != NULL;
};

String OS_Windows::get_environment(const String &p_var) const {

	wchar_t wval[0x7Fff]; // MSDN says 32767 char is the maximum
	int wlen = GetEnvironmentVariableW(p_var.c_str(), wval, 0x7Fff);
	if (wlen > 0) {
		return wval;
	}
	return "";
}

String OS_Windows::get_stdin_string(bool p_block) {

	if (p_block) {
		char buff[1024];
		return fgets(buff, 1024, stdin);
	};

	return String();
}

void OS_Windows::enable_for_stealing_focus(ProcessID pid) {

	AllowSetForegroundWindow(pid);
}

void OS_Windows::move_window_to_foreground() {

	SetForegroundWindow(hWnd);
}

Error OS_Windows::shell_open(String p_uri) {

	ShellExecuteW(NULL, L"open", p_uri.c_str(), NULL, NULL, SW_SHOWNORMAL);
	return OK;
}

String OS_Windows::get_locale() const {

	const _WinLocale *wl = &_win_locales[0];

	LANGID langid = GetUserDefaultUILanguage();
	String neutral;
	int lang = langid & ((1 << 9) - 1);
	int sublang = langid & ~((1 << 9) - 1);

	while (wl->locale) {

		if (wl->main_lang == lang && wl->sublang == SUBLANG_NEUTRAL)
			neutral = wl->locale;

		if (lang == wl->main_lang && sublang == wl->sublang)
			return wl->locale;

		wl++;
	}

	if (neutral != "")
		return neutral;

	return "en";
}

OS::LatinKeyboardVariant OS_Windows::get_latin_keyboard_variant() const {

	unsigned long azerty[] = {
		0x00020401, // Arabic (102) AZERTY
		0x0001080c, // Belgian (Comma)
		0x0000080c, // Belgian French
		0x0000040c, // French
		0 // <--- STOP MARK
	};
	unsigned long qwertz[] = {
		0x0000041a, // Croation
		0x00000405, // Czech
		0x00000407, // German
		0x00010407, // German (IBM)
		0x0000040e, // Hungarian
		0x0000046e, // Luxembourgish
		0x00010415, // Polish (214)
		0x00000418, // Romanian (Legacy)
		0x0000081a, // Serbian (Latin)
		0x0000041b, // Slovak
		0x00000424, // Slovenian
		0x0001042e, // Sorbian Extended
		0x0002042e, // Sorbian Standard
		0x0000042e, // Sorbian Standard (Legacy)
		0x0000100c, // Swiss French
		0x00000807, // Swiss German
		0 // <--- STOP MARK
	};
	unsigned long dvorak[] = {
		0x00010409, // US-Dvorak
		0x00030409, // US-Dvorak for left hand
		0x00040409, // US-Dvorak for right hand
		0 // <--- STOP MARK
	};

	char name[KL_NAMELENGTH + 1];
	name[0] = 0;
	GetKeyboardLayoutNameA(name);

	unsigned long hex = strtoul(name, NULL, 16);

	int i = 0;
	while (azerty[i] != 0) {
		if (azerty[i] == hex) return LATIN_KEYBOARD_AZERTY;
		i++;
	}

	i = 0;
	while (qwertz[i] != 0) {
		if (qwertz[i] == hex) return LATIN_KEYBOARD_QWERTZ;
		i++;
	}

	i = 0;
	while (dvorak[i] != 0) {
		if (dvorak[i] == hex) return LATIN_KEYBOARD_DVORAK;
		i++;
	}

	return LATIN_KEYBOARD_QWERTY;
}

void OS_Windows::release_rendering_thread() {

	gl_context->release_current();
}

void OS_Windows::make_rendering_thread() {

	gl_context->make_current();
}

void OS_Windows::swap_buffers() {

	gl_context->swap_buffers();
}

void OS_Windows::run() {

	if (!main_loop)
		return;

	main_loop->init();

	uint64_t last_ticks = get_ticks_usec();

	int frames = 0;
	uint64_t frame = 0;

	while (!force_quit) {

		process_events(); // get rid of pending events
		if (Main::iteration() == true)
			break;
	};

	main_loop->finish();
}

MainLoop *OS_Windows::get_main_loop() const {

	return main_loop;
}

String OS_Windows::get_system_dir(SystemDir p_dir) const {

	int id;

	switch (p_dir) {
		case SYSTEM_DIR_DESKTOP: {
			id = CSIDL_DESKTOPDIRECTORY;
		} break;
		case SYSTEM_DIR_DCIM: {
			id = CSIDL_MYPICTURES;
		} break;
		case SYSTEM_DIR_DOCUMENTS: {
			id = CSIDL_PERSONAL;
		} break;
		case SYSTEM_DIR_DOWNLOADS: {
			id = 0x000C;
		} break;
		case SYSTEM_DIR_MOVIES: {
			id = CSIDL_MYVIDEO;
		} break;
		case SYSTEM_DIR_MUSIC: {
			id = CSIDL_MYMUSIC;
		} break;
		case SYSTEM_DIR_PICTURES: {
			id = CSIDL_MYPICTURES;
		} break;
		case SYSTEM_DIR_RINGTONES: {
			id = CSIDL_MYMUSIC;
		} break;
	}

	WCHAR szPath[MAX_PATH];
	HRESULT res = SHGetFolderPathW(NULL, id, NULL, 0, szPath);
	ERR_FAIL_COND_V(res != S_OK, String());
	return String(szPath);
}
String OS_Windows::get_data_dir() const {

	String an = get_safe_application_name();
	if (an != "") {

		if (has_environment("APPDATA")) {

			bool use_godot = Globals::get_singleton()->get("application/use_shared_user_dir");
			if (!use_godot)
				return (OS::get_singleton()->get_environment("APPDATA") + "/" + an).replace("\\", "/");
			else
				return (OS::get_singleton()->get_environment("APPDATA") + "/Godot/app_userdata/" + an).replace("\\", "/");
		}
	}

	return Globals::get_singleton()->get_resource_path();
}

bool OS_Windows::is_joy_known(int p_device) {
	return input->is_joy_mapped(p_device);
}

String OS_Windows::get_joy_guid(int p_device) const {
	return input->get_joy_guid_remapped(p_device);
}

void OS_Windows::set_use_vsync(bool p_enable) {

	if (gl_context)
		gl_context->set_use_vsync(p_enable);
}

bool OS_Windows::is_vsync_enabled() const {

	if (gl_context)
		return gl_context->is_using_vsync();

	return true;
}

void OS_Windows::disable_crash_handler() {
	crash_handler.disable();
}

bool OS_Windows::is_disable_crash_handler() const {
	return crash_handler.is_disabled();
}

Error OS_Windows::move_path_to_trash(String p_dir) {
	SHFILEOPSTRUCTA sf;
	TCHAR *from = new TCHAR[p_dir.length() + 2];
	strcpy(from, p_dir.utf8().get_data());
	from[p_dir.length()] = 0;
	from[p_dir.length() + 1] = 0;

	sf.hwnd = hWnd;
	sf.wFunc = FO_DELETE;
	sf.pFrom = from;
	sf.pTo = NULL;
	sf.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
	sf.fAnyOperationsAborted = FALSE;
	sf.hNameMappings = NULL;
	sf.lpszProgressTitle = NULL;

	int ret = SHFileOperation(&sf);
	delete[] from;

	if (ret) {
		ERR_PRINTS("SHFileOperation error: " + itos(ret));
		return FAILED;
	}

	return OK;
}

OS_Windows::OS_Windows(HINSTANCE _hInstance) {

	key_event_pos = 0;
	force_quit = false;
	alt_mem = false;
	gr_mem = false;
	shift_mem = false;
	control_mem = false;
	meta_mem = false;
	minimized = false;

	hInstance = _hInstance;
	pressrc = 0;
	old_invalid = true;
	last_id = 0;
	mouse_mode = MOUSE_MODE_VISIBLE;
#ifdef STDOUT_FILE
	stdo = fopen("stdout.txt", "wb");
#endif
	user_proc = NULL;

#ifdef WASAPI_ENABLED
	AudioDriverManagerSW::add_driver(&driver_wasapi);
#endif
#ifdef RTAUDIO_ENABLED
	AudioDriverManagerSW::add_driver(&driver_rtaudio);
#endif
}

OS_Windows::~OS_Windows() {
#ifdef STDOUT_FILE
	fclose(stdo);
#endif
}
