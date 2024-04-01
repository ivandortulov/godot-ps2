/*************************************************************************/
/*  os_android.h                                                         */
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
#ifndef OS_ANDROID_H
#define OS_ANDROID_H

#include "drivers/unix/os_unix.h"
#include "main/input_default.h"
#include "os/input.h"
#include "os/main_loop.h"
#include "servers/audio/audio_server_sw.h"
#include "servers/physics/physics_server_sw.h"
#include "servers/physics_2d/physics_2d_server_sw.h"
#include "servers/physics_2d/physics_2d_server_wrap_mt.h"
#include "servers/spatial_sound/spatial_sound_server_sw.h"
#include "servers/spatial_sound_2d/spatial_sound_2d_server_sw.h"
#include "servers/visual/rasterizer.h"

//#ifdef USE_JAVA_FILE_ACCESS

#ifdef ANDROID_NATIVE_ACTIVITY

#include <android/log.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>

#else

#endif

#include "audio_driver_jandroid.h"
#include "audio_driver_opensl.h"

typedef void (*GFXInitFunc)(void *ud, bool gl2);
typedef int (*OpenURIFunc)(const String &);
typedef String (*GetDataDirFunc)();
typedef String (*GetLocaleFunc)();
typedef void (*SetClipboardFunc)(const String &);
typedef String (*GetClipboardFunc)();
typedef void (*EmitErrorSignal)(const String &, const String &, const String &, const String &, int);
typedef String (*GetModelFunc)();
typedef int (*GetScreenDPIFunc)();
typedef String (*GetUniqueIDFunc)();
typedef void (*ShowVirtualKeyboardFunc)(const String &);
typedef void (*HideVirtualKeyboardFunc)();
typedef void (*SetScreenOrientationFunc)(int);
typedef String (*GetSystemDirFunc)(int);

typedef void (*VideoPlayFunc)(const String &);
typedef bool (*VideoIsPlayingFunc)();
typedef void (*VideoPauseFunc)();
typedef void (*VideoStopFunc)();
typedef void (*SetKeepScreenOnFunc)(bool p_enabled);
typedef void (*AlertFunc)(const String &, const String &);

class OS_Android : public OS_Unix {
public:
	struct TouchPos {
		int id;
		Point2 pos;
	};

	enum {
		JOY_EVENT_BUTTON = 0,
		JOY_EVENT_AXIS = 1,
		JOY_EVENT_HAT = 2
	};

	struct JoystickEvent {

		int device;
		int type;
		int index;
		bool pressed;
		float value;
		int hat;
	};

private:
	Vector<TouchPos> touch;

	Point2 last_mouse;
	unsigned int last_id;
	GFXInitFunc gfx_init_func;
	void *gfx_init_ud;

	bool use_gl2;
	bool use_reload_hooks;
	bool use_apk_expansion;

	bool use_16bits_fbo;

	Rasterizer *rasterizer;
	VisualServer *visual_server;
	AudioServerSW *audio_server;
	SampleManagerMallocSW *sample_manager;
	SpatialSoundServerSW *spatial_sound_server;
	SpatialSound2DServerSW *spatial_sound_2d_server;
	PhysicsServer *physics_server;
	Physics2DServer *physics_2d_server;

	mutable String data_dir_cache;

#if 0
	AudioDriverAndroid audio_driver_android;
#else
	AudioDriverOpenSL audio_driver_android;
#endif

	const char *gl_extensions;

	InputDefault *input;
	VideoMode default_videomode;
	MainLoop *main_loop;

	OpenURIFunc open_uri_func;
	GetDataDirFunc get_data_dir_func;
	GetLocaleFunc get_locale_func;
	SetClipboardFunc set_clipboard_func;
	GetClipboardFunc get_clipboard_func;
	EmitErrorSignal emit_error_signal;
	GetModelFunc get_model_func;
	GetScreenDPIFunc get_screen_dpi_func;
	ShowVirtualKeyboardFunc show_virtual_keyboard_func;
	HideVirtualKeyboardFunc hide_virtual_keyboard_func;
	SetScreenOrientationFunc set_screen_orientation_func;
	GetUniqueIDFunc get_unique_id_func;
	GetSystemDirFunc get_system_dir_func;

	VideoPlayFunc video_play_func;
	VideoIsPlayingFunc video_is_playing_func;
	VideoPauseFunc video_pause_func;
	VideoStopFunc video_stop_func;
	SetKeepScreenOnFunc set_keep_screen_on_func;
	AlertFunc alert_func;

public:
	// functions used by main to initialize/deintialize the OS
	virtual int get_video_driver_count() const;
	virtual const char *get_video_driver_name(int p_driver) const;

