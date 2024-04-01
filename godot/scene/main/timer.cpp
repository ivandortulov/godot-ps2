/*************************************************************************/
/*  timer.cpp                                                            */
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
#include "timer.h"

void Timer::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_READY: {

			if (autostart) {
#ifdef TOOLS_ENABLED
				if (get_tree()->is_editor_hint() && get_tree()->get_edited_scene_root() && (get_tree()->get_edited_scene_root() == this || get_tree()->get_edited_scene_root()->is_a_parent_of(this)))
					break;
#endif
				start();
				autostart = false;
			}
		} break;
		case NOTIFICATION_PROCESS: {
			if (timer_process_mode == TIMER_PROCESS_FIXED || !is_processing())
				return;
			time_left -= get_process_delta_time();

			if (time_left < 0) {
				if (!one_shot)
					time_left += wait_time;
				else
					stop();

				emit_signal("timeout");
			}

		} break;
		case NOTIFICATION_FIXED_PROCESS: {
			if (timer_process_mode == TIMER_PROCESS_IDLE || !is_fixed_processing())
				return;
			time_left -= get_fixed_process_delta_time();

			if (time_left < 0) {
				if (!one_shot)
					time_left += wait_time;
				else
					stop();
				emit_signal("timeout");
			}

		} break;
	}
}

void Timer::set_wait_time(float p_time) {
	ERR_EXPLAIN("time should be greater than zero.");
	ERR_FAIL_COND(p_time <= 0);
	wait_time = p_time;
}
float Timer::get_wait_time() const {

	return wait_time;
}

void Timer::set_one_shot(bool p_one_shot) {

	one_shot = p_one_shot;
}
bool Timer::is_one_shot() const {

	return one_shot;
}

void Timer::set_autostart(bool p_start) {

	autostart = p_start;
}
bool Timer::has_autostart() const {

	return autostart;
}

void Timer::start() {
	time_left = wait_time;
	_set_process(true);
}

void Timer::stop() {
	time_left = -1;
	_set_process(false);
	autostart = false;
}

void Timer::set_active(bool p_active) {
	if (active == p_active)
		return;

	active = p_active;
	_set_process(processing);
}

bool Timer::is_active() const {
	return active;
}

float Timer::get_time_left() const {

	return time_left > 0 ? time_left : 0;
}

void Timer::set_timer_process_mode(TimerProcessMode p_mode) {

	if (timer_process_mode == p_mode)
		return;

	switch (timer_process_mode) {
		case TIMER_PROCESS_FIXED:
			if (is_fixed_processing()) {
				set_fixed_process(false);
				set_process(true);
			}
			break;
		case TIMER_PROCESS_IDLE:
			if (is_processing()) {
				set_process(false);
				set_fixed_process(true);
			}
			break;
	}
	timer_process_mode = p_mode;
}

Timer::TimerProcessMode Timer::get_timer_process_mode() const {

	return timer_process_mode;
}

void Timer::_set_process(bool p_process, bool p_force) {
	switch (timer_process_mode) {
		case TIMER_PROCESS_FIXED: set_fixed_process(p_process && active); break;
		case TIMER_PROCESS_IDLE: set_process(p_process && active); break;
	}
	processing = p_process;
}

void Timer::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_wait_time", "time_sec"), &Timer::set_wait_time);
	ObjectTypeDB::bind_method(_MD("get_wait_time"), &Timer::get_wait_time);

	ObjectTypeDB::bind_method(_MD("set_one_shot", "enable"), &Timer::set_one_shot);
	ObjectTypeDB::bind_method(_MD("is_one_shot"), &Timer::is_one_shot);

	ObjectTypeDB::bind_method(_MD("set_autostart", "enable"), &Timer::set_autostart);
	ObjectTypeDB::bind_method(_MD("has_autostart"), &Timer::has_autostart);

	ObjectTypeDB::bind_method(_MD("start"), &Timer::start);
	ObjectTypeDB::bind_method(_MD("stop"), &Timer::stop);

	ObjectTypeDB::bind_method(_MD("set_active", "active"), &Timer::set_active);
	ObjectTypeDB::bind_method(_MD("is_active"), &Timer::is_active);

	ObjectTypeDB::bind_method(_MD("get_time_left"), &Timer::get_time_left);

	ObjectTypeDB::bind_method(_MD("set_timer_process_mode", "mode"), &Timer::set_timer_process_mode);
	ObjectTypeDB::bind_method(_MD("get_timer_process_mode"), &Timer::get_timer_process_mode);

	ADD_SIGNAL(MethodInfo("timeout"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Fixed,Idle"), _SCS("set_timer_process_mode"), _SCS("get_timer_process_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "wait_time", PROPERTY_HINT_EXP_RANGE, "0.01,4096,0.01"), _SCS("set_wait_time"), _SCS("get_wait_time"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), _SCS("set_one_shot"), _SCS("is_one_shot"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autostart"), _SCS("set_autostart"), _SCS("has_autostart"));

	BIND_CONSTANT(TIMER_PROCESS_FIXED);
	BIND_CONSTANT(TIMER_PROCESS_IDLE);
}

Timer::Timer() {
	timer_process_mode = TIMER_PROCESS_IDLE;
	autostart = false;
	wait_time = 1;
	one_shot = false;
	time_left = -1;
	processing = false;
	active = true;
}
