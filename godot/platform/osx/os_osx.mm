/*************************************************************************/
/*  os_osx.mm                                                            */
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
#include "os_osx.h"

#include "dir_access_osx.h"
#include "drivers/gles2/rasterizer_instance_gles2.h"
#include "main/main.h"
#include "os/keyboard.h"
#include "print_string.h"
#include "scene/resources/texture.h"
#include "sem_osx.h"
#include "servers/physics/physics_server_sw.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
#include <os/log.h>
#endif

#include <fcntl.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//uses portions of glfw

//========================================================================
// GLFW 3.0 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSWindowStyleMaskBorderless NSBorderlessWindowMask
#endif

static NSRect convertRectToBacking(NSRect contentRect) {

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6)
		return [OS_OSX::singleton->window_view convertRectToBacking:contentRect];
	else
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/
		return contentRect;
}

static InputModifierState translateFlags(NSUInteger flags) {
	InputModifierState mod;

	mod.shift = (flags & NSShiftKeyMask);
	mod.control = (flags & NSControlKeyMask);
	mod.alt = (flags & NSAlternateKeyMask);
	mod.meta = (flags & NSCommandKeyMask);

	return mod;
}

static int mouse_x = 0;
static int mouse_y = 0;
static int prev_mouse_x = 0;
static int prev_mouse_y = 0;
static int button_mask = 0;
static bool mouse_down_control = false;

@interface GodotApplication : NSApplication
@end

@implementation GodotApplication

// From http://cocoadev.com/index.pl?GameKeyboardHandlingAlmost
// This works around an AppKit bug, where key up events while holding
// down the command key don't get sent to the key window.
- (void)sendEvent:(NSEvent *)event {
	if ([event type] == NSKeyUp && ([event modifierFlags] & NSCommandKeyMask))
		[[self keyWindow] sendEvent:event];
	else
		[super sendEvent:event];
}

@end

@interface GodotApplicationDelegate : NSObject
@end

@implementation GodotApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);

	return NSTerminateCancel;
}

- (void)applicationDidHide:(NSNotification *)notification {
	/*
	_Godotwindow* window;

	for (window = _Godot.windowListHead;  window;  window = window->next)
		_GodotInputWindowVisibility(window, GL_FALSE);
*/
}

- (void)applicationDidUnhide:(NSNotification *)notification {
	/*
	_Godotwindow* window;

	for (window = _Godot.windowListHead;  window;  window = window->next)
	{
		if ([window_object isVisible])
			_GodotInputWindowVisibility(window, GL_TRUE);
	}
*/
}

- (void)applicationDidChangeScreenParameters:(NSNotification *)notification {
	//_GodotInputMonitorChange();
}

@end

@interface GodotWindowDelegate : NSObject {
	//_Godotwindow* window;
}

@end

@implementation GodotWindowDelegate

- (BOOL)windowShouldClose:(id)sender {
	//_GodotInputWindowCloseRequest(window);
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
	return NO;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
- (void)windowDidEnterFullScreen:(NSNotification *)notification {
	OS_OSX::singleton->zoomed = true;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
	OS_OSX::singleton->zoomed = false;
}
#endif // MAC_OS_X_VERSION_MAX_ALLOWED

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
	if (!OS_OSX::singleton)
		return;

	NSWindow *window = (NSWindow *)[notification object];
	CGFloat newBackingScaleFactor = [window backingScaleFactor];
	CGFloat oldBackingScaleFactor = [[[notification userInfo] objectForKey:@"NSBackingPropertyOldScaleFactorKey"] doubleValue];

	if (newBackingScaleFactor != oldBackingScaleFactor) {
		//Set new display scale and window size
		OS_OSX::singleton->display_scale = newBackingScaleFactor;

		const NSRect contentRect = [OS_OSX::singleton->window_view frame];
		const NSRect fbRect = contentRect; //convertRectToBacking(contentRect);

		OS_OSX::singleton->window_size.width = fbRect.size.width * OS_OSX::singleton->display_scale;
		OS_OSX::singleton->window_size.height = fbRect.size.height * OS_OSX::singleton->display_scale;

		//Update context
		if (OS_OSX::singleton->main_loop) {
			[OS_OSX::singleton->context update];

			//Force window resize ???
			NSRect frame = [OS_OSX::singleton->window_object frame];
			[OS_OSX::singleton->window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y, 1, 1) display:YES];
			[OS_OSX::singleton->window_object setFrame:frame display:YES];
		}
	}
}

- (void)windowDidResize:(NSNotification *)notification {
	[OS_OSX::singleton->context update];

	const NSRect contentRect = [OS_OSX::singleton->window_view frame];
	const NSRect fbRect = contentRect; //convertRectToBacking(contentRect);

	OS_OSX::singleton->window_size.width = fbRect.size.width * OS_OSX::singleton->display_scale;
	OS_OSX::singleton->window_size.height = fbRect.size.height * OS_OSX::singleton->display_scale;

	if (OS_OSX::singleton->main_loop) {
		Main::force_redraw();
		//Event retrieval blocks until resize is over. Call Main::iteration() directly.
		Main::iteration();
	}

	//_GodotInputFramebufferSize(window, fbRect.size.width, fbRect.size.height);
	//_GodotInputWindowSize(window, contentRect.size.width, contentRect.size.height);
	//_GodotInputWindowDamage(window);

	//if (window->cursorMode == Godot_CURSOR_DISABLED)
	//	centerCursor(window);
}

- (void)windowDidMove:(NSNotification *)notification {
	//[window->nsgl.context update];

	//int x, y;
	//_GodotPlatformGetWindowPos(window, &x, &y);
	//_GodotInputWindowPos(window, x, y);

	//if (window->cursorMode == Godot_CURSOR_DISABLED)
	//	centerCursor(window);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	//_GodotInputWindowFocus(window, GL_TRUE);
	//_GodotPlatformSetCursorMode(window, window->cursorMode);
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
}

- (void)windowDidResignKey:(NSNotification *)notification {
	//_GodotInputWindowFocus(window, GL_FALSE);
	//_GodotPlatformSetCursorMode(window, Godot_CURSOR_NORMAL);
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
}

- (void)windowDidMiniaturize:(NSNotification *)notification {
	OS_OSX::singleton->wm_minimized(true);
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
};

- (void)windowDidDeminiaturize:(NSNotification *)notification {
	OS_OSX::singleton->wm_minimized(false);
	if (OS_OSX::singleton->get_main_loop())
		OS_OSX::singleton->get_main_loop()->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
};

@end

@interface GodotContentView : NSView {
	NSTrackingArea *trackingArea;
}

@end

@implementation GodotContentView

+ (void)initialize {
	if (self == [GodotContentView class]) {
		/*
		if (_glfw.ns.cursor == nil) {
			NSImage* data = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
			_glfw.ns.cursor = [[NSCursor alloc] initWithImage:data hotSpot:NSZeroPoint];
			[data release];
		}
*/
	}
}

