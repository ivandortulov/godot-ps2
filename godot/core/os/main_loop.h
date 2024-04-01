/*************************************************************************/
/*  main_loop.h                                                          */
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
#ifndef MAIN_LOOP_H
#define MAIN_LOOP_H

#include "os/input_event.h"
#include "reference.h"
#include "script_language.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class MainLoop : public Object {

	OBJ_TYPE(MainLoop, Object);
	OBJ_CATEGORY("Main Loop");

	Ref<Script> init_script;

protected:
	static void _bind_methods();

public:
	enum {
		NOTIFICATION_WM_MOUSE_ENTER = 3,
		NOTIFICATION_WM_MOUSE_EXIT = 4,
		NOTIFICATION_WM_FOCUS_IN = 5,
		NOTIFICATION_WM_FOCUS_OUT = 6,
		NOTIFICATION_WM_QUIT_REQUEST = 7,
		NOTIFICATION_WM_UNFOCUS_REQUEST = 8,
		NOTIFICATION_OS_MEMORY_WARNING = 9,
	};

	virtual void input_event(const InputEvent &p_event);
	virtual void input_text(const String &p_text);

	virtual void init();
	virtual bool iteration(float p_time);
	virtual bool idle(float p_time);
	virtual void finish();

	virtual void drop_files(const Vector<String> &p_files, int p_from_screen = 0);

	void set_init_script(const Ref<Script> &p_init_script);

	MainLoop();
	virtual ~MainLoop();
};

#endif
