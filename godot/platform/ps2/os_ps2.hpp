#ifndef _OS_PS2_H_
#define _OS_PS2_H_

#include "core/os/input.h"
#include "core/os/os.h"
#include "main/input_default.h"
#include "core/os/main_loop.h"
#include "core/error_list.h"
#include "servers/visual/rasterizer.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/physics_server.h"
#include "servers/physics_2d_server.h"
#include "servers/spatial_sound/spatial_sound_server_sw.h"
#include "servers/spatial_sound_2d/spatial_sound_2d_server_sw.h"


class OS_PS2 : public OS {

protected:
	virtual int get_video_driver_count() const;
	virtual const char* get_video_driver_name(int p_driver) const;
	virtual VideoMode get_default_video_mode() const;

	virtual void initialize_core();
	virtual void finalize_core();

	virtual void initialize(const VideoMode& p_desired, int p_video_driver, int p_audio_driver);
	virtual void finalize();

	virtual void set_main_loop(MainLoop* p_main_loop);

public:
	virtual String get_name();

	virtual void set_cursor_shape(CursorShape p_shape);
	virtual void set_custom_mouse_cursor(const RES& p_cursor, CursorShape p_shape, const Vector2& p_hotspot);

	virtual void set_mouse_show(bool p_show);
	virtual void set_mouse_grab(bool p_grab);
	virtual bool is_mouse_grab_enabled() const;
	virtual Point2 get_mouse_pos() const;
	virtual int get_mouse_button_state() const;
	virtual void set_window_title(const String& p_title);

	virtual MainLoop* get_main_loop() const;

	virtual bool can_draw() const;

	virtual void set_video_mode(const VideoMode& p_video_mode, int p_screen = 0);
	virtual VideoMode get_video_mode(int p_screen = 0) const;
	virtual void get_fullscreen_mode_list(List<VideoMode>* p_list, int p_screen = 0) const;

	virtual Size2 get_window_size() const;

	virtual void move_window_to_foreground();

	virtual int get_audio_driver_count() const;
	virtual const char* get_audio_driver_name(int p_driver) const;

	virtual void vprint(const char* p_format, va_list p_list, bool p_stderr = false);
	virtual void alert(const String&, const String&);

	virtual String get_stdin_string(bool p_block = true);
	virtual Error execute(const String& p_path, const List<String>& p_arguments, bool p_blocking, ProcessID* r_child_id = NULL, String* r_pipe = NULL, int* r_exitcode = NULL, bool read_stderr = false);
	virtual Error kill(const ProcessID& p_pid);

	virtual bool has_environment(const String& p_var) const;
	virtual String get_environment(const String& p_var) const;

	virtual Date get_date(bool) const;
	virtual Time get_time(bool local = false) const;
	virtual TimeZoneInfo get_time_zone_info() const;
	virtual void delay_usec(uint32_t) const;
	virtual uint64_t get_ticks_usec() const;

	void run();

	OS_PS2();

private:
	Rasterizer* rasterizer;
	VisualServer* visual_server;
	VideoMode current_videomode;
	List<String> args;
	MainLoop* main_loop;

	bool grab;

	AudioDriverDummy driver_dummy;

	PhysicsServer* physics_server;
	Physics2DServer* physics_2d_server;

	AudioServerSW* audio_server;
	SampleManagerMallocSW* sample_manager;
	SpatialSoundServerSW* spatial_sound_server;
	SpatialSound2DServerSW* spatial_sound_2d_server;

	bool force_quit;

	InputDefault* input;

	virtual void delete_main_loop();

	uint32_t ticks_start;
};

#endif