- (id)init {
	self = [super init];
	trackingArea = nil;
	[self updateTrackingAreas];
	[self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
	return self;
}

- (void)dealloc {
	[trackingArea release];
	[super dealloc];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
	return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
	return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
	NSPasteboard *pboard = [sender draggingPasteboard];
	NSArray *filenames = [pboard propertyListForType:NSFilenamesPboardType];

	Vector<String> files;
	for (int i = 0; i < filenames.count; i++) {
		NSString *ns = [filenames objectAtIndex:i];
		char *utfs = strdup([ns UTF8String]);
		String ret;
		ret.parse_utf8(utfs);
		free(utfs);
		files.push_back(ret);
	}

	if (files.size()) {
		OS_OSX::singleton->main_loop->drop_files(files, 0);
		OS_OSX::singleton->move_window_to_foreground();
	}

	return NO;
}

- (BOOL)isOpaque {
	return YES;
}

- (BOOL)canBecomeKeyView {
	return YES;
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)cursorUpdate:(NSEvent *)event {
	//setModeCursor(window, window->cursorMode);
}

static void _mouseDownEvent(NSEvent *event, int index, int mask, bool pressed) {
	if (pressed) {
		button_mask |= mask;
	} else {
		button_mask &= ~mask;
	}

	InputEvent ev;

	ev.type = InputEvent::MOUSE_BUTTON;
	ev.mouse_button.button_index = index;
	ev.mouse_button.pressed = pressed;
	ev.mouse_button.x = mouse_x;
	ev.mouse_button.y = mouse_y;
	ev.mouse_button.global_x = mouse_x;
	ev.mouse_button.global_y = mouse_y;
	ev.mouse_button.button_mask = button_mask;
	if (index == BUTTON_LEFT && pressed) {
		ev.mouse_button.doubleclick = [event clickCount] == 2;
	}
	ev.mouse_button.mod = translateFlags([event modifierFlags]);

	OS_OSX::singleton->push_input(ev);
}

- (void)mouseDown:(NSEvent *)event {
	if (([event modifierFlags] & NSControlKeyMask)) {
		mouse_down_control = true;
		_mouseDownEvent(event, BUTTON_RIGHT, BUTTON_MASK_RIGHT, true);
	} else {
		mouse_down_control = false;
		_mouseDownEvent(event, BUTTON_LEFT, BUTTON_MASK_LEFT, true);
	}
}

- (void)mouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event {
	if (mouse_down_control) {
		_mouseDownEvent(event, BUTTON_RIGHT, BUTTON_MASK_RIGHT, false);
	} else {
		_mouseDownEvent(event, BUTTON_LEFT, BUTTON_MASK_LEFT, false);
	}
}

- (void)mouseMoved:(NSEvent *)event {

	InputEvent ev;
	ev.type = InputEvent::MOUSE_MOTION;
	ev.mouse_motion.button_mask = button_mask;
	prev_mouse_x = mouse_x;
	prev_mouse_y = mouse_y;
	const NSRect contentRect = [OS_OSX::singleton->window_view frame];
	const NSPoint p = [event locationInWindow];
	mouse_x = p.x * OS_OSX::singleton->_mouse_scale([[event window] backingScaleFactor]);
	mouse_y = (contentRect.size.height - p.y) * OS_OSX::singleton->_mouse_scale([[event window] backingScaleFactor]);
	ev.mouse_motion.x = mouse_x;
	ev.mouse_motion.y = mouse_y;
	ev.mouse_motion.global_x = mouse_x;
	ev.mouse_motion.global_y = mouse_y;
	ev.mouse_motion.relative_x = [event deltaX] * OS_OSX::singleton->_mouse_scale([[event window] backingScaleFactor]);
	ev.mouse_motion.relative_y = [event deltaY] * OS_OSX::singleton->_mouse_scale([[event window] backingScaleFactor]);
	ev.mouse_motion.mod = translateFlags([event modifierFlags]);

	OS_OSX::singleton->input->set_mouse_pos(Point2(mouse_x, mouse_y));
	OS_OSX::singleton->push_input(ev);

	/*
	if (window->cursorMode == GLFW_CURSOR_DISABLED)
		_glfwInputCursorMotion(window, [event deltaX], [event deltaY]);
	else {
		const NSRect contentRect = [window->ns.view frame];
		const NSPoint p = [event locationInWindow];

		_glfwInputCursorMotion(window, p.x, contentRect.size.height - p.y);
	}
*/
}

- (void)rightMouseDown:(NSEvent *)event {
	_mouseDownEvent(event, BUTTON_RIGHT, BUTTON_MASK_RIGHT, true);
}

- (void)rightMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event {
	_mouseDownEvent(event, BUTTON_RIGHT, BUTTON_MASK_RIGHT, false);
}

- (void)otherMouseDown:(NSEvent *)event {

	if ((int)[event buttonNumber] != 2)
		return;

	_mouseDownEvent(event, BUTTON_MIDDLE, BUTTON_MASK_MIDDLE, true);
}

- (void)otherMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event {

	if ((int)[event buttonNumber] != 2)
		return;

	_mouseDownEvent(event, BUTTON_MIDDLE, BUTTON_MASK_MIDDLE, false);
}

- (void)mouseExited:(NSEvent *)event {
	if (!OS_OSX::singleton)
		return;

	if (OS_OSX::singleton->main_loop && OS_OSX::singleton->mouse_mode != OS::MOUSE_MODE_CAPTURED)
		OS_OSX::singleton->main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_EXIT);

	//_glfwInputCursorEnter(window, GL_FALSE);
}

- (void)mouseEntered:(NSEvent *)event {
	//_glfwInputCursorEnter(window, GL_TRUE);
	if (!OS_OSX::singleton)
		return;

	if (OS_OSX::singleton->main_loop && OS_OSX::singleton->mouse_mode != OS::MOUSE_MODE_CAPTURED)
		OS_OSX::singleton->main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);

	if (OS_OSX::singleton->input) {
		OS_OSX::singleton->cursor_shape = OS::CURSOR_MAX;
		OS_OSX::singleton->set_cursor_shape(OS::CURSOR_ARROW);
	}
}

- (void)viewDidChangeBackingProperties {
	/*
	const NSRect contentRect = [window->ns.view frame];
	const NSRect fbRect = convertRectToBacking(window, contentRect);

	_glfwInputFramebufferSize(window, fbRect.size.width, fbRect.size.height);
*/
}

- (void)updateTrackingAreas {
	if (trackingArea != nil) {
		[self removeTrackingArea:trackingArea];
		[trackingArea release];
	}

	NSTrackingAreaOptions options =
			NSTrackingMouseEnteredAndExited |
			NSTrackingActiveInKeyWindow |
			NSTrackingCursorUpdate |
			NSTrackingInVisibleRect;

	trackingArea = [[NSTrackingArea alloc]
			initWithRect:[self bounds]
				 options:options
				   owner:self
				userInfo:nil];

	[self addTrackingArea:trackingArea];
	[super updateTrackingAreas];
}

