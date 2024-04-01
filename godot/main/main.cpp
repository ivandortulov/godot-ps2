/*************************************************************************/
/*  main.cpp                                                             */
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
#include "main.h"
#include "core/register_core_types.h"
#include "drivers/register_driver_types.h"
#include "globals.h"
#include "input_map.h"
#include "io/resource_loader.h"
#include "message_queue.h"
#include "modules/register_module_types.h"
#include "os/os.h"
#include "path_remap.h"
#include "scene/main/scene_main_loop.h"
#include "scene/register_scene_types.h"
#include "script_debugger_local.h"
#include "script_debugger_remote.h"
#include "servers/register_server_types.h"
#include "splash.h"

#include "io/resource_loader.h"
#include "script_language.h"

#include "core/io/ip.h"
#include "main/tests/test_main.h"
#include "os/dir_access.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#include "editor/doc/doc_data.h"
#include "editor/editor_node.h"
#include "editor/project_manager.h"
#endif

#include "io/file_access_network.h"

#include "servers/physics_2d_server.h"
#include "servers/spatial_sound_2d_server.h"
#include "servers/spatial_sound_server.h"

#include "core/io/file_access_pack.h"
#include "core/io/file_access_zip.h"
#include "core/io/stream_peer_ssl.h"
#include "core/io/stream_peer_tcp.h"
#include "main/input_default.h"
#include "performance.h"
#include "translation.h"
#include "version.h"

static Globals *globals = NULL;
static InputMap *input_map = NULL;
static bool _start_success = false;
static ScriptDebugger *script_debugger = NULL;

static MessageQueue *message_queue = NULL;
static Performance *performance = NULL;
static PathRemap *path_remap;
static PackedData *packed_data = NULL;
#ifdef MINIZIP_ENABLED
static ZipArchive *zip_packed_data = NULL;
#endif
static FileAccessNetworkClient *file_access_network_client = NULL;
static TranslationServer *translation_server = NULL;

static OS::VideoMode video_mode;
static bool init_maximized = false;
static bool init_windowed = false;
static bool init_fullscreen = false;
static bool init_always_on_top = false;
static bool init_use_custom_pos = false;
#ifdef DEBUG_ENABLED
static bool debug_collisions = false;
static bool debug_navigation = false;
#endif
static int frame_delay = 0;
static Vector2 init_custom_pos;
static int video_driver_idx = -1;
static int audio_driver_idx = -1;
static String locale;
static bool use_debug_profiler = false;
static bool force_lowdpi = false;
static int init_screen = -1;
static bool use_vsync = true;
static bool editor = false;

static OS::ProcessID allow_focus_steal_pid = 0;

static String unescape_cmdline(const String &p_str) {

	return p_str.replace("%20", " ");
}

//#define DEBUG_INIT
#ifdef DEBUG_INIT
#define MAIN_PRINT(m_txt) print_line(m_txt)
#else
#define MAIN_PRINT(m_txt)
#endif

void Main::print_help(const char *p_binary) {

	OS::get_singleton()->print(VERSION_FULL_NAME " - https://godotengine.org\n");
	OS::get_singleton()->print("(c) 2007-2019 Juan Linietsky, Ariel Manzur.\n");
	OS::get_singleton()->print("(c) 2014-2019 Godot Engine contributors.\n");
	OS::get_singleton()->print("\n");
	OS::get_singleton()->print("Usage: %s [options] [path to scene or 'engine.cfg' file]\n", p_binary);
	OS::get_singleton()->print("\n");
	OS::get_singleton()->print("Options:\n");
	OS::get_singleton()->print("\t-h, -help : Print these usage instructions.\n");
	OS::get_singleton()->print("\t-path <dir> : Path to a game, containing engine.cfg.\n");
#ifdef TOOLS_ENABLED
	OS::get_singleton()->print("\t-e, -editor : Bring up the editor instead of running the scene.\n");
#endif
	OS::get_singleton()->print("\t-test <test> : Run a test (");
	const char **test_names = tests_get_names();
	const char *coma = "";
	while (*test_names) {

		OS::get_singleton()->print("%s%s", coma, *test_names);
		test_names++;
		coma = ", ";
	}
	OS::get_singleton()->print(").\n");

	OS::get_singleton()->print("\t-r <width>x<height> : Request window resolution.\n");
	OS::get_singleton()->print("\t-p <X>x<Y> : Request window position.\n");
	OS::get_singleton()->print("\t-f : Request fullscreen.\n");
	OS::get_singleton()->print("\t-mx : Request maximized.\n");
	OS::get_singleton()->print("\t-w : Request windowed.\n");
	OS::get_singleton()->print("\t-t : Request always-on-top.\n");
	OS::get_singleton()->print("\t-vd <driver> : Video driver (");
	for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {

		if (i != 0)
			OS::get_singleton()->print(", ");
		OS::get_singleton()->print("%s", OS::get_singleton()->get_video_driver_name(i));
	}
	OS::get_singleton()->print(").\n");
	OS::get_singleton()->print("\t-ldpi : Force low-dpi mode (OSX only).\n");

	OS::get_singleton()->print("\t-ad <driver> : Audio driver (");
	for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {

		if (i != 0)
			OS::get_singleton()->print(", ");
		OS::get_singleton()->print("%s", OS::get_singleton()->get_audio_driver_name(i));
	}
	OS::get_singleton()->print(").\n");
	OS::get_singleton()->print("\t-rthread <mode> : Render thread mode ('unsafe', 'safe', 'separate').\n");
	OS::get_singleton()->print("\t-s, -script <script> : Run a script.\n");
	OS::get_singleton()->print("\t-d, -debug : Debug (local stdout debugger).\n");
	OS::get_singleton()->print("\t-rdebug <address> : Remote debug (<ip>:<port> host address).\n");
	OS::get_singleton()->print("\t-fdelay <msec> : Simulate high CPU load (delay each frame by [msec]).\n");
	OS::get_singleton()->print("\t-timescale <msec> : Define custom timescale (time between frames in [msec]).\n");
	OS::get_singleton()->print("\t-bp : Breakpoint list as source::line comma separated pairs, no spaces (%%20 instead).\n");
	OS::get_singleton()->print("\t-v : Verbose stdout mode.\n");
	OS::get_singleton()->print("\t-lang <locale> : Use a specific locale.\n");
	OS::get_singleton()->print("\t-rfs <host/ip>[:<port>] : Remote filesystem.\n");
	OS::get_singleton()->print("\t-rfs_pass <password> : Password for remote filesystem.\n");
	OS::get_singleton()->print("\t-dch : Disable crash handler when supported by the platform code.\n");
#ifdef TOOLS_ENABLED
	OS::get_singleton()->print("\t-doctool <file> : Dump the whole engine api to <file> in XML format. If <file> exists, it will be merged.\n");
	OS::get_singleton()->print("\t-nodocbase : Disallow dump the base types (used with -doctool).\n");
	OS::get_singleton()->print("\t-optimize <file> : Save an optimized copy of scene to <file>.\n");
	OS::get_singleton()->print("\t-optimize_preset <preset> : Use a given preset for optimization.\n");
	OS::get_singleton()->print("\t-export <target> : Export the project using given export target.\n");
	OS::get_singleton()->print("\t-export_debug : Use together with -export, enables debug mode for the template.\n");
#endif
}