	virtual VideoMode get_default_video_mode() const;

	virtual int get_audio_driver_count() const;
	virtual const char *get_audio_driver_name(int p_driver) const;

	virtual void initialize_core();
	virtual void initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver);

	virtual void set_main_loop(MainLoop *p_main_loop);
	virtual void delete_main_loop();

	virtual void finalize();

	typedef int64_t ProcessID;

	static OS *get_singleton();

	void print_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type);
	virtual void vprint(const char *p_format, va_list p_list, bool p_stderr = false);
	virtual void print(const char *p_format, ...);
	virtual void alert(const String &p_alert, const String &p_title = "ALERT!");

	virtual void set_mouse_show(bool p_show);
	virtual void set_mouse_grab(bool p_grab);
	virtual bool is_mouse_grab_enabled() const;
	virtual Point2 get_mouse_pos() const;
	virtual int get_mouse_button_state() const;
	virtual void set_window_title(const String &p_title);

	virtual void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0);
	virtual VideoMode get_video_mode(int p_screen = 0) const;
	virtual void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const;

	virtual void set_keep_screen_on(bool p_enabled);

	virtual Size2 get_window_size() const;

	virtual String get_name();
	virtual MainLoop *get_main_loop() const;

	virtual bool can_draw() const;

	virtual void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot);
	virtual void set_cursor_shape(CursorShape p_shape);

	void main_loop_begin();
	bool main_loop_iterate();
	void main_loop_request_quit();
	void main_loop_end();
	void main_loop_focusout();
	void main_loop_focusin();

	virtual bool has_touchscreen_ui_hint() const;

	virtual bool has_virtual_keyboard() const;
	virtual void show_virtual_keyboard(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2());
	virtual void hide_virtual_keyboard();

	void set_opengl_extensions(const char *p_gl_extensions);
	void set_display_size(Size2 p_size);

	void reload_gfx();
	void set_context_is_16_bits(bool p_is_16);

	void set_need_reload_hooks(bool p_needs_them);
	virtual void set_screen_orientation(ScreenOrientation p_orientation);

	virtual Error shell_open(String p_uri);
	virtual String get_data_dir() const;
	virtual String get_resource_dir() const;
	virtual String get_locale() const;
	virtual void set_clipboard(const String &p_text);
	virtual String get_clipboard() const;
	virtual String get_model_name() const;
	virtual int get_screen_dpi(int p_screen = 0) const;

	virtual String get_unique_ID() const;

	virtual String get_system_dir(SystemDir p_dir) const;

	void process_accelerometer(const Vector3 &p_accelerometer);
	void process_gravitymeter(const Vector3 &p_gravitymeter);
	void process_magnetometer(const Vector3 &p_magnetometer);
	void process_gyroscope(const Vector3 &p_gyroscope);
	void process_touch(int p_what, int p_pointer, const Vector<TouchPos> &p_points);
	void process_joy_event(JoystickEvent p_event);
	void process_event(InputEvent p_event);
	void init_video_mode(int p_video_width, int p_video_height);

	virtual Error native_video_play(String p_path, float p_volume);
	virtual bool native_video_is_playing();
	virtual void native_video_pause();
	virtual void native_video_stop();

	virtual bool is_joy_known(int p_device);
	virtual String get_joy_guid(int p_device) const;
	void joy_connection_changed(int p_device, bool p_connected, String p_name);

	OS_Android(GFXInitFunc p_gfx_init_func, void *p_gfx_init_ud, OpenURIFunc p_open_uri_func, GetDataDirFunc p_get_data_dir_func, GetLocaleFunc p_get_locale_func, GetModelFunc p_get_model_func, GetScreenDPIFunc p_get_screen_dpi_func, ShowVirtualKeyboardFunc p_show_vk, HideVirtualKeyboardFunc p_hide_vk, SetScreenOrientationFunc p_screen_orient, GetUniqueIDFunc p_get_unique_id, GetSystemDirFunc p_get_sdir_func, VideoPlayFunc p_video_play_func, VideoIsPlayingFunc p_video_is_playing_func, VideoPauseFunc p_video_pause_func, VideoStopFunc p_video_stop_func, SetKeepScreenOnFunc p_set_keep_screen_on_func, AlertFunc p_alert_func, bool p_use_apk_expansion, SetClipboardFunc p_set_clipboard_func, GetClipboardFunc p_get_clipboard_func, EmitErrorSignal p_emit_error_signal);
	~OS_Android();
};

#endif