// Translates a OS X keycode to a Godot keycode
//
static int translateKey(unsigned int key) {
	// Keyboard symbol translation table
	static const unsigned int table[128] = {
		/* 00 */ KEY_A,
		/* 01 */ KEY_S,
		/* 02 */ KEY_D,
		/* 03 */ KEY_F,
		/* 04 */ KEY_H,
		/* 05 */ KEY_G,
		/* 06 */ KEY_Z,
		/* 07 */ KEY_X,
		/* 08 */ KEY_C,
		/* 09 */ KEY_V,
		/* 0a */ KEY_UNKNOWN,
		/* 0b */ KEY_B,
		/* 0c */ KEY_Q,
		/* 0d */ KEY_W,
		/* 0e */ KEY_E,
		/* 0f */ KEY_R,
		/* 10 */ KEY_Y,
		/* 11 */ KEY_T,
		/* 12 */ KEY_1,
		/* 13 */ KEY_2,
		/* 14 */ KEY_3,
		/* 15 */ KEY_4,
		/* 16 */ KEY_6,
		/* 17 */ KEY_5,
		/* 18 */ KEY_EQUAL,
		/* 19 */ KEY_9,
		/* 1a */ KEY_7,
		/* 1b */ KEY_MINUS,
		/* 1c */ KEY_8,
		/* 1d */ KEY_0,
		/* 1e */ KEY_BRACERIGHT,
		/* 1f */ KEY_O,
		/* 20 */ KEY_U,
		/* 21 */ KEY_BRACELEFT,
		/* 22 */ KEY_I,
		/* 23 */ KEY_P,
		/* 24 */ KEY_RETURN,
		/* 25 */ KEY_L,
		/* 26 */ KEY_J,
		/* 27 */ KEY_APOSTROPHE,
		/* 28 */ KEY_K,
		/* 29 */ KEY_SEMICOLON,
		/* 2a */ KEY_BACKSLASH,
		/* 2b */ KEY_COMMA,
		/* 2c */ KEY_SLASH,
		/* 2d */ KEY_N,
		/* 2e */ KEY_M,
		/* 2f */ KEY_PERIOD,
		/* 30 */ KEY_TAB,
		/* 31 */ KEY_SPACE,
		/* 32 */ KEY_QUOTELEFT,
		/* 33 */ KEY_BACKSPACE,
		/* 34 */ KEY_UNKNOWN,
		/* 35 */ KEY_ESCAPE,
		/* 36 */ KEY_META,
		/* 37 */ KEY_META,
		/* 38 */ KEY_SHIFT,
		/* 39 */ KEY_CAPSLOCK,
		/* 3a */ KEY_ALT,
		/* 3b */ KEY_CONTROL,
		/* 3c */ KEY_SHIFT,
		/* 3d */ KEY_ALT,
		/* 3e */ KEY_CONTROL,
		/* 3f */ KEY_UNKNOWN, /* Function */
		/* 40 */ KEY_UNKNOWN,
		/* 41 */ KEY_KP_PERIOD,
		/* 42 */ KEY_UNKNOWN,
		/* 43 */ KEY_KP_MULTIPLY,
		/* 44 */ KEY_UNKNOWN,
		/* 45 */ KEY_KP_ADD,
		/* 46 */ KEY_UNKNOWN,
		/* 47 */ KEY_NUMLOCK, /* Really KeypadClear... */
		/* 48 */ KEY_UNKNOWN, /* VolumeUp */
		/* 49 */ KEY_UNKNOWN, /* VolumeDown */
		/* 4a */ KEY_UNKNOWN, /* Mute */
		/* 4b */ KEY_KP_DIVIDE,
		/* 4c */ KEY_ENTER,
		/* 4d */ KEY_UNKNOWN,
		/* 4e */ KEY_KP_SUBTRACT,
		/* 4f */ KEY_UNKNOWN,
		/* 50 */ KEY_UNKNOWN,
		/* 51 */ KEY_EQUAL, //wtf equal?
		/* 52 */ KEY_KP_0,
		/* 53 */ KEY_KP_1,
		/* 54 */ KEY_KP_2,
		/* 55 */ KEY_KP_3,
		/* 56 */ KEY_KP_4,
		/* 57 */ KEY_KP_5,
		/* 58 */ KEY_KP_6,
		/* 59 */ KEY_KP_7,
		/* 5a */ KEY_UNKNOWN,
		/* 5b */ KEY_KP_8,
		/* 5c */ KEY_KP_9,
		/* 5d */ KEY_UNKNOWN,
		/* 5e */ KEY_UNKNOWN,
		/* 5f */ KEY_UNKNOWN,
		/* 60 */ KEY_F5,
		/* 61 */ KEY_F6,
		/* 62 */ KEY_F7,
		/* 63 */ KEY_F3,
		/* 64 */ KEY_F8,
		/* 65 */ KEY_F9,
		/* 66 */ KEY_UNKNOWN,
		/* 67 */ KEY_F11,
		/* 68 */ KEY_UNKNOWN,
		/* 69 */ KEY_F13,
		/* 6a */ KEY_F16,
		/* 6b */ KEY_F14,
		/* 6c */ KEY_UNKNOWN,
		/* 6d */ KEY_F10,
		/* 6e */ KEY_UNKNOWN,
		/* 6f */ KEY_F12,
		/* 70 */ KEY_UNKNOWN,
		/* 71 */ KEY_F15,
		/* 72 */ KEY_INSERT, /* Really Help... */
		/* 73 */ KEY_HOME,
		/* 74 */ KEY_PAGEUP,
		/* 75 */ KEY_DELETE,
		/* 76 */ KEY_F4,
		/* 77 */ KEY_END,
		/* 78 */ KEY_F2,
		/* 79 */ KEY_PAGEDOWN,
		/* 7a */ KEY_F1,
		/* 7b */ KEY_LEFT,
		/* 7c */ KEY_RIGHT,
		/* 7d */ KEY_DOWN,
		/* 7e */ KEY_UP,
		/* 7f */ KEY_UNKNOWN,
	};

	if (key >= 128)
		return KEY_UNKNOWN;

	return table[key];
}

- (void)keyDown:(NSEvent *)event {
	InputEvent ev;
	ev.type = InputEvent::KEY;
	ev.key.pressed = true;
	ev.key.mod = translateFlags([event modifierFlags]);
	ev.key.scancode = latin_keyboard_keycode_convert(translateKey([event keyCode]));
	ev.key.echo = [event isARepeat];

	NSString *characters = [event characters];
	NSUInteger i, length = [characters length];

	if (length > 0 && keycode_has_unicode(ev.key.scancode)) {
		for (i = 0; i < length; i++) {
			ev.key.unicode = [characters characterAtIndex:i];
			OS_OSX::singleton->push_input(ev);
			ev.key.scancode = 0;
		}
	} else {
		OS_OSX::singleton->push_input(ev);
	}
}

- (void)flagsChanged:(NSEvent *)event {
	InputEvent ev;
	int key = [event keyCode];
	int mod = [event modifierFlags];

	ev.type = InputEvent::KEY;

	if (key == 0x36 || key == 0x37) {
		if (mod & NSCommandKeyMask) {
			mod &= ~NSCommandKeyMask;
			ev.key.pressed = true;
		} else {
			ev.key.pressed = false;
		}
	} else if (key == 0x38 || key == 0x3c) {
		if (mod & NSShiftKeyMask) {
			mod &= ~NSShiftKeyMask;
			ev.key.pressed = true;
		} else {
			ev.key.pressed = false;
		}
	} else if (key == 0x3a || key == 0x3d) {
		if (mod & NSAlternateKeyMask) {
			mod &= ~NSAlternateKeyMask;
			ev.key.pressed = true;
		} else {
			ev.key.pressed = false;
		}
	} else if (key == 0x3b || key == 0x3e) {
		if (mod & NSControlKeyMask) {
			mod &= ~NSControlKeyMask;
			ev.key.pressed = true;
		} else {
			ev.key.pressed = false;
		}
	} else {
		return;
	}

	ev.key.mod = translateFlags(mod);
	ev.key.scancode = latin_keyboard_keycode_convert(translateKey(key));

	OS_OSX::singleton->push_input(ev);
}

- (void)keyUp:(NSEvent *)event {

	InputEvent ev;
	ev.type = InputEvent::KEY;
	ev.key.pressed = false;
	ev.key.mod = translateFlags([event modifierFlags]);
	ev.key.scancode = latin_keyboard_keycode_convert(translateKey([event keyCode]));
	OS_OSX::singleton->push_input(ev);

	/*
	const int key = translateKey([event keyCode]);
	const int mods = translateFlags([event modifierFlags]);
	_glfwInputKey(window, key, [event keyCode], GLFW_RELEASE, mods);
*/
}

inline void sendScrollEvent(int button, double factor) {
	InputEvent ev;
	ev.type = InputEvent::MOUSE_BUTTON;
	ev.mouse_button.button_index = button;
	ev.mouse_button.factor = factor;
	ev.mouse_button.pressed = true;
	ev.mouse_button.x = mouse_x;
	ev.mouse_button.y = mouse_y;
	ev.mouse_button.global_x = mouse_x;
	ev.mouse_button.global_y = mouse_y;
	ev.mouse_button.button_mask = button_mask;
	OS_OSX::singleton->push_input(ev);
	ev.mouse_button.pressed = false;
	OS_OSX::singleton->push_input(ev);
}

- (void)scrollWheel:(NSEvent *)event {

	double deltaX, deltaY;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6) {
		deltaX = [event scrollingDeltaX];
		deltaY = [event scrollingDeltaY];

		if ([event hasPreciseScrollingDeltas]) {
			deltaX *= 0.03;
			deltaY *= 0.03;
		}
	} else
#endif // MAC_OS_X_VERSION_MAX_ALLOWED
	{
		deltaX = [event deltaX];
		deltaY = [event deltaY];
	}

	if (fabs(deltaX)) {
		sendScrollEvent(0 > deltaX ? BUTTON_WHEEL_RIGHT : BUTTON_WHEEL_LEFT, fabs(deltaX * 0.3));
	}
	if (fabs(deltaY)) {
		sendScrollEvent(0 < deltaY ? BUTTON_WHEEL_UP : BUTTON_WHEEL_DOWN, fabs(deltaY * 0.3));
	}
}

