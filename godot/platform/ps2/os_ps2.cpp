#include "os_ps2.hpp"

#include "file_access_ps2.hpp"
#include "dir_access_ps2.hpp"

#include "main/main.h"

#include "core/os/thread.h"
#include "core/os/thread_dummy.h"
#include "core/os/file_access.h"
#include "core/os/dir_access.h"
#include "core/os/memory_pool_dynamic_static.h"

#include "drivers/unix/memory_pool_static_malloc.h"

#include "servers/visual/rasterizer_dummy.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/physics/physics_server_sw.h"
#include "servers/physics_2d/physics_2d_server_sw.h"

#include <time.h>

#include <tamtypes.h>
#include <sifrpc.h>
#include <debug.h>
#include <unistd.h>


static MemoryPoolStatic* mempool_static = NULL;
static MemoryPoolDynamic* mempool_dynamic = NULL;


OS_PS2::OS_PS2()
{
	AudioDriverManagerSW::add_driver(&driver_dummy);
}


int OS_PS2::get_video_driver_count() const
{
	return 1;
}

const char* OS_PS2::get_video_driver_name(int p_driver) const
{
	return "GS";
}

OS::VideoMode OS_PS2::get_default_video_mode() const
{
	return OS::VideoMode(640, 512, true);
}

void OS_PS2::initialize(const VideoMode& p_desired, int p_video_driver, int p_audio_driver)
{
	args = OS::get_singleton()->get_cmdline_args();
	current_videomode = p_desired;
	main_loop = NULL;

	ticks_start = get_ticks_usec();

	rasterizer = memnew(RasterizerDummy);

	visual_server = memnew(VisualServerRaster(rasterizer));

	AudioDriverManagerSW::get_driver(p_audio_driver)->set_singleton();

	if (AudioDriverManagerSW::get_driver(p_audio_driver)->init() != OK)
	{
		ERR_PRINT("Initializing audio failed.");
	}

	sample_manager = memnew(SampleManagerMallocSW);
	audio_server = memnew(AudioServerSW(sample_manager));
	audio_server->init();
	spatial_sound_server = memnew(SpatialSoundServerSW);
	spatial_sound_server->init();
	spatial_sound_2d_server = memnew(SpatialSound2DServerSW);
	spatial_sound_2d_server->init();

	ERR_FAIL_COND(!visual_server);

	visual_server->init();

	physics_server = memnew(PhysicsServerSW);
	physics_server->init();
	physics_2d_server = memnew(Physics2DServerSW);
	physics_2d_server->init();

	input = memnew(InputDefault);

	_ensure_data_dir();
}

void OS_PS2::initialize_core()
{
	ThreadDummy::make_default();
	SemaphoreDummy::make_default();
	MutexDummy::make_default();

	FileAccess::make_default<FileAccessPS2>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessPS2>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessPS2>(FileAccess::ACCESS_FILESYSTEM);

	DirAccess::make_default<DirAccessPS2>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessPS2>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessPS2>(DirAccess::ACCESS_FILESYSTEM);

	mempool_static = new MemoryPoolStaticMalloc;
	mempool_dynamic = memnew(MemoryPoolDynamicStatic);
}

void OS_PS2::finalize_core()
{

}

void OS_PS2::set_main_loop(MainLoop* p_main_loop)
{
	main_loop = p_main_loop;
	input->set_main_loop(p_main_loop);
}

void OS_PS2::delete_main_loop()
{
	if (main_loop)
	{
		memdelete(main_loop);
	}
	main_loop = NULL;
}

void OS_PS2::finalize()
{
	if (main_loop)
	{
		memdelete(main_loop);
	}
	main_loop = NULL;

	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

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

	memdelete(input);

	args.clear();
}

void OS_PS2::set_mouse_show(bool p_show)
{

}

void OS_PS2::set_mouse_grab(bool p_grab)
{
	grab = p_grab;
}

bool OS_PS2::is_mouse_grab_enabled() const
{
	return grab;
}

Point2 OS_PS2::get_mouse_pos() const
{
	return Point2(0, 0);
}

int OS_PS2::get_mouse_button_state() const
{
	return 0;
}

void OS_PS2::set_window_title(const String& p_title)
{

}

void OS_PS2::set_video_mode(const VideoMode& p_video_mode, int p_screen)
{

}

OS::VideoMode OS_PS2::get_video_mode(int p_screen) const
{
	return current_videomode;
}

void OS_PS2::get_fullscreen_mode_list(List<VideoMode>* p_list, int p_screen) const
{

}

Size2 OS_PS2::get_window_size() const
{
	return Vector2(current_videomode.width, current_videomode.height);
}

void OS_PS2::move_window_to_foreground()
{

}

String OS_PS2::get_name()
{
	return "PS2";
}

MainLoop* OS_PS2::get_main_loop() const
{
	return main_loop;
}

bool OS_PS2::can_draw() const
{
	return false;
}

void OS_PS2::set_cursor_shape(CursorShape p_shape)
{

}

void OS_PS2::set_custom_mouse_cursor(const RES& p_cursor, CursorShape p_shape, const Vector2& p_hotspot)
{

}

void OS_PS2::run()
{
	force_quit = false;

	if (!main_loop)
	{
		return;
	}

	main_loop->init();

	while (!force_quit)
	{
		if (Main::iteration() == true)
		{
			break;
		}
	};

	main_loop->finish();
}

int OS_PS2::get_audio_driver_count() const
{
	return AudioDriverManagerSW::get_driver_count();
}

const char* OS_PS2::get_audio_driver_name(int p_driver) const
{
	AudioDriverSW* driver = AudioDriverManagerSW::get_driver(p_driver);
	ERR_FAIL_COND_V(!driver, "");
	return AudioDriverManagerSW::get_driver(p_driver)->get_name();
}

void OS_PS2::vprint(const char* p_format, va_list p_list, bool p_stderr)
{
	vprintf(p_format, p_list);
	scr_vprintf(p_format, p_list);
}

void OS_PS2::alert(const String& p_alert, const String& p_title)
{

}

String OS_PS2::get_stdin_string(bool p_block)
{
	return "";
}

Error OS_PS2::execute(
	const String& p_path,
	const List<String>& p_arguments,
	bool p_blocking,
	ProcessID* r_child_id,
	String* r_pipe,
	int* r_exitcode,
	bool read_stderr)
{
	return FAILED;
}

Error OS_PS2::kill(const ProcessID& p_pid)
{
	return FAILED;
}

bool OS_PS2::has_environment(const String& p_var) const
{
	return false;
}

String OS_PS2::get_environment(const String& p_var) const
{
	return "";
}

OS::Date OS_PS2::get_date(bool local) const
{
	return OS::Date();
}

OS::Time OS_PS2::get_time(bool local) const
{
	return OS::Time();
}

OS::TimeZoneInfo OS_PS2::get_time_zone_info() const
{
	return OS::TimeZoneInfo();
}

void OS_PS2::delay_usec(uint32_t p_usec) const
{
	usleep(p_usec);
}

uint64_t OS_PS2::get_ticks_usec() const
{
	return clock();
}