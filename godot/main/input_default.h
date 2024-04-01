/*************************************************************************/
/*  input_default.h                                                      */
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
#ifndef INPUT_DEFAULT_H
#define INPUT_DEFAULT_H

#include "os/input.h"

class InputDefault : public Input {

	OBJ_TYPE(InputDefault, Input);
	_THREAD_SAFE_CLASS_

	int mouse_button_mask;
	Set<int> keys_pressed;
	Set<int> joy_buttons_pressed;
	Map<int, float> _joy_axis;
	Map<StringName, int> custom_action_press;
	Vector3 gravity;
	Vector3 accelerometer;
	Vector3 magnetometer;
	Vector3 gyroscope;
	Vector2 mouse_pos;
	MainLoop *main_loop;

	bool emulate_touch;

	struct VibrationInfo {
		float weak_magnitude;
		float strong_magnitude;
		float duration; // Duration in seconds
		uint64_t timestamp;
	};

	Map<int, VibrationInfo> joy_vibration;

	struct SpeedTrack {

		uint64_t last_tick;
		Vector2 speed;
		Vector2 accum;
		float accum_t;
		float min_ref_frame;
		float max_ref_frame;

		void update(const Vector2 &p_delta_p);
		void reset();
		SpeedTrack();
	};

	struct Joystick {
		StringName name;
		StringName uid;
		bool connected;
		bool last_buttons[JOY_BUTTON_MAX + 19]; //apparently SDL specifies 35 possible buttons on android
		float last_axis[JOY_AXIS_MAX];
		float filter;
		int last_hat;
		int mapping;
		int hat_current;

		Joystick() {

			for (int i = 0; i < JOY_AXIS_MAX; i++) {

				last_axis[i] = 0.0f;
			}
			for (int i = 0; i < JOY_BUTTON_MAX + 19; i++) {

				last_buttons[i] = false;
			}
			connected = false;
			last_hat = HAT_MASK_CENTER;
			filter = 0.01f;
			mapping = -1;
		}
	};

	SpeedTrack mouse_speed_track;
	Map<int, Joystick> joy_names;
	int fallback_mapping;
	RES custom_cursors[CURSOR_MAX] = { NULL };

public:
	enum HatMask {
		HAT_MASK_CENTER = 0,
		HAT_MASK_UP = 1,
		HAT_MASK_RIGHT = 2,
		HAT_MASK_DOWN = 4,
		HAT_MASK_LEFT = 8,
	};

	enum HatDir {
		HAT_UP,
		HAT_RIGHT,
		HAT_DOWN,
		HAT_LEFT,
		HAT_MAX,
	};

	enum {
		JOYSTICKS_MAX = 16,
	};

	struct JoyAxis {
		int min;
		float value;
	};

private:
	enum JoyType {
		TYPE_BUTTON,
		TYPE_AXIS,
		TYPE_HAT,
		TYPE_MAX,
	};

	struct JoyEvent {
		int type;
		int index;
		int value;
	};

	struct JoyDeviceMapping {

		String uid;
		String name;
		Map<int, JoyEvent> buttons;
		Map<int, JoyEvent> axis;
		JoyEvent hat[HAT_MAX];
	};

	JoyEvent hat_map_default[HAT_MAX];

	Vector<JoyDeviceMapping> map_db;

	JoyEvent _find_to_event(String p_to);
	uint32_t _button_event(uint32_t p_last_id, int p_device, int p_index, bool p_pressed);
	uint32_t _axis_event(uint32_t p_last_id, int p_device, int p_axis, float p_value);
	float _handle_deadzone(int p_device, int p_axis, float p_value);

public:
	virtual bool is_key_pressed(int p_scancode);
	virtual bool is_mouse_button_pressed(int p_button);
	virtual bool is_joy_button_pressed(int p_device, int p_button);
	virtual bool is_action_pressed(const StringName &p_action);

	virtual float get_joy_axis(int p_device, int p_axis);
	String get_joy_name(int p_idx);
	virtual Array get_connected_joysticks();
	virtual Vector2 get_joy_vibration_strength(int p_device);
	virtual float get_joy_vibration_duration(int p_device);
	virtual uint64_t get_joy_vibration_timestamp(int p_device);
	void joy_connection_changed(int p_idx, bool p_connected, String p_name, String p_guid = "");
	void parse_joystick_mapping(String p_mapping, bool p_update_existing);

	virtual Vector3 get_gravity();
	virtual Vector3 get_accelerometer();
	virtual Vector3 get_magnetometer();
	virtual Vector3 get_gyroscope();

	virtual Point2 get_mouse_pos() const;
	virtual Point2 get_mouse_speed() const;
	virtual int get_mouse_button_mask() const;

	virtual void warp_mouse_pos(const Vector2 &p_to);
	virtual Point2i warp_mouse_motion(const InputEventMouseMotion &p_motion, const Rect2 &p_rect);

	virtual void parse_input_event(const InputEvent &p_event);

	void set_gravity(const Vector3 &p_gravity);
	void set_accelerometer(const Vector3 &p_accel);
	void set_magnetometer(const Vector3 &p_magnetometer);
	void set_gyroscope(const Vector3 &p_gyroscope);
	void set_joy_axis(int p_device, int p_axis, float p_value);

	virtual void start_joy_vibration(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration = 0);
	virtual void stop_joy_vibration(int p_device);

	void set_main_loop(MainLoop *main_loop);
	void set_mouse_pos(const Point2 &p_posf);

	void action_press(const StringName &p_action);
	void action_release(const StringName &p_action);

	void iteration(float p_step);

	void set_emulate_touch(bool p_emulate);
	virtual bool is_emulating_touchscreen() const;

	virtual void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape = Input::CURSOR_ARROW, const Vector2 &p_hotspot = Vector2());

	void parse_mapping(String p_mapping);
	uint32_t joy_button(uint32_t p_last_id, int p_device, int p_button, bool p_pressed);
	uint32_t joy_axis(uint32_t p_last_id, int p_device, int p_axis, const JoyAxis &p_value);
	uint32_t joy_hat(uint32_t p_last_id, int p_device, int p_val);

	virtual void add_joy_mapping(String p_mapping, bool p_update_existing = false);
	virtual void remove_joy_mapping(String p_guid);
	virtual bool is_joy_known(int p_device);
	virtual String get_joy_guid(int p_device) const;

	virtual String get_joy_button_string(int p_button);
	virtual String get_joy_axis_string(int p_axis);
	virtual int get_joy_axis_index_from_string(String p_axis);
	virtual int get_joy_button_index_from_string(String p_button);

	int get_unused_joy_id();

	bool is_joy_mapped(int p_device);
	String get_joy_guid_remapped(int p_device) const;
	void set_fallback_mapping(String p_guid);
	InputDefault();
};

#endif // INPUT_DEFAULT_H