@end

@interface GodotWindow : NSWindow {
}
@end

@implementation GodotWindow

- (BOOL)canBecomeKeyWindow {
	// Required for NSBorderlessWindowMask windows
	return YES;
}

@end

int OS_OSX::get_video_driver_count() const {
	return 1;
}

const char *OS_OSX::get_video_driver_name(int p_driver) const {
	return "GLES2";
}

OS::VideoMode OS_OSX::get_default_video_mode() const {

	VideoMode vm;
	vm.width = 1024;
	vm.height = 600;
	vm.fullscreen = false;
	vm.resizable = true;
	return vm;
}

void OS_OSX::initialize_core() {

	crash_handler.initialize();

	OS_Unix::initialize_core();

	DirAccess::make_default<DirAccessOSX>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessOSX>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessOSX>(DirAccess::ACCESS_FILESYSTEM);

	SemaphoreOSX::make_default();
}

static bool keyboard_layout_dirty = true;
static void keyboard_layout_changed(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef user_info) {
	keyboard_layout_dirty = true;
}

static bool displays_arrangement_dirty = true;
static void displays_arrangement_changed(CGDirectDisplayID display_id, CGDisplayChangeSummaryFlags flags, void *user_info) {
	displays_arrangement_dirty = true;
}

void OS_OSX::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {

	/*** OSX INITIALIZATION ***/
	/*** OSX INITIALIZATION ***/
	/*** OSX INITIALIZATION ***/

	keyboard_layout_dirty = true;
	displays_arrangement_dirty = true;

	// Register to be notified on keyboard layout changes
	CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(),
			NULL, keyboard_layout_changed,
			kTISNotifySelectedKeyboardInputSourceChanged, NULL,
			CFNotificationSuspensionBehaviorDeliverImmediately);

	// Register to be notified on displays arrangement changes
	CGDisplayRegisterReconfigurationCallback(displays_arrangement_changed, NULL);

	if (is_hidpi_allowed() && [[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)]) {
		for (NSScreen *screen in [NSScreen screens]) {
			float s = [screen backingScaleFactor];
			if (s > display_scale) {
				display_scale = s;
			}
		}
	}

	window_delegate = [[GodotWindowDelegate alloc] init];

	// Don't use accumulation buffer support; it's not accelerated
	// Aux buffers probably aren't accelerated either

	unsigned int styleMask;

	if (p_desired.borderless_window) {
		styleMask = NSWindowStyleMaskBorderless;
	} else {
		styleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | (p_desired.resizable ? NSResizableWindowMask : 0);
	}

	window_object = [[GodotWindow alloc]
			initWithContentRect:NSMakeRect(0, 0, p_desired.width, p_desired.height)
					  styleMask:styleMask
						backing:NSBackingStoreBuffered
						  defer:NO];

	ERR_FAIL_COND(window_object == nil);

	window_view = [[GodotContentView alloc] init];

	window_size.width = p_desired.width * display_scale;
	window_size.height = p_desired.height * display_scale;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6 && display_scale > 1) {
		[window_view setWantsBestResolutionOpenGLSurface:YES];
		//if (current_videomode.resizable)
		[window_object setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	}
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/

	//[window_object setTitle:[NSString stringWithUTF8String:"GodotEnginies"]];
	[window_object setContentView:window_view];
	[window_object setDelegate:window_delegate];
	[window_object setAcceptsMouseMovedEvents:YES];
	[window_object center];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6)
		[window_object setRestorable:NO];
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/

	unsigned int attributeCount = 0;

	// OS X needs non-zero color size, so set resonable values
	int colorBits = 32;

// Fail if a robustness strategy was requested
#define ADD_ATTR(x) \
	{ attributes[attributeCount++] = x; }
#define ADD_ATTR2(x, y) \
	{                   \
		ADD_ATTR(x);    \
		ADD_ATTR(y);    \
	}

	// Arbitrary array size here
	NSOpenGLPixelFormatAttribute attributes[40];

	ADD_ATTR(NSOpenGLPFADoubleBuffer);
	ADD_ATTR(NSOpenGLPFAClosestPolicy);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	if (false /* use gl3*/)
		ADD_ATTR2(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/

	ADD_ATTR2(NSOpenGLPFAColorSize, colorBits);

	/*
	if (fbconfig->alphaBits > 0)
		ADD_ATTR2(NSOpenGLPFAAlphaSize, fbconfig->alphaBits);
*/

	ADD_ATTR2(NSOpenGLPFADepthSize, 24);

	ADD_ATTR2(NSOpenGLPFAStencilSize, 8);

	/*
	if (fbconfig->stereo)
		ADD_ATTR(NSOpenGLPFAStereo);
*/

	/*
	if (fbconfig->samples > 0) {
		 ADD_ATTR2(NSOpenGLPFASampleBuffers, 1);
		 ADD_ATTR2(NSOpenGLPFASamples, fbconfig->samples);
	}
*/

	// NOTE: All NSOpenGLPixelFormats on the relevant cards support sRGB
	//       frambuffer, so there's no need (and no way) to request it

	ADD_ATTR(0);

#undef ADD_ATTR
#undef ADD_ATTR2

	pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	ERR_FAIL_COND(pixelFormat == nil);

	context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

	ERR_FAIL_COND(context == nil);

	[context setView:window_view];

	[context makeCurrentContext];

	[NSApp activateIgnoringOtherApps:YES];

	[window_object makeKeyAndOrderFront:nil];

	if (p_desired.fullscreen)
		zoomed = true;

	/*** END OSX INITIALIZATION ***/
	/*** END OSX INITIALIZATION ***/
	/*** END OSX INITIALIZATION ***/

	bool use_gl2 = p_video_driver != 1;

	AudioDriverManagerSW::add_driver(&audio_driver_osx);

	rasterizer = instance_RasterizerGLES2();

	visual_server = memnew(VisualServerRaster(rasterizer));

	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		visual_server = memnew(VisualServerWrapMT(visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}

	visual_server->init();

	AudioDriverManagerSW::get_driver(p_audio_driver)->set_singleton();

	if (AudioDriverManagerSW::get_driver(p_audio_driver)->init() != OK) {

		ERR_PRINT("Initializing audio failed.");
	}

	sample_manager = memnew(SampleManagerMallocSW);
	audio_server = memnew(AudioServerSW(sample_manager));

	audio_server->set_mixer_params(AudioMixerSW::INTERPOLATION_LINEAR, false);
	audio_server->init();

	spatial_sound_server = memnew(SpatialSoundServerSW);
	spatial_sound_server->init();

	spatial_sound_2d_server = memnew(SpatialSound2DServerSW);
	spatial_sound_2d_server->init();

	physics_server = memnew(PhysicsServerSW);
	physics_server->init();
	//physics_2d_server = memnew( Physics2DServerSW );
	physics_2d_server = Physics2DServerWrapMT::init_server<Physics2DServerSW>();
	physics_2d_server->init();

	input = memnew(InputDefault);
	joystick_osx = memnew(JoystickOSX);

	_ensure_data_dir();

	restore_rect = Rect2(get_window_position(), get_window_size());
}

void OS_OSX::finalize() {

	CFNotificationCenterRemoveObserver(CFNotificationCenterGetDistributedCenter(), NULL, kTISNotifySelectedKeyboardInputSourceChanged, NULL);
	CGDisplayRemoveReconfigurationCallback(displays_arrangement_changed, NULL);

	delete_main_loop();

	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

	memdelete(joystick_osx);
	memdelete(input);

	memdelete(sample_manager);

	audio_server->finish();
	memdelete(audio_server);

	visual_server->finish();
	memdelete(visual_server);
	memdelete(rasterizer);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);
}