Error Main::setup(const char *execpath, int argc, char *argv[], bool p_second_phase) {

	RID_OwnerBase::init_rid();

	OS::get_singleton()->initialize_core();
	ObjectTypeDB::init();

	MAIN_PRINT("Main: Initialize CORE");

	register_core_types();
	register_core_driver_types();

	MAIN_PRINT("Main: Initialize Globals");

	Thread::_main_thread_id = Thread::get_caller_ID();

	globals = memnew(Globals);
	input_map = memnew(InputMap);

	path_remap = memnew(PathRemap);
	translation_server = memnew(TranslationServer);
	performance = memnew(Performance);
	globals->add_singleton(Globals::Singleton("Performance", performance));

	GLOBAL_DEF("application/crash_handler_message", String("Please include this when reporting the bug on https://github.com/godotengine/godot/issues"));

	MAIN_PRINT("Main: Parse CMDLine");

	/* argument parsing and main creation */
	List<String> args;
	List<String> main_args;

	for (int i = 0; i < argc; i++) {

		args.push_back(String::utf8(argv[i]));
	}

	List<String>::Element *I = args.front();

	I = args.front();

	while (I) {

		I->get() = unescape_cmdline(I->get().strip_escapes());
		//		print_line("CMD: "+I->get());
		I = I->next();
	}

	I = args.front();

	video_mode = OS::get_singleton()->get_default_video_mode();

	String video_driver = "";
	String audio_driver = "";
	String game_path = ".";
	String debug_mode;
	String debug_host;
	String main_pack;
	bool quiet_stdout = false;
	int rtm = -1;

	String remotefs;
	String remotefs_pass;

	String screen = "";

	List<String> pack_list;
	Vector<String> breakpoints;
	bool use_custom_res = true;
	bool force_res = false;

	I = args.front();

	packed_data = PackedData::get_singleton();
	if (!packed_data)
		packed_data = memnew(PackedData);

#ifdef MINIZIP_ENABLED

	//XXX: always get_singleton() == 0x0
	zip_packed_data = ZipArchive::get_singleton();
	//TODO: remove this temporary fix
	if (!zip_packed_data) {
		zip_packed_data = memnew(ZipArchive);
	}

	packed_data->add_pack_source(zip_packed_data);
#endif

	while (I) {

		List<String>::Element *N = I->next();

		if (I->get() == "-noop") {

			// no op
		} else if (I->get() == "-h" || I->get() == "-help" || I->get() == "--help" || I->get() == "/?") { // resolution

			goto error;

		} else if (I->get() == "-r") { // resolution

			if (I->next()) {

				String vm = I->next()->get();

				if (vm.find("x") == -1) { // invalid parameter format

					OS::get_singleton()->print("Invalid -r argument: %s\n", vm.utf8().get_data());
					goto error;
				}

				int w = vm.get_slice("x", 0).to_int();
				int h = vm.get_slice("x", 1).to_int();

				if (w == 0 || h == 0) {

					OS::get_singleton()->print("Invalid -r resolution, x and y must be >0\n");
					goto error;
				}

				video_mode.width = w;
				video_mode.height = h;
				force_res = true;

				N = I->next()->next();
			} else {
				OS::get_singleton()->print("Invalid -p argument, needs resolution\n");
				goto error;
			}
		} else if (I->get() == "-p") { // position

			if (I->next()) {

				String vm = I->next()->get();

				if (vm.find("x") == -1) { // invalid parameter format

					OS::get_singleton()->print("Invalid -p argument: %s\n", vm.utf8().get_data());
					goto error;
				}

				int x = vm.get_slice("x", 0).to_int();
				int y = vm.get_slice("x", 1).to_int();

				init_custom_pos = Point2(x, y);
				init_use_custom_pos = true;

				N = I->next()->next();
			} else {
				OS::get_singleton()->print("Invalid -r argument, needs position\n");
				goto error;
			}

		} else if (I->get() == "-mx") { // video driver

			init_maximized = true;
		} else if (I->get() == "-w") { // video driver

			init_windowed = true;
		} else if (I->get() == "-profile") { // video driver

			use_debug_profiler = true;
		} else if (I->get() == "-vd") { // video driver

			if (I->next()) {

				video_driver = I->next()->get();
				N = I->next()->next();
			} else {
				OS::get_singleton()->print("Invalid -cd argument, needs driver name\n");
				goto error;
			}
		} else if (I->get() == "-lang") { // language

			if (I->next()) {

				locale = I->next()->get();
				N = I->next()->next();
			} else {
				OS::get_singleton()->print("Invalid -lang argument, needs language code\n");
				goto error;
			}
		} else if (I->get() == "-ldpi") { // language

			force_lowdpi = true;
		} else if (I->get() == "-rfs") { // language

			if (I->next()) {

				remotefs = I->next()->get();
				N = I->next()->next();
			} else {
				goto error;
			}
		} else if (I->get() == "-rfs_pass") { // language

			if (I->next()) {

				remotefs_pass = I->next()->get();
				N = I->next()->next();
			} else {
				goto error;
			}
		} else if (I->get() == "-rthread") { // language

			if (I->next()) {

				if (I->next()->get() == "safe")
					rtm = OS::RENDER_THREAD_SAFE;
				else if (I->next()->get() == "unsafe")
					rtm = OS::RENDER_THREAD_UNSAFE;
				else if (I->next()->get() == "separate")
					rtm = OS::RENDER_SEPARATE_THREAD;

				N = I->next()->next();
			} else {
				goto error;
			}

		} else if (I->get() == "-ad") { // video driver

			if (I->next()) {

				audio_driver = I->next()->get();
				N = I->next()->next();
			} else {
				goto error;
			}

		} else if (I->get() == "-f") { // fullscreen

			init_fullscreen = true;
		} else if (I->get() == "-t") { // always-on-top

			init_always_on_top = true;
		} else if (I->get() == "-e" || I->get() == "-editor") { // fonud editor

			editor = true;
		} else if (I->get() == "-nowindow") { // fullscreen

			OS::get_singleton()->set_no_window_mode(true);
		} else if (I->get() == "-quiet") { // fullscreen

			quiet_stdout = true;
		} else if (I->get() == "-v") { // fullscreen
			OS::get_singleton()->_verbose_stdout = true;
		} else if (I->get() == "-path") { // resolution

			if (I->next()) {

				String p = I->next()->get();
				if (OS::get_singleton()->set_cwd(p) == OK) {
					//nothing
				} else {
					game_path = I->next()->get(); //use game_path instead
				}
				N = I->next()->next();
			} else {
				goto error;
			}
		} else if (I->get() == "-bp") { // /breakpoints

			if (I->next()) {

				String bplist = I->next()->get();
				breakpoints = bplist.split(",");
				N = I->next()->next();
			} else {
				goto error;
			}

		} else if (I->get() == "-fdelay") { // resolution

			if (I->next()) {

				frame_delay = I->next()->get().to_int();
				N = I->next()->next();
			} else {
				goto error;
			}

		} else if (I->get() == "-timescale") { // resolution

			if (I->next()) {

				OS::get_singleton()->set_time_scale(I->next()->get().to_double());
				N = I->next()->next();
			} else {
				goto error;
			}

		} else if (I->get() == "-pack") {

			if (I->next()) {

				pack_list.push_back(I->next()->get());
				N = I->next()->next();
			} else {

				goto error;
			};

		} else if (I->get() == "-main_pack") {

			if (I->next()) {

				main_pack = I->next()->get();
				N = I->next()->next();
			} else {

				goto error;
			};

		} else if (I->get() == "-debug" || I->get() == "-d") {
			debug_mode = "local";
#ifdef DEBUG_ENABLED
		} else if (I->get() == "-debugcol" || I->get() == "-dc") {
			debug_collisions = true;
		} else if (I->get() == "-debugnav" || I->get() == "-dn") {
			debug_navigation = true;
#endif
		} else if (I->get() == "-editor_scene") {

			if (I->next()) {

				Globals::get_singleton()->set("editor_scene", game_path = I->next()->get());
			} else {
				goto error;
			}

		} else if (I->get() == "-rdebug") {
			if (I->next()) {

				debug_mode = "remote";
				debug_host = I->next()->get();
				if (debug_host.find(":") == -1) { //wrong host
					OS::get_singleton()->print("Invalid debug host string\n");
					goto error;
				}
				N = I->next()->next();
			} else {
				goto error;
			}
		} else if (I->get() == "-allow_focus_steal_pid") {
			if (I->next()) {

				allow_focus_steal_pid = I->next()->get().to_int64();
				N = I->next()->next();
			} else {
				goto error;
			}
		} else if (I->get() == "-dch") {
			OS::get_singleton()->disable_crash_handler();
		} else {

			//test for game path
			bool gpfound = false;

			if (!I->get().begins_with("-") && game_path == "") {
				DirAccess *da = DirAccess::open(I->get());
				if (da != NULL) {
					game_path = I->get();
					gpfound = true;
					memdelete(da);
				}
			}

			if (!gpfound) {
				main_args.push_back(I->get());
			}
		}

		I = N;
	}

	GLOBAL_DEF("debug/max_remote_stdout_chars_per_second", 2048);
	if (debug_mode == "remote") {

		ScriptDebuggerRemote *sdr = memnew(ScriptDebuggerRemote);
		uint16_t debug_port = 6096;
		if (debug_host.find(":") != -1) {
			int sep_pos = debug_host.find_last(":");
			debug_port = debug_host.substr(sep_pos + 1, debug_host.length()).to_int();
			debug_host = debug_host.substr(0, sep_pos);
		}
		Error derr = sdr->connect_to_host(debug_host, debug_port);

		if (derr != OK) {
			memdelete(sdr);
		} else {
			script_debugger = sdr;
		}
	} else if (debug_mode == "local") {

		script_debugger = memnew(ScriptDebuggerLocal);
	}

	if (remotefs != "") {

		file_access_network_client = memnew(FileAccessNetworkClient);
		int port;
		if (remotefs.find(":") != -1) {
			port = remotefs.get_slicec(':', 1).to_int();
			remotefs = remotefs.get_slicec(':', 0);
		} else {
			port = 6010;
		}

		Error err = file_access_network_client->connect(remotefs, port, remotefs_pass);
		if (err) {
			OS::get_singleton()->printerr("Could not connect to remotefs: %s:%i\n", remotefs.utf8().get_data(), port);
			goto error;
		}

		FileAccess::make_default<FileAccessNetwork>(FileAccess::ACCESS_RESOURCES);
	}
	if (script_debugger) {
		//there is a debugger, parse breakpoints

		for (int i = 0; i < breakpoints.size(); i++) {

			String bp = breakpoints[i];
			int sp = bp.find_last(":");
			if (sp == -1) {
				ERR_EXPLAIN("Invalid breakpoint: '" + bp + "', expected file:line format.");
				ERR_CONTINUE(sp == -1);
			}

			script_debugger->insert_breakpoint(bp.substr(sp + 1, bp.length()).to_int(), bp.substr(0, sp));
		}
	}

#ifdef TOOLS_ENABLED
	if (editor) {
		packed_data->set_disabled(true);
		globals->set_disable_platform_override(true);
		StreamPeerSSL::initialize_certs = false; //will be initialized by editor
	}

#endif

	if (globals->setup(game_path, main_pack) != OK) {

#ifdef TOOLS_ENABLED
		editor = false;
#else
		OS::get_singleton()->print("error: Couldn't load game path '%s'\n", game_path.ascii().get_data());

		goto error;
#endif
	}

	if (editor) {
		main_args.push_back("-editor");
		init_maximized = true;
		use_custom_res = false;
	}

	if (bool(Globals::get_singleton()->get("application/disable_stdout"))) {
		quiet_stdout = true;
	}
	if (bool(Globals::get_singleton()->get("application/disable_stderr"))) {
		_print_error_enabled = false;
	};

	if (quiet_stdout)
		_print_line_enabled = false;

	OS::get_singleton()->set_cmdline(execpath, main_args);

#ifdef TOOLS_ENABLED

	if (main_args.size() == 0 && (!Globals::get_singleton()->has("application/main_loop_type")) && (!Globals::get_singleton()->has("application/main_scene") || String(Globals::get_singleton()->get("application/main_scene")) == ""))
		use_custom_res = false; //project manager (run without arguments)

#endif

	if (editor)
		input_map->load_default(); //keys for editor
	else
		input_map->load_from_globals(); //keys for game

	if (video_driver == "") // specified in engine.cfg
		video_driver = _GLOBAL_DEF("display/driver", Variant((const char *)OS::get_singleton()->get_video_driver_name(0)));

	if (!force_res && use_custom_res && globals->has("display/width"))
		video_mode.width = globals->get("display/width");
	if (!force_res && use_custom_res && globals->has("display/height"))
		video_mode.height = globals->get("display/height");
	if (!editor && ((globals->has("display/allow_hidpi") && !globals->get("display/allow_hidpi")) || force_lowdpi)) {
		OS::get_singleton()->_allow_hidpi = false;
	}
	if (use_custom_res && globals->has("display/fullscreen"))
		video_mode.fullscreen = globals->get("display/fullscreen");
	if (use_custom_res && globals->has("display/resizable"))
		video_mode.resizable = globals->get("display/resizable");
	if (use_custom_res && globals->has("display/borderless_window"))
		video_mode.borderless_window = globals->get("display/borderless_window");
	if (use_custom_res && globals->has("display/always_on_top"))
		video_mode.always_on_top = globals->get("display/always_on_top");

	if (!force_res && use_custom_res && globals->has("display/test_width") && globals->has("display/test_height")) {
		int tw = globals->get("display/test_width");
		int th = globals->get("display/test_height");
		if (tw > 0 && th > 0) {
			video_mode.width = tw;
			video_mode.height = th;
		}
	}

	GLOBAL_DEF("display/width", video_mode.width);
	GLOBAL_DEF("display/height", video_mode.height);
	GLOBAL_DEF("display/allow_hidpi", false);
	GLOBAL_DEF("display/fullscreen", video_mode.fullscreen);
	GLOBAL_DEF("display/resizable", video_mode.resizable);
	GLOBAL_DEF("display/borderless_window", video_mode.borderless_window);
	GLOBAL_DEF("display/always_on_top", video_mode.always_on_top);
	use_vsync = GLOBAL_DEF("display/use_vsync", use_vsync);
	GLOBAL_DEF("display/test_width", 0);
	GLOBAL_DEF("display/test_height", 0);
	OS::get_singleton()->_pixel_snap = GLOBAL_DEF("display/use_2d_pixel_snap", false);
	OS::get_singleton()->_keep_screen_on = GLOBAL_DEF("display/keep_screen_on", true);
	if (rtm == -1) {
		rtm = GLOBAL_DEF("render/thread_model", OS::RENDER_THREAD_SAFE);
		if (rtm >= 1) //hack for now
			rtm = 1;
	}

	if (rtm >= 0 && rtm < 3) {
		if (editor) {
			rtm = OS::RENDER_THREAD_SAFE;
		}
		OS::get_singleton()->_render_thread_mode = OS::RenderThreadMode(rtm);
	}

	/* Determine Video Driver */

	if (audio_driver == "") { // specified in engine.cfg
		audio_driver = GLOBAL_DEF("audio/driver", OS::get_singleton()->get_audio_driver_name(0));
	}

	for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {

		if (video_driver == OS::get_singleton()->get_video_driver_name(i)) {

			video_driver_idx = i;
			break;
		}
	}

	if (video_driver_idx < 0) {

		OS::get_singleton()->alert("Invalid Video Driver: " + video_driver);
		video_driver_idx = 0;
		//goto error;
	}

	for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {

		if (audio_driver == OS::get_singleton()->get_audio_driver_name(i)) {

			audio_driver_idx = i;
			break;
		}
	}

	if (audio_driver_idx < 0) {

		OS::get_singleton()->alert("Invalid Audio Driver: " + audio_driver);
		audio_driver_idx = 0;
		//goto error;
	}

	{
		String orientation = GLOBAL_DEF("display/orientation", "landscape");

		if (orientation == "portrait")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_PORTRAIT);
		else if (orientation == "reverse_landscape")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_REVERSE_LANDSCAPE);
		else if (orientation == "reverse_portrait")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_REVERSE_PORTRAIT);
		else if (orientation == "sensor_landscape")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR_LANDSCAPE);
		else if (orientation == "sensor_portrait")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR_PORTRAIT);
		else if (orientation == "sensor")
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR);
		else
			OS::get_singleton()->set_screen_orientation(OS::SCREEN_LANDSCAPE);
	}

	OS::get_singleton()->set_iterations_per_second(GLOBAL_DEF("physics/fixed_fps", 60));
	OS::get_singleton()->set_target_fps(GLOBAL_DEF("debug/force_fps", 0));

	if (!OS::get_singleton()->_verbose_stdout) //overrided
		OS::get_singleton()->_verbose_stdout = GLOBAL_DEF("debug/verbose_stdout", false);

	if (frame_delay == 0) {
		frame_delay = GLOBAL_DEF("application/frame_delay_msec", 0);
	}

	OS::get_singleton()->set_frame_delay(frame_delay);

	message_queue = memnew(MessageQueue);

	Globals::get_singleton()->register_global_defaults();

	if (p_second_phase)
		return setup2();

	return OK;