void OS_OSX::set_main_loop(MainLoop *p_main_loop) {

	main_loop = p_main_loop;
	input->set_main_loop(p_main_loop);
}

void OS_OSX::delete_main_loop() {

	if (!main_loop)
		return;

	memdelete(main_loop);
	main_loop = NULL;
}

String OS_OSX::get_name() {

	return "OSX";
}

void OS_OSX::print_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type) {

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
	if (!_print_error_enabled)
		return;

	const char *err_details;
	if (p_rationale && p_rationale[0])
		err_details = p_rationale;
	else
		err_details = p_code;

	switch (p_type) {
		case ERR_ERROR:
			os_log_error(OS_LOG_DEFAULT, "ERROR: %{public}s: %{public}s\nAt: %{public}s:%i.", p_function, err_details, p_file, p_line);
			print("\E[1;31mERROR: %s: \E[0m\E[1m%s\n", p_function, err_details);
			print("\E[0;31m   At: %s:%i.\E[0m\n", p_file, p_line);
			break;
		case ERR_WARNING:
			os_log_info(OS_LOG_DEFAULT, "WARNING: %{public}s: %{public}s\nAt: %{public}s:%i.", p_function, err_details, p_file, p_line);
			print("\E[1;33mWARNING: %s: \E[0m\E[1m%s\n", p_function, err_details);
			print("\E[0;33m   At: %s:%i.\E[0m\n", p_file, p_line);
			break;
		case ERR_SCRIPT:
			os_log_error(OS_LOG_DEFAULT, "SCRIPT ERROR: %{public}s: %{public}s\nAt: %{public}s:%i.", p_function, err_details, p_file, p_line);
			print("\E[1;35mSCRIPT ERROR: %s: \E[0m\E[1m%s\n", p_function, err_details);
			print("\E[0;35m   At: %s:%i.\E[0m\n", p_file, p_line);
			break;
	}
#else
	OS_Unix::print_error(p_function, p_file, p_line, p_code, p_rationale, p_type);
#endif
}

void OS_OSX::alert(const String &p_alert, const String &p_title) {
	// Set OS X-compliant variables
	NSAlert *window = [[NSAlert alloc] init];
	NSString *ns_title = [NSString stringWithUTF8String:p_title.utf8().get_data()];
	NSString *ns_alert = [NSString stringWithUTF8String:p_alert.utf8().get_data()];

	[window addButtonWithTitle:@"OK"];
	[window setMessageText:ns_title];
	[window setInformativeText:ns_alert];
	[window setAlertStyle:NSWarningAlertStyle];

	// Display it, then release
	[window runModal];
	[window release];
}

void OS_OSX::set_cursor_shape(CursorShape p_shape) {

	if (cursor_shape == p_shape)
		return;

	if (mouse_mode != MOUSE_MODE_VISIBLE) {
		cursor_shape = p_shape;
		return;
	}

	if (cursors[p_shape] != NULL) {
		[cursors[p_shape] set];
	} else {
		switch (p_shape) {
			case CURSOR_ARROW: [[NSCursor arrowCursor] set]; break;
			case CURSOR_IBEAM: [[NSCursor IBeamCursor] set]; break;
			case CURSOR_POINTING_HAND: [[NSCursor pointingHandCursor] set]; break;
			case CURSOR_CROSS: [[NSCursor crosshairCursor] set]; break;
			case CURSOR_WAIT: [[NSCursor arrowCursor] set]; break;
			case CURSOR_BUSY: [[NSCursor arrowCursor] set]; break;
			case CURSOR_DRAG: [[NSCursor closedHandCursor] set]; break;
			case CURSOR_CAN_DROP: [[NSCursor openHandCursor] set]; break;
			case CURSOR_FORBIDDEN: [[NSCursor arrowCursor] set]; break;
			case CURSOR_VSIZE: [[NSCursor resizeUpDownCursor] set]; break;
			case CURSOR_HSIZE: [[NSCursor resizeLeftRightCursor] set]; break;
			case CURSOR_BDIAGSIZE: [[NSCursor arrowCursor] set]; break;
			case CURSOR_FDIAGSIZE: [[NSCursor arrowCursor] set]; break;
			case CURSOR_MOVE: [[NSCursor arrowCursor] set]; break;
			case CURSOR_VSPLIT: [[NSCursor resizeUpDownCursor] set]; break;
			case CURSOR_HSPLIT: [[NSCursor resizeLeftRightCursor] set]; break;
			case CURSOR_HELP: [[NSCursor arrowCursor] set]; break;
			default: {};
		}
	}

	cursor_shape = p_shape;
}

void OS_OSX::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
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

		NSBitmapImageRep *imgrep = [[NSBitmapImageRep alloc]
				initWithBitmapDataPlanes:NULL
							  pixelsWide:int(texture_size.width)
							  pixelsHigh:int(texture_size.height)
						   bitsPerSample:8
						 samplesPerPixel:4
								hasAlpha:YES
								isPlanar:NO
						  colorSpaceName:NSDeviceRGBColorSpace
							 bytesPerRow:int(texture_size.width) * 4
							bitsPerPixel:32];
		ERR_FAIL_COND(imgrep == nil);
		uint8_t *pixels = [imgrep bitmapData];

		int len = int(texture_size.width * texture_size.height);
		DVector<uint8_t> data = image.get_data();
		DVector<uint8_t>::Read r = data.read();

		/* Premultiply the alpha channel */
		for (int i = 0; i < len; i++) {
			int row_index = floor(i / texture_size.width) + atlas_rect.pos.y;
			int column_index = (i % int(texture_size.width)) + atlas_rect.pos.x;

			if (atlas_texture.is_valid()) {
				column_index = MIN(column_index, atlas_rect.size.width - 1);
				row_index = MIN(row_index, atlas_rect.size.height - 1);
			}

			uint32_t color = image.get_pixel(column_index, row_index).to_ARGB32();

			uint8_t alpha = (color >> 24) & 0xFF;
			pixels[i * 4 + 0] = ((color >> 16) & 0xFF) * alpha / 255;
			pixels[i * 4 + 1] = ((color >> 8) & 0xFF) * alpha / 255;
			pixels[i * 4 + 2] = ((color)&0xFF) * alpha / 255;
			pixels[i * 4 + 3] = alpha;
		}

		NSImage *nsimage = [[NSImage alloc] initWithSize:NSMakeSize(texture_size.width, texture_size.height)];
		[nsimage addRepresentation:imgrep];

		[cursors[p_shape] release];
		NSCursor *cursor = [[NSCursor alloc] initWithImage:nsimage hotSpot:NSMakePoint(p_hotspot.x, p_hotspot.y)];

		cursors[p_shape] = cursor;

		if (p_shape == CURSOR_ARROW) {
			if (mouse_mode == MOUSE_MODE_VISIBLE) {
				[cursor set];
			}
		}

		[imgrep release];
		[nsimage release];
	}
}

void OS_OSX::set_mouse_show(bool p_show) {
}

void OS_OSX::set_mouse_grab(bool p_grab) {
}

bool OS_OSX::is_mouse_grab_enabled() const {

	return mouse_grab;
}

void OS_OSX::warp_mouse_pos(const Point2 &p_to) {

	//copied from windows impl with osx native calls
	if (mouse_mode == MOUSE_MODE_CAPTURED) {
		mouse_x = p_to.x;
		mouse_y = p_to.y;
	} else {
		//set OS position

		//local point in window coords
		const NSRect contentRect = [window_view frame];
		NSRect pointInWindowRect = NSMakeRect(p_to.x / display_scale, contentRect.size.height - (p_to.y / display_scale) - 1, 0, 0);
		NSPoint pointOnScreen = [[window_view window] convertRectToScreen:pointInWindowRect].origin;

		//point in scren coords
		CGPoint lMouseWarpPos = { pointOnScreen.x, CGDisplayBounds(CGMainDisplayID()).size.height - pointOnScreen.y };

		//do the warping
		CGEventSourceRef lEventRef = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
		CGEventSourceSetLocalEventsSuppressionInterval(lEventRef, 0.0);
		CGAssociateMouseAndMouseCursorPosition(false);
		CGWarpMouseCursorPosition(lMouseWarpPos);
		CGAssociateMouseAndMouseCursorPosition(true);
	}
}

Point2 OS_OSX::get_mouse_pos() const {

	return Vector2(mouse_x, mouse_y);
}

int OS_OSX::get_mouse_button_state() const {
	return button_mask;
}

void OS_OSX::set_window_title(const String &p_title) {
	title = p_title;

	[window_object setTitle:[NSString stringWithUTF8String:p_title.utf8().get_data()]];
}

void OS_OSX::set_icon(const Image &p_icon) {

	Image img = p_icon;
	img.convert(Image::FORMAT_RGBA);
	NSBitmapImageRep *imgrep = [[[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:NULL
						  pixelsWide:p_icon.get_width()
						  pixelsHigh:p_icon.get_height()
					   bitsPerSample:8
					 samplesPerPixel:4
							hasAlpha:YES
							isPlanar:NO
					  colorSpaceName:NSDeviceRGBColorSpace
						 bytesPerRow:p_icon.get_width() * 4
						bitsPerPixel:32] autorelease];
	ERR_FAIL_COND(imgrep == nil);
	uint8_t *pixels = [imgrep bitmapData];

	int len = img.get_width() * img.get_height();
	DVector<uint8_t> data = img.get_data();
	DVector<uint8_t>::Read r = data.read();

	/* Premultiply the alpha channel */
	for (int i = 0; i < len; i++) {
		uint8_t alpha = r[i * 4 + 3];
		pixels[i * 4 + 0] = (uint8_t)(((uint16_t)r[i * 4 + 0] * alpha) / 255);
		pixels[i * 4 + 1] = (uint8_t)(((uint16_t)r[i * 4 + 1] * alpha) / 255);
		pixels[i * 4 + 2] = (uint8_t)(((uint16_t)r[i * 4 + 2] * alpha) / 255);
		pixels[i * 4 + 3] = alpha;
	}

	NSImage *nsimg = [[[NSImage alloc] initWithSize:NSMakeSize(img.get_width(), img.get_height())] autorelease];
	ERR_FAIL_COND(nsimg == nil);
	[nsimg addRepresentation:imgrep];

	[NSApp setApplicationIconImage:nsimg];
}

MainLoop *OS_OSX::get_main_loop() const {

	return main_loop;
}

bool OS_OSX::can_draw() const {

	return true;
}

void OS_OSX::set_clipboard(const String &p_text) {

	NSArray *types = [NSArray arrayWithObjects:NSStringPboardType, nil];

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard declareTypes:types owner:nil];
	[pasteboard setString:[NSString stringWithUTF8String:p_text.utf8().get_data()]
				  forType:NSStringPboardType];
}

String OS_OSX::get_clipboard() const {

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

	if (![[pasteboard types] containsObject:NSStringPboardType]) {
		return "";
	}

	NSString *object = [pasteboard stringForType:NSStringPboardType];
	if (!object) {
		return "";
	}

	char *utfs = strdup([object UTF8String]);
	String ret;
	ret.parse_utf8(utfs);
	free(utfs);

	return ret;
}

void OS_OSX::release_rendering_thread() {

	[NSOpenGLContext clearCurrentContext];
}

void OS_OSX::make_rendering_thread() {

	[context makeCurrentContext];
}

Error OS_OSX::shell_open(String p_uri) {

	[[NSWorkspace sharedWorkspace] openURL:[[NSURL alloc] initWithString:[NSString stringWithUTF8String:p_uri.utf8().get_data()]]];
	return OK;
}

String OS_OSX::get_locale() const {
	NSString *locale_code = [[NSLocale currentLocale] localeIdentifier];
	return [locale_code UTF8String];
}

void OS_OSX::swap_buffers() {

	[context flushBuffer];
}

void OS_OSX::wm_minimized(bool p_minimized) {

	minimized = p_minimized;
};

void OS_OSX::set_video_mode(const VideoMode &p_video_mode, int p_screen) {
}

OS::VideoMode OS_OSX::get_video_mode(int p_screen) const {

	VideoMode vm;
	vm.width = window_size.width;
	vm.height = window_size.height;

	return vm;
}

void OS_OSX::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
}

int OS_OSX::get_screen_count() const {
	NSArray *screenArray = [NSScreen screens];
	return [screenArray count];
};

// Returns the native top-left screen coordinate of the smallest rectangle
// that encompasses all screens. Needed in get_screen_position(),
// get_window_position, and set_window_position()
// to convert between OS X native screen coordinates and the ones expected by Godot
Point2 OS_OSX::get_screens_origin() const {
	static Point2 origin;

	if (displays_arrangement_dirty) {
		origin = Point2();

		for (int i = 0; i < get_screen_count(); i++) {
			Point2 position = get_native_screen_position(i);
			if (position.x < origin.x) {
				origin.x = position.x;
			}
			if (position.y > origin.y) {
				origin.y = position.y;
			}
		}

		displays_arrangement_dirty = false;
	}

	return origin;
}

static int get_screen_index(NSScreen *screen) {
	const NSUInteger index = [[NSScreen screens] indexOfObject:screen];
	return index == NSNotFound ? 0 : index;
}

int OS_OSX::get_current_screen() const {
	if (window_object) {
		return get_screen_index([window_object screen]);
	} else {
		return get_screen_index([NSScreen mainScreen]);
	}
};

void OS_OSX::set_current_screen(int p_screen) {
	Vector2 wpos = get_window_position() - get_screen_position(get_current_screen());
	set_window_position(wpos + get_screen_position(p_screen));
};

Point2 OS_OSX::get_native_screen_position(int p_screen) const {
	NSArray *screenArray = [NSScreen screens];
	if (p_screen < [screenArray count]) {
		float displayScale = 1.0;

		if (display_scale > 1.0 && [[screenArray objectAtIndex:p_screen] respondsToSelector:@selector(backingScaleFactor)]) {
			displayScale = [[screenArray objectAtIndex:p_screen] backingScaleFactor];
		}

		NSRect nsrect = [[screenArray objectAtIndex:p_screen] frame];
		// Return the top-left corner of the screen, for OS X the y starts at the bottom
		return Point2(nsrect.origin.x, nsrect.origin.y + nsrect.size.height) * displayScale;
	}

	return Point2();
}

Point2 OS_OSX::get_screen_position(int p_screen) const {
	Point2 position = get_native_screen_position(p_screen) - get_screens_origin();
	// OS X native y-coordinate relative to get_screens_origin() is negative,
	// Godot expects a positive value
	position.y *= -1;
	return position;
}

int OS_OSX::get_screen_dpi(int p_screen) const {
	NSArray *screenArray = [NSScreen screens];
	if (p_screen < [screenArray count]) {
		float displayScale = 1.0;

		if (display_scale > 1.0 && [[screenArray objectAtIndex:p_screen] respondsToSelector:@selector(backingScaleFactor)]) {
			displayScale = [[screenArray objectAtIndex:p_screen] backingScaleFactor];
		}

		NSDictionary *description = [[screenArray objectAtIndex:p_screen] deviceDescription];
		NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
		CGSize displayPhysicalSize = CGDisplayScreenSize(
				[[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);

		return (displayPixelSize.width * 25.4f / displayPhysicalSize.width) * displayScale;
	}

	return 72;
}

Size2 OS_OSX::get_screen_size(int p_screen) const {
	NSArray *screenArray = [NSScreen screens];
	if (p_screen < [screenArray count]) {
		float displayScale = 1.0;

		if (display_scale > 1.0 && [[screenArray objectAtIndex:p_screen] respondsToSelector:@selector(backingScaleFactor)]) {
			displayScale = [[screenArray objectAtIndex:p_screen] backingScaleFactor];
		}

		// Note: Use frame to get the whole screen size
		NSRect nsrect = [[screenArray objectAtIndex:p_screen] frame];
		return Size2(nsrect.size.width, nsrect.size.height) * displayScale;
	}

	return Size2();
}

Point2 OS_OSX::get_native_window_position() const {
	NSRect nsrect = [window_object frame];

	Point2 pos;

	// Return the position of the top-left corner, for OS X the y starts at the bottom
	pos.x = nsrect.origin.x * display_scale;
	pos.y = (nsrect.origin.y + nsrect.size.height) * display_scale;

	return pos;
};

Point2 OS_OSX::get_window_position() const {
	Point2 position = get_native_window_position() - get_screens_origin();
	// OS X native y-coordinate relative to get_screens_origin() is negative,
	// Godot expects a positive value
	position.y *= -1;
	return position;
}

void OS_OSX::_update_window() {
	bool borderless_full = false;

	if (get_borderless_window()) {
		NSRect frameRect = [window_object frame];
		NSRect screenRect = [[window_object screen] frame];

		// Check if our window covers up the screen
		if (frameRect.origin.x <= screenRect.origin.x && frameRect.origin.y <= frameRect.origin.y &&
				frameRect.size.width >= screenRect.size.width && frameRect.size.height >= screenRect.size.height) {
			borderless_full = true;
		}
	}

	if (borderless_full) {
		// If the window covers up the screen set the level to above the main menu and hide on deactivate
		[window_object setLevel:NSMainMenuWindowLevel + 1];
		[window_object setHidesOnDeactivate:YES];
	} else {
		// Reset these when our window is not a borderless window that covers up the screen
		[window_object setLevel:NSNormalWindowLevel];
		[window_object setHidesOnDeactivate:NO];
	}
}

void OS_OSX::set_native_window_position(const Point2 &p_position) {

	NSPoint pos;

	pos.x = p_position.x / display_scale;
	pos.y = p_position.y / display_scale;

	[window_object setFrameTopLeftPoint:pos];

	_update_window();
};

void OS_OSX::set_window_position(const Point2 &p_position) {
	Point2 position = p_position;
	// OS X native y-coordinate relative to get_screens_origin() is negative,
	// Godot passes a positive value
	position.y *= -1;
	set_native_window_position(get_screens_origin() + position);
};

Size2 OS_OSX::get_window_size() const {

	return window_size;
};

Size2 OS_OSX::get_real_window_size() const {

	NSRect frame = [window_object frame];
	return Size2(frame.size.width, frame.size.height);
}

void OS_OSX::set_window_size(const Size2 p_size) {

	Size2 size = p_size;

	if (get_borderless_window() == false) {
		// NSRect used by setFrame includes the title bar, so add it to our size.y
		CGFloat menuBarHeight = [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
		if (menuBarHeight != 0.f) {
			size.y += menuBarHeight;
#if MAC_OS_X_VERSION_MAX_ALLOWED <= 101104
		} else {
			size.y += [[NSStatusBar systemStatusBar] thickness];
#endif
		}
	}

	NSRect frame = [window_object frame];
	[window_object setFrame:NSMakeRect(frame.origin.x, frame.origin.y, size.x, size.y) display:YES];

	_update_window();
};

void OS_OSX::set_window_fullscreen(bool p_enabled) {

	if (zoomed != p_enabled) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
		[window_object toggleFullScreen:nil];
#else
		[window_object performZoom:nil];
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/
	}
	zoomed = p_enabled;
};

bool OS_OSX::is_window_fullscreen() const {

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
	if ([window_object respondsToSelector:@selector(isZoomed)])
		return [window_object isZoomed];
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/

	return zoomed;
};

void OS_OSX::set_window_resizable(bool p_enabled) {

	if (p_enabled)
		[window_object setStyleMask:[window_object styleMask] | NSResizableWindowMask];
	else
		[window_object setStyleMask:[window_object styleMask] & ~NSResizableWindowMask];
};

bool OS_OSX::is_window_resizable() const {

	return [window_object styleMask] & NSResizableWindowMask;
};

void OS_OSX::set_window_minimized(bool p_enabled) {

	if (p_enabled)
		[window_object performMiniaturize:nil];
	else
		[window_object deminiaturize:nil];
};

bool OS_OSX::is_window_minimized() const {

	if ([window_object respondsToSelector:@selector(isMiniaturized)])
		return [window_object isMiniaturized];

	return minimized;
};

void OS_OSX::set_window_maximized(bool p_enabled) {

	if (p_enabled) {
		restore_rect = Rect2(get_window_position(), get_window_size());
		[window_object setFrame:[[[NSScreen screens] objectAtIndex:get_current_screen()] visibleFrame] display:YES];
	} else {
		set_window_size(restore_rect.size);
		set_window_position(restore_rect.pos);
	};
	maximized = p_enabled;
};

bool OS_OSX::is_window_maximized() const {

	// don't know
	return maximized;
};

void OS_OSX::move_window_to_foreground() {

	[window_object orderFrontRegardless];
}

void OS_OSX::set_window_always_on_top(bool p_enabled) {
	if (is_window_always_on_top() == p_enabled)
		return;

	if (p_enabled)
		[window_object setLevel:NSFloatingWindowLevel];
	else
		[window_object setLevel:NSNormalWindowLevel];
}

bool OS_OSX::is_window_always_on_top() const {
	return [window_object level] == NSFloatingWindowLevel;
}

void OS_OSX::request_attention() {

	[NSApp requestUserAttention:NSCriticalRequest];
}

void OS_OSX::set_borderless_window(int p_borderless) {

	// OrderOut prevents a lose focus bug with the window
	[window_object orderOut:nil];

	if (p_borderless) {
		[window_object setStyleMask:NSWindowStyleMaskBorderless];
	} else {
		[window_object setStyleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask];

		// Force update of the window styles
		NSRect frameRect = [window_object frame];
		[window_object setFrame:NSMakeRect(frameRect.origin.x, frameRect.origin.y, frameRect.size.width + 1, frameRect.size.height) display:NO];
		[window_object setFrame:frameRect display:NO];

		// Restore the window title
		[window_object setTitle:[NSString stringWithUTF8String:title.utf8().get_data()]];
	}

	_update_window();

	[window_object makeKeyAndOrderFront:nil];
}

bool OS_OSX::get_borderless_window() {

	return [window_object styleMask] == NSWindowStyleMaskBorderless;
}

String OS_OSX::get_executable_path() const {

	int ret;
	pid_t pid;
	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];

	pid = getpid();
	ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
	if (ret <= 0) {
		return OS::get_executable_path();
	} else {
		String path;
		path.parse_utf8(pathbuf);

		return path;
	}
}

// Returns string representation of keys, if they are printable.
//
static NSString *createStringForKeys(const CGKeyCode *keyCode, int length) {

	TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardInputSource();
	if (!currentKeyboard)
		return nil;

	CFDataRef layoutData = (CFDataRef)TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
	if (!layoutData)
		return nil;

	const UCKeyboardLayout *keyboardLayout = (const UCKeyboardLayout *)CFDataGetBytePtr(layoutData);

	OSStatus err;
	CFMutableStringRef output = CFStringCreateMutable(NULL, 0);

	for (int i = 0; i < length; ++i) {

		UInt32 keysDown = 0;
		UniChar chars[4];
		UniCharCount realLength;

		err = UCKeyTranslate(keyboardLayout,
				keyCode[i],
				kUCKeyActionDisplay,
				0,
				LMGetKbdType(),
				kUCKeyTranslateNoDeadKeysBit,
				&keysDown,
				sizeof(chars) / sizeof(chars[0]),
				&realLength,
				chars);

		if (err != noErr) {
			CFRelease(output);
			return nil;
		}

		CFStringAppendCharacters(output, chars, 1);
	}

	//CFStringUppercase(output, NULL);

	return (NSString *)output;
}

OS::LatinKeyboardVariant OS_OSX::get_latin_keyboard_variant() const {

	static LatinKeyboardVariant layout = LATIN_KEYBOARD_QWERTY;

	if (keyboard_layout_dirty) {

		layout = LATIN_KEYBOARD_QWERTY;

		CGKeyCode keys[] = { kVK_ANSI_Q, kVK_ANSI_W, kVK_ANSI_E, kVK_ANSI_R, kVK_ANSI_T, kVK_ANSI_Y };
		NSString *test = createStringForKeys(keys, 6);

		if ([test isEqualToString:@"qwertz"]) {
			layout = LATIN_KEYBOARD_QWERTZ;
		} else if ([test isEqualToString:@"azerty"]) {
			layout = LATIN_KEYBOARD_AZERTY;
		} else if ([test isEqualToString:@"qzerty"]) {
			layout = LATIN_KEYBOARD_QZERTY;
		} else if ([test isEqualToString:@"',.pyf"]) {
			layout = LATIN_KEYBOARD_DVORAK;
		} else if ([test isEqualToString:@"xvlcwk"]) {
			layout = LATIN_KEYBOARD_NEO;
		} else if ([test isEqualToString:@"qwfpgj"]) {
			layout = LATIN_KEYBOARD_COLEMAK;
		}

		[test release];

		keyboard_layout_dirty = false;
		return layout;
	}

	return layout;
}

void OS_OSX::process_events() {

	while (true) {
		NSEvent *event = [NSApp
				nextEventMatchingMask:NSAnyEventMask
							untilDate:[NSDate distantPast]
							   inMode:NSDefaultRunLoopMode
							  dequeue:YES];
		if (event == nil)
			break;

		[NSApp sendEvent:event];
	}

	[autoreleasePool drain];
	autoreleasePool = [[NSAutoreleasePool alloc] init];
}

void OS_OSX::push_input(const InputEvent &p_event) {

	InputEvent ev = p_event;
	ev.ID = last_id++;
	input->parse_input_event(ev);
}

void OS_OSX::run() {

	force_quit = false;

	if (!main_loop)
		return;

	main_loop->init();

	if (zoomed) {
		zoomed = false;
		set_window_fullscreen(true);
	}

	//uint64_t last_ticks=get_ticks_usec();

	//int frames=0;
	//uint64_t frame=0;

	while (!force_quit) {

		process_events(); // get rid of pending events
		last_id = joystick_osx->process_joysticks(last_id);
		if (Main::iteration() == true)
			break;
	};

	main_loop->finish();
}

void OS_OSX::set_mouse_mode(MouseMode p_mode) {

	if (p_mode == mouse_mode)
		return;

	if (p_mode == MOUSE_MODE_CAPTURED) {
		// Apple Docs state that the display parameter is not used.
		// "This parameter is not used. By default, you may pass kCGDirectMainDisplay."
		// https://developer.apple.com/library/mac/documentation/graphicsimaging/reference/Quartz_Services_Ref/Reference/reference.html
		CGDisplayHideCursor(kCGDirectMainDisplay);
		CGAssociateMouseAndMouseCursorPosition(false);
	} else if (p_mode == MOUSE_MODE_HIDDEN) {
		CGDisplayHideCursor(kCGDirectMainDisplay);
		CGAssociateMouseAndMouseCursorPosition(true);
	} else {
		CGDisplayShowCursor(kCGDirectMainDisplay);
		CGAssociateMouseAndMouseCursorPosition(true);
	}

	mouse_mode = p_mode;
}

OS::MouseMode OS_OSX::get_mouse_mode() const {

	return mouse_mode;
}

String OS_OSX::get_joy_guid(int p_device) const {
	return input->get_joy_guid_remapped(p_device);
}

Error OS_OSX::move_path_to_trash(String p_dir) {
	NSFileManager *fm = [NSFileManager defaultManager];
	NSURL *url = [NSURL fileURLWithPath:@(p_dir.utf8().get_data())];
	NSError *err;

	if (![fm trashItemAtURL:url resultingItemURL:nil error:&err]) {
		ERR_PRINTS("trashItemAtURL error: " + String(err.localizedDescription.UTF8String));
		return FAILED;
	}

	return OK;
}

void OS_OSX::set_use_vsync(bool p_enable) {
	CGLContextObj ctx = CGLGetCurrentContext();
	if (ctx) {
		GLint swapInterval = p_enable ? 1 : 0;
		CGLSetParameter(ctx, kCGLCPSwapInterval, &swapInterval);
	}
}

bool OS_OSX::is_vsync_enabled() const {
	GLint swapInterval = 0;
	CGLContextObj ctx = CGLGetCurrentContext();
	if (ctx) {
		CGLGetParameter(ctx, kCGLCPSwapInterval, &swapInterval);
	}
	return swapInterval ? true : false;
}

OS_OSX *OS_OSX::singleton = NULL;

OS_OSX::OS_OSX() {

	mouse_mode = OS::MOUSE_MODE_VISIBLE;
	main_loop = NULL;
	singleton = this;
	autoreleasePool = [[NSAutoreleasePool alloc] init];

	eventSource = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	ERR_FAIL_COND(!eventSource);

	CGEventSourceSetLocalEventsSuppressionInterval(eventSource, 0.0);

	/*
	if (pthread_key_create(&_Godot.nsgl.current, NULL) != 0) {
		_GodotInputError(Godot_PLATFORM_ERROR,
			"NSGL: Failed to create context TLS");
		return GL_FALSE;
	}
*/

	framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
	ERR_FAIL_COND(!framework);

	// Implicitly create shared NSApplication instance
	[GodotApplication sharedApplication];

	// In case we are unbundled, make us a proper UI application
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	// Menu bar setup must go between sharedApplication above and
	// finishLaunching below, in order to properly emulate the behavior
	// of NSApplicationMain
	NSMenuItem *menu_item;
	NSString *title;

	NSString *nsappname = [[[NSBundle mainBundle] performSelector:@selector(localizedInfoDictionary)] objectForKey:@"CFBundleName"];
	if (nsappname == nil)
		nsappname = [[NSProcessInfo processInfo] processName];

	// Setup Apple menu
	NSMenu *apple_menu = [[NSMenu alloc] initWithTitle:@""];
	title = [NSString stringWithFormat:NSLocalizedString(@"About %@", nil), nsappname];
	[apple_menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

	[apple_menu addItem:[NSMenuItem separatorItem]];

	NSMenu *services = [[NSMenu alloc] initWithTitle:@""];
	menu_item = [apple_menu addItemWithTitle:NSLocalizedString(@"Services", nil) action:nil keyEquivalent:@""];
	[apple_menu setSubmenu:services forItem:menu_item];
	[NSApp setServicesMenu:services];
	[services release];

	[apple_menu addItem:[NSMenuItem separatorItem]];

	title = [NSString stringWithFormat:NSLocalizedString(@"Hide %@", nil), nsappname];
	[apple_menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

	menu_item = [apple_menu addItemWithTitle:NSLocalizedString(@"Hide Others", nil) action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[menu_item setKeyEquivalentModifierMask:(NSAlternateKeyMask | NSCommandKeyMask)];

	[apple_menu addItemWithTitle:NSLocalizedString(@"Show all", nil) action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[apple_menu addItem:[NSMenuItem separatorItem]];

	title = [NSString stringWithFormat:NSLocalizedString(@"Quit %@", nil), nsappname];
	[apple_menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

	// Setup menu bar
	NSMenu *main_menu = [[NSMenu alloc] initWithTitle:@""];
	menu_item = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	[main_menu setSubmenu:apple_menu forItem:menu_item];
	[NSApp setMainMenu:main_menu];

	[main_menu release];
	[apple_menu release];

	[NSApp finishLaunching];

	delegate = [[GodotApplicationDelegate alloc] init];
	ERR_FAIL_COND(!delegate);
	[NSApp setDelegate:delegate];

	last_id = 1;
	cursor_shape = CURSOR_ARROW;

	maximized = false;
	minimized = false;
	window_size = Vector2(1024, 600);
	zoomed = false;
	display_scale = 1.0;
}

void OS_OSX::disable_crash_handler() {
	crash_handler.disable();
}

bool OS_OSX::is_disable_crash_handler() const {
	return crash_handler.is_disabled();
}