error:

	video_driver = "";
	audio_driver = "";
	game_path = "";

	args.clear();
	main_args.clear();

	print_help(execpath);

	if (performance)
		memdelete(performance);
	if (input_map)
		memdelete(input_map);
	if (translation_server)
		memdelete(translation_server);
	if (globals)
		memdelete(globals);
	if (script_debugger)
		memdelete(script_debugger);
	if (packed_data)
		memdelete(packed_data);
	if (file_access_network_client)
		memdelete(file_access_network_client);
	if (path_remap)
		memdelete(path_remap);

	// Note 1: *zip_packed_data live into *packed_data
	// Note 2: PackedData::~PackedData destroy this.
	//#ifdef MINIZIP_ENABLED
	//	if (zip_packed_data)
	//		memdelete( zip_packed_data );
	//#endif

	unregister_core_driver_types();
	unregister_core_types();

	OS::get_singleton()->_cmdline.clear();

	if (message_queue)
		memdelete(message_queue);
	OS::get_singleton()->finalize_core();
	locale = String();

	return ERR_INVALID_PARAMETER;
}

Error Main::setup2(Thread::ID p_main_tid_override) {

	if (p_main_tid_override) {
		Thread::_main_thread_id = p_main_tid_override;
	}

	OS::get_singleton()->initialize(video_mode, video_driver_idx, audio_driver_idx);
	if (init_use_custom_pos) {
		OS::get_singleton()->set_window_position(init_custom_pos);
	}

	OS::get_singleton()->set_use_vsync(use_vsync);

	register_core_singletons();

	MAIN_PRINT("Main: Setup Logo");

	bool show_logo = true;
#ifdef JAVASCRIPT_ENABLED
	show_logo = false;
#endif

	if (init_screen != -1) {
		OS::get_singleton()->set_current_screen(init_screen);
	}
	if (init_windowed) {
		//do none..
	} else if (init_maximized) {
		OS::get_singleton()->set_window_maximized(true);
	} else if (init_fullscreen) {
		OS::get_singleton()->set_window_fullscreen(true);
	}
	if (init_always_on_top) {
		OS::get_singleton()->set_window_always_on_top(true);
	}
	MAIN_PRINT("Main: Load Remaps");

	path_remap->load_remaps();

	if (show_logo) { //boot logo!
		String boot_logo_path = GLOBAL_DEF("application/boot_splash", String());
		bool boot_logo_scale = GLOBAL_DEF("application/boot_splash_fullsize", true);
		Globals::get_singleton()->set_custom_property_info("application/boot_splash", PropertyInfo(Variant::STRING, "application/boot_splash", PROPERTY_HINT_FILE, "*.png"));

		Image boot_logo;

		boot_logo_path = boot_logo_path.strip_edges();

		if (boot_logo_path != String() /*&& FileAccess::exists(boot_logo_path)*/) {
			print_line("Boot splash path: " + boot_logo_path);
			Error err = boot_logo.load(boot_logo_path);
			if (err)
				ERR_PRINTS("Non-existing or invalid boot splash at: " + boot_logo_path + ". Loading default splash.");
		}

		if (!boot_logo.empty()) {
			OS::get_singleton()->_msec_splash = OS::get_singleton()->get_ticks_msec();
			Color clear = GLOBAL_DEF("render/default_clear_color", Color(0.3, 0.3, 0.3));
			VisualServer::get_singleton()->set_default_clear_color(clear);
			Color boot_bg = GLOBAL_DEF("application/boot_bg_color", clear);
			VisualServer::get_singleton()->set_boot_image(boot_logo, boot_bg, boot_logo_scale);
#ifndef TOOLS_ENABLED
//no tools, so free the boot logo (no longer needed)
//	Globals::get_singleton()->set("application/boot_logo",Image());
#endif

		} else {
#ifndef NO_DEFAULT_BOOT_LOGO

			MAIN_PRINT("Main: Create bootsplash");
			Image splash(boot_splash_png);

			MAIN_PRINT("Main: ClearColor");
			VisualServer::get_singleton()->set_default_clear_color(boot_splash_bg_color);
			MAIN_PRINT("Main: Image");
			VisualServer::get_singleton()->set_boot_image(splash, boot_splash_bg_color, false);
#endif
		}

		Image icon(app_icon_png);
		OS::get_singleton()->set_icon(icon);
	}

	MAIN_PRINT("Main: DCC");
	VisualServer::get_singleton()->set_default_clear_color(GLOBAL_DEF("render/default_clear_color", Color(0.3, 0.3, 0.3)));
	MAIN_PRINT("Main: END");

	GLOBAL_DEF("application/icon", String());
	Globals::get_singleton()->set_custom_property_info("application/icon", PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.png,*.webp"));

	if (bool(GLOBAL_DEF("display/emulate_touchscreen", false))) {
		if (!OS::get_singleton()->has_touchscreen_ui_hint() && Input::get_singleton() && !editor) {
			//only if no touchscreen ui hint, set emulation
			InputDefault *id = Input::get_singleton()->cast_to<InputDefault>();
			if (id)
				id->set_emulate_touch(true);
		}
	}

	MAIN_PRINT("Main: Load Remaps");

	MAIN_PRINT("Main: Load Scene Types");

	register_scene_types();
	register_server_types();

#ifdef TOOLS_ENABLED
	EditorNode::register_editor_types();
#endif

	if (allow_focus_steal_pid) {
		OS::get_singleton()->enable_for_stealing_focus(allow_focus_steal_pid);
	}

	MAIN_PRINT("Main: Load Scripts, Modules, Drivers");

	register_module_types();
	register_driver_types();

	ScriptServer::init_languages();

	MAIN_PRINT("Main: Load Translations");

	translation_server->setup(); //register translations, load them, etc.
	if (locale != "") {

		translation_server->set_locale(locale);
	}
	translation_server->load_translations();

	if (use_debug_profiler && script_debugger) {
		script_debugger->profiling_start();
	}
	_start_success = true;
	locale = String();

	MAIN_PRINT("Main: Done");

	return OK;
}

bool Main::start() {

	ERR_FAIL_COND_V(!_start_success, false);

	bool editor = false;
	String doc_tool;
	bool doc_base = true;
	String game_path;
	String script;
	String test;
	String screen;
	String optimize;
	String optimize_preset;
	String _export_platform;
	String _import;
	String _import_script;
	String dumpstrings;
	bool noquit = false;
	bool export_debug = false;
	bool project_manager_request = false;
	List<String> args = OS::get_singleton()->get_cmdline_args();
	for (int i = 0; i < args.size(); i++) {
		//parameters that do not have an argument to the right
		if (args[i] == "-nodocbase") {
			doc_base = false;
		} else if (args[i] == "-noquit") {
			noquit = true;
		} else if (args[i] == "-editor" || args[i] == "-e") {
			editor = true;
		} else if (args[i] == "-pm" || args[i] == "-project_manager") {
			project_manager_request = true;
		} else if (args[i].length() && args[i][0] != '-' && game_path == "") {
			game_path = args[i];
		}
		//parameters that have an argument to the right
		else if (i < (args.size() - 1)) {
			bool parsed_pair = true;
			if (args[i] == "-doctool") {
				doc_tool = args[i + 1];
			} else if (args[i] == "-script" || args[i] == "-s") {
				script = args[i + 1];
			} else if (args[i] == "-level" || args[i] == "-l") {
				OS::get_singleton()->_custom_level = args[i + 1];
			} else if (args[i] == "-test") {
				test = args[i + 1];
			} else if (args[i] == "-optimize") {
				optimize = args[i + 1];
			} else if (args[i] == "-optimize_preset") {
				optimize_preset = args[i + 1];
			} else if (args[i] == "-export") {
				editor = true; //needs editor
				_export_platform = args[i + 1];
			} else if (args[i] == "-export_debug") {
				editor = true; //needs editor
				_export_platform = args[i + 1];
				export_debug = true;
			} else if (args[i] == "-import") {
				editor = true; //needs editor
				_import = args[i + 1];
			} else if (args[i] == "-import_script") {
				editor = true; //needs editor
				_import_script = args[i + 1];
			} else if (args[i] == "-dumpstrings") {
				editor = true; //needs editor
				dumpstrings = args[i + 1];
			} else {
				// The parameter does not match anything known, don't skip the next argument
				parsed_pair = false;
			}
			if (parsed_pair) {
				i++;
			}
		}
	}

	if (editor)
		Globals::get_singleton()->set("editor_active", true);

	String main_loop_type;
#ifdef TOOLS_ENABLED
	if (doc_tool != "") {

		DocData doc;
		doc.generate(doc_base);

		DocData docsrc;
		if (docsrc.load(doc_tool) == OK) {
			print_line("Doc exists. Merging..");
			doc.merge_from(docsrc);
		} else {
			print_line("No Doc exists. Generating empty.");
		}

		doc.save(doc_tool);

		return false;
	}

	if (optimize != "")
		editor = true; //need editor

#endif

	if (_export_platform != "") {
		if (game_path == "") {
			String err = "Command line param ";
			err += export_debug ? "-export_debug" : "-export";
			err += " passed but no destination path given.\n";
			err += "Please specify the binary's file path to export to. Aborting export.";
			ERR_PRINT(err.utf8().get_data());
			return false;
		}
	}

	if (script == "" && game_path == "" && String(GLOBAL_DEF("application/main_scene", "")) != "") {
		game_path = GLOBAL_DEF("application/main_scene", "");
	}

	MainLoop *main_loop = NULL;
	if (editor) {
		main_loop = memnew(SceneTree);
	};

	if (test != "") {
#ifdef DEBUG_ENABLED
		main_loop = test_main(test, args);

		if (!main_loop)
			return false;

#endif

	} else if (script != "") {

		Ref<Script> script_res = ResourceLoader::load(script);
		ERR_EXPLAIN("Can't load script: " + script);
		ERR_FAIL_COND_V(script_res.is_null(), false);

		if (script_res->can_instance() /*&& script_res->inherits_from("SceneTreeScripted")*/) {

			StringName instance_type = script_res->get_instance_base_type();
			Object *obj = ObjectTypeDB::instance(instance_type);
			MainLoop *script_loop = obj ? obj->cast_to<MainLoop>() : NULL;
			if (!script_loop) {
				if (obj)
					memdelete(obj);
				ERR_EXPLAIN("Can't load script '" + script + "', it does not inherit from a MainLoop type");
				ERR_FAIL_COND_V(!script_loop, false);
			}

			script_loop->set_init_script(script_res);
			main_loop = script_loop;
		} else {

			return false;
		}

	} else {
		main_loop_type = GLOBAL_DEF("application/main_loop_type", "");
	}

	if (!main_loop && main_loop_type == "")
		main_loop_type = "SceneTree";

	if (!main_loop) {
		if (!ObjectTypeDB::type_exists(main_loop_type)) {
			OS::get_singleton()->alert("godot: error: MainLoop type doesn't exist: " + main_loop_type);
			return false;
		} else {

			Object *ml = ObjectTypeDB::instance(main_loop_type);
			if (!ml) {
				ERR_EXPLAIN("Can't instance MainLoop type");
				ERR_FAIL_V(false);
			}

			main_loop = ml->cast_to<MainLoop>();
			if (!main_loop) {

				memdelete(ml);
				ERR_EXPLAIN("Invalid MainLoop type");
				ERR_FAIL_V(false);
			}
		}
	}

	if (main_loop->is_type("SceneTree")) {

		SceneTree *sml = main_loop->cast_to<SceneTree>();

#ifdef DEBUG_ENABLED
		if (debug_collisions) {
			sml->set_debug_collisions_hint(true);
		}
		if (debug_navigation) {
			sml->set_debug_navigation_hint(true);
		}
#endif

#ifdef TOOLS_ENABLED

		EditorNode *editor_node = NULL;
		if (editor) {

			editor_node = memnew(EditorNode);
			sml->get_root()->add_child(editor_node);

			//root_node->set_editor(editor);
			//startup editor

			if (_export_platform != "") {

				editor_node->export_platform(_export_platform, game_path, export_debug, "", true);
				game_path = ""; //no load anything
			}
		}
#endif

		if (!editor) {
			//standard helpers that can be changed from main config

			String stretch_mode = GLOBAL_DEF("display/stretch_mode", "disabled");
			String stretch_aspect = GLOBAL_DEF("display/stretch_aspect", "ignore");
			Size2i stretch_size = Size2(GLOBAL_DEF("display/width", 0), GLOBAL_DEF("display/height", 0));

			SceneTree::StretchMode sml_sm = SceneTree::STRETCH_MODE_DISABLED;
			if (stretch_mode == "2d")
				sml_sm = SceneTree::STRETCH_MODE_2D;
			else if (stretch_mode == "viewport")
				sml_sm = SceneTree::STRETCH_MODE_VIEWPORT;

			SceneTree::StretchAspect sml_aspect = SceneTree::STRETCH_ASPECT_IGNORE;
			if (stretch_aspect == "keep")
				sml_aspect = SceneTree::STRETCH_ASPECT_KEEP;
			else if (stretch_aspect == "keep_width")
				sml_aspect = SceneTree::STRETCH_ASPECT_KEEP_WIDTH;
			else if (stretch_aspect == "keep_height")
				sml_aspect = SceneTree::STRETCH_ASPECT_KEEP_HEIGHT;
			else if (stretch_aspect == "expand")
				sml_aspect = SceneTree::STRETCH_ASPECT_EXPAND;

			sml->set_screen_stretch(sml_sm, sml_aspect, stretch_size);

			sml->set_auto_accept_quit(GLOBAL_DEF("application/auto_accept_quit", true));
			String appname = Globals::get_singleton()->get("application/name");
			appname = TranslationServer::get_singleton()->translate(appname);
			OS::get_singleton()->set_window_title(appname);

		} else {
			GLOBAL_DEF("display/stretch_mode", "disabled");
			Globals::get_singleton()->set_custom_property_info("display/stretch_mode", PropertyInfo(Variant::STRING, "display/stretch_mode", PROPERTY_HINT_ENUM, "disabled,2d,viewport"));
			GLOBAL_DEF("display/stretch_aspect", "ignore");
			Globals::get_singleton()->set_custom_property_info("display/stretch_aspect", PropertyInfo(Variant::STRING, "display/stretch_aspect", PROPERTY_HINT_ENUM, "ignore,keep,keep_width,keep_height,expand"));
			sml->set_auto_accept_quit(GLOBAL_DEF("application/auto_accept_quit", true));
		}

		if (game_path != "" && !project_manager_request) {

			String local_game_path = game_path.replace("\\", "/");

			if (!local_game_path.begins_with("res://")) {
				bool absolute = (local_game_path.size() > 1) && (local_game_path[0] == '/' || local_game_path[1] == ':');

				if (!absolute) {

					if (Globals::get_singleton()->is_using_datapack()) {

						local_game_path = "res://" + local_game_path;

					} else {
						int sep = local_game_path.find_last("/");

						if (sep == -1) {
							DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
							local_game_path = da->get_current_dir() + "/" + local_game_path;
							memdelete(da);
						} else {

							DirAccess *da = DirAccess::open(local_game_path.substr(0, sep));
							if (da) {
								local_game_path = da->get_current_dir() + "/" + local_game_path.substr(sep + 1, local_game_path.length());
								memdelete(da);
							}
						}
					}
				}
			}

			local_game_path = Globals::get_singleton()->localize_path(local_game_path);

#ifdef TOOLS_ENABLED
			if (editor) {

				if (_import != "") {

					//editor_node->import_scene(_import,local_game_path,_import_script);
					if (!noquit)
						sml->quit();
					game_path = ""; //no load anything
				} else {

					Error serr = editor_node->load_scene(local_game_path);

					if (serr == OK) {

						if (optimize != "") {

							editor_node->save_optimized_copy(optimize, optimize_preset);
							if (!noquit)
								sml->quit();
						}

						if (dumpstrings != "") {

							editor_node->save_translatable_strings(dumpstrings);
							if (!noquit)
								sml->quit();
						}
					}
				}
				OS::get_singleton()->set_context(OS::CONTEXT_EDITOR);

				//editor_node->set_edited_scene(game);
			} else {
#endif

				{
					//autoload
					List<PropertyInfo> props;
					Globals::get_singleton()->get_property_list(&props);

					//first pass, add the constants so they exist before any script is loaded
					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {

						String s = E->get().name;
						if (!s.begins_with("autoload/"))
							continue;
						String name = s.get_slicec('/', 1);
						String path = Globals::get_singleton()->get(s);
						bool global_var = false;
						if (path.begins_with("*")) {
							global_var = true;
						}

						if (global_var) {
							for (int i = 0; i < ScriptServer::get_language_count(); i++) {
								ScriptServer::get_language(i)->add_global_constant(name, Variant());
							}
						}
					}

					//second pass, load into global constants
					List<Node *> to_add;
					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {

						String s = E->get().name;
						if (!s.begins_with("autoload/"))
							continue;
						String name = s.get_slicec('/', 1);
						String path = Globals::get_singleton()->get(s);
						bool global_var = false;
						if (path.begins_with("*")) {
							global_var = true;
							path = path.substr(1, path.length() - 1);
						}

						RES res = ResourceLoader::load(path);
						ERR_EXPLAIN("Can't autoload: " + path);
						ERR_CONTINUE(res.is_null());
						Node *n = NULL;
						if (res->is_type("PackedScene")) {
							Ref<PackedScene> ps = res;
							n = ps->instance();
						} else if (res->is_type("Script")) {
							Ref<Script> s = res;
							StringName ibt = s->get_instance_base_type();
							bool valid_type = ObjectTypeDB::is_type(ibt, "Node");
							ERR_EXPLAIN("Script does not inherit a Node: " + path);
							ERR_CONTINUE(!valid_type);

							Object *obj = ObjectTypeDB::instance(ibt);

							ERR_EXPLAIN("Cannot instance script for autoload, expected 'Node' inheritance, got: " + String(ibt));
							ERR_CONTINUE(obj == NULL);

							n = obj->cast_to<Node>();
							n->set_script(s.get_ref_ptr());
						}

						ERR_EXPLAIN("Path in autoload not a node or script: " + path);
						ERR_CONTINUE(!n);
						n->set_name(name);

						//defer so references are all valid on _ready()
						//sml->get_root()->add_child(n);
						to_add.push_back(n);

						if (global_var) {
							for (int i = 0; i < ScriptServer::get_language_count(); i++) {
								ScriptServer::get_language(i)->add_global_constant(name, n);
							}
						}
					}

					for (List<Node *>::Element *E = to_add.front(); E; E = E->next()) {

						sml->get_root()->add_child(E->get());
					}
				}

				Node *scene = NULL;
				Ref<PackedScene> scenedata = ResourceLoader::load(local_game_path);
				if (scenedata.is_valid())
					scene = scenedata->instance();

				ERR_EXPLAIN("Failed loading scene: " + local_game_path);
				ERR_FAIL_COND_V(!scene, false)
				//sml->get_root()->add_child(scene);
				sml->add_current_scene(scene);

				String iconpath = GLOBAL_DEF("application/icon", "Variant()");
				if (iconpath != "") {
					iconpath = PathRemap::get_singleton()->get_remap(iconpath);
					Image icon;
					if (icon.load(iconpath) == OK)
						OS::get_singleton()->set_icon(icon);
				}

//singletons
#ifdef TOOLS_ENABLED
			}
#endif
		}

#ifdef TOOLS_ENABLED

		/*if (_export_platform!="") {

			sml->quit();
		}*/

		/*
		if (sml->get_root_node()) {

			Console *console = memnew( Console );

			sml->get_root_node()->cast_to<RootNode>()->set_console(console);
			if (GLOBAL_DEF("console/visible_default",false).operator bool()) {

				console->show();
			} else {P

				console->hide();
			};
		}
*/
		if (project_manager_request || (script == "" && test == "" && game_path == "" && !editor)) {

			ProjectManager *pmanager = memnew(ProjectManager);
			ProgressDialog *progress_dialog = memnew(ProgressDialog);
			pmanager->add_child(progress_dialog);
			sml->get_root()->add_child(pmanager);
			OS::get_singleton()->set_context(OS::CONTEXT_PROJECTMAN);
		}

#endif
	}

	OS::get_singleton()->set_main_loop(main_loop);

	return true;
}

uint64_t Main::last_ticks = 0;
uint64_t Main::target_ticks = 0;
float Main::time_accum = 0;
uint32_t Main::frames = 0;
uint32_t Main::frame = 0;
bool Main::force_redraw_requested = false;

//for performance metrics
static uint64_t fixed_process_max = 0;
static uint64_t idle_process_max = 0;

bool Main::iteration() {

	uint64_t ticks = OS::get_singleton()->get_ticks_usec();
	uint64_t ticks_elapsed = ticks - last_ticks;

	double step = (double)ticks_elapsed / 1000000.0;
	float frame_slice = 1.0 / OS::get_singleton()->get_iterations_per_second();

	//	if (time_accum+step < frame_slice)
	//		return false;

	uint64_t fixed_process_ticks = 0;
	uint64_t idle_process_ticks = 0;

	frame += ticks_elapsed;

	last_ticks = ticks;

	if (step > frame_slice * 8)
		step = frame_slice * 8;

	time_accum += step;

	float time_scale = OS::get_singleton()->get_time_scale();

	bool exit = false;

	int iters = 0;

	while (time_accum > frame_slice) {

		uint64_t fixed_begin = OS::get_singleton()->get_ticks_usec();

		PhysicsServer::get_singleton()->sync();
		PhysicsServer::get_singleton()->flush_queries();

		Physics2DServer::get_singleton()->sync();
		Physics2DServer::get_singleton()->flush_queries();

		if (OS::get_singleton()->get_main_loop()->iteration(frame_slice * time_scale)) {
			exit = true;
			break;
		}

		message_queue->flush();

		PhysicsServer::get_singleton()->step(frame_slice * time_scale);

		Physics2DServer::get_singleton()->end_sync();
		Physics2DServer::get_singleton()->step(frame_slice * time_scale);

		time_accum -= frame_slice;
		message_queue->flush();
		//if (AudioServer::get_singleton())
		//	AudioServer::get_singleton()->update();

		fixed_process_ticks = MAX(fixed_process_ticks, OS::get_singleton()->get_ticks_usec() - fixed_begin); // keep the largest one for reference
		fixed_process_max = MAX(OS::get_singleton()->get_ticks_usec() - fixed_begin, fixed_process_max);
		iters++;
	}

	uint64_t idle_begin = OS::get_singleton()->get_ticks_usec();

	OS::get_singleton()->get_main_loop()->idle(step * time_scale);
	message_queue->flush();

	if (SpatialSoundServer::get_singleton())
		SpatialSoundServer::get_singleton()->update(step * time_scale);
	if (SpatialSound2DServer::get_singleton())
		SpatialSound2DServer::get_singleton()->update(step * time_scale);

	VisualServer::get_singleton()->sync(); //sync if still drawing from previous frames.

	if (OS::get_singleton()->can_draw()) {

		if ((!force_redraw_requested) && OS::get_singleton()->is_in_low_processor_usage_mode()) {
			if (VisualServer::get_singleton()->has_changed()) {
				VisualServer::get_singleton()->draw(); // flush visual commands
				OS::get_singleton()->frames_drawn++;
			}
		} else {
			VisualServer::get_singleton()->draw(); // flush visual commands
			OS::get_singleton()->frames_drawn++;
			force_redraw_requested = false;
		}
	}

	if (AudioServer::get_singleton())
		AudioServer::get_singleton()->update();

	idle_process_ticks = OS::get_singleton()->get_ticks_usec() - idle_begin;
	idle_process_max = MAX(idle_process_ticks, idle_process_max);
	uint64_t frame_time = OS::get_singleton()->get_ticks_usec() - ticks;

	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptServer::get_language(i)->frame();
	}

	if (script_debugger) {
		if (script_debugger->is_profiling()) {
			script_debugger->profiling_set_frame_times(USEC_TO_SEC(frame_time), USEC_TO_SEC(idle_process_ticks), USEC_TO_SEC(fixed_process_ticks), frame_slice);
		}
		script_debugger->idle_poll();
	}

	//	x11_delay_usec(10000);
	frames++;

	if (frame > 1000000) {

		if (GLOBAL_DEF("debug/print_fps", OS::get_singleton()->is_stdout_verbose())) {
			print_line("FPS: " + itos(frames));
		};

		OS::get_singleton()->_fps = frames;
		performance->set_process_time(USEC_TO_SEC(idle_process_max));
		performance->set_fixed_process_time(USEC_TO_SEC(fixed_process_max));
		idle_process_max = 0;
		fixed_process_max = 0;

		if (GLOBAL_DEF("debug/print_metrics", false)) {

			//PerformanceMetrics::print();
		};

		frame %= 1000000;
		frames = 0;
	}

	if (OS::get_singleton()->is_in_low_processor_usage_mode() || !OS::get_singleton()->can_draw())
		OS::get_singleton()->delay_usec(16600); //apply some delay to force idle time (results in about 60 FPS max)
	else {
		uint32_t frame_delay = OS::get_singleton()->get_frame_delay();
		if (frame_delay)
			OS::get_singleton()->delay_usec(OS::get_singleton()->get_frame_delay() * 1000);
	}

	int target_fps = OS::get_singleton()->get_target_fps();
	if (target_fps > 0) {
		uint64_t time_step = 1000000L / target_fps;
		target_ticks += time_step;
		uint64_t current_ticks = OS::get_singleton()->get_ticks_usec();
		if (current_ticks < target_ticks) OS::get_singleton()->delay_usec(target_ticks - current_ticks);
		current_ticks = OS::get_singleton()->get_ticks_usec();
		target_ticks = MIN(MAX(target_ticks, current_ticks - time_step), current_ticks + time_step);
	}

	return exit;
}

void Main::force_redraw() {

	force_redraw_requested = true;
};

void Main::cleanup() {

	ERR_FAIL_COND(!_start_success);

	if (script_debugger) {
		if (use_debug_profiler) {
			script_debugger->profiling_end();
		}

		memdelete(script_debugger);
	}

	OS::get_singleton()->delete_main_loop();

	OS::get_singleton()->_cmdline.clear();
	OS::get_singleton()->_execpath = "";
	OS::get_singleton()->_local_clipboard = "";

#ifdef TOOLS_ENABLED
	EditorNode::unregister_editor_types();
#endif

	unregister_driver_types();
	unregister_module_types();
	unregister_scene_types();
	unregister_server_types();

	OS::get_singleton()->finalize();

	if (packed_data)
		memdelete(packed_data);
	if (file_access_network_client)
		memdelete(file_access_network_client);
	if (performance)
		memdelete(performance);
	if (input_map)
		memdelete(input_map);
	if (translation_server)
		memdelete(translation_server);
	if (path_remap)
		memdelete(path_remap);
	if (globals)
		memdelete(globals);

	memdelete(message_queue);

	unregister_core_driver_types();
	unregister_core_types();

	//PerformanceMetrics::finish();
	OS::get_singleton()->clear_last_error();
	OS::get_singleton()->finalize_core();
}
