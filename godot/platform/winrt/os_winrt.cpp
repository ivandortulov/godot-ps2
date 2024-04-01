/*************************************************************************/
/*  os_winrt.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
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
#include "main/main.h"
#include "os/memory_pool_dynamic_static.h"
#include "os_winrt.h"
#include "thread_winrt.h"

#include "servers/audio/audio_server_sw.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include "globals.h"
#include "io/marshalls.h"
#include "os/memory_pool_dynamic_prealloc.h"

#include "drivers/unix/ip_unix.h"
#include "platform/windows/packet_peer_udp_winsock.h"
#include "platform/windows/stream_peer_winsock.h"
#include "platform/windows/tcp_server_winsock.h"

#include <ppltasks.h>
#include <wrl.h>

using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Popups;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Microsoft::WRL;
using namespace Windows::UI::ViewManagement;
using namespace Windows::Devices::Input;
using namespace Windows::Devices::Sensors;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace concurrency;

int OSWinrt::get_video_driver_count() const {

	return 1;
}
const char *OSWinrt::get_video_driver_name(int p_driver) const {

	return "GLES2";
}

OS::VideoMode OSWinrt::get_default_video_mode() const {

	return video_mode;
}

Size2 OSWinrt::get_window_size() const {
	Size2 size;
	size.width = video_mode.width;
	size.height = video_mode.height;
	return size;
}

void OSWinrt::set_window_size(const Size2 p_size) {

	Windows::Foundation::Size new_size;
	new_size.Width = p_size.width;
	new_size.Height = p_size.height;

	ApplicationView ^ view = ApplicationView::GetForCurrentView();

	if (view->TryResizeView(new_size)) {

		video_mode.width = p_size.width;
		video_mode.height = p_size.height;
	}
}

void OSWinrt::set_window_fullscreen(bool p_enabled) {

	ApplicationView ^ view = ApplicationView::GetForCurrentView();

	video_mode.fullscreen = view->IsFullScreenMode;

	if (video_mode.fullscreen == p_enabled)
		return;

	if (p_enabled) {

		video_mode.fullscreen = view->TryEnterFullScreenMode();

	} else {

		view->ExitFullScreenMode();
		video_mode.fullscreen = false;
	}
}

bool OSWinrt::is_window_fullscreen() const {

	return ApplicationView::GetForCurrentView()->IsFullScreenMode;
}

void OSWinrt::set_keep_screen_on(bool p_enabled) {

	if (is_keep_screen_on() == p_enabled) return;

	if (p_enabled)
		display_request->RequestActive();
	else
		display_request->RequestRelease();

	OS::set_keep_screen_on(p_enabled);
}

int OSWinrt::get_audio_driver_count() const {

	return AudioDriverManagerSW::get_driver_count();
}
const char *OSWinrt::get_audio_driver_name(int p_driver) const {

	AudioDriverSW *driver = AudioDriverManagerSW::get_driver(p_driver);
	ERR_FAIL_COND_V(!driver, "");
	return AudioDriverManagerSW::get_driver(p_driver)->get_name();
}

static MemoryPoolStatic *mempool_static = NULL;
static MemoryPoolDynamic *mempool_dynamic = NULL;

void OSWinrt::initialize_core() {

	last_button_state = 0;

	//RedirectIOToConsole();

	ThreadWinrt::make_default();
	SemaphoreWindows::make_default();
	MutexWindows::make_default();

	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_FILESYSTEM);
	//FileAccessBufferedFA<FileAccessWindows>::make_default();
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_FILESYSTEM);

	//TCPServerWinsock::make_default();
	//StreamPeerWinsock::make_default();

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

	IP_Unix::make_default();

	cursor_shape = CURSOR_ARROW;
}

bool OSWinrt::can_draw() const {

	return !minimized;
};

void OSWinrt::set_gl_context(ContextEGL *p_context) {

	gl_context = p_context;
};

void OSWinrt::screen_size_changed() {

	gl_context->reset();
};

void OSWinrt::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {

	main_loop = NULL;
	outside = true;

	gl_context->initialize();
	VideoMode vm;
	vm.width = gl_context->get_window_width();
	vm.height = gl_context->get_window_height();
	vm.resizable = false;

	ApplicationView ^ view = ApplicationView::GetForCurrentView();
	vm.fullscreen = view->IsFullScreenMode;

	view->SetDesiredBoundsMode(ApplicationViewBoundsMode::UseVisible);
	view->PreferredLaunchWindowingMode = ApplicationViewWindowingMode::PreferredLaunchViewSize;

	if (p_desired.fullscreen != view->IsFullScreenMode) {
		if (p_desired.fullscreen) {

			vm.fullscreen = view->TryEnterFullScreenMode();

		} else {

			view->ExitFullScreenMode();
			vm.fullscreen = false;
		}
	}

	Windows::Foundation::Size desired;
	desired.Width = p_desired.width;
	desired.Height = p_desired.height;

	view->PreferredLaunchViewSize = desired;

	if (view->TryResizeView(desired)) {

		vm.width = view->VisibleBounds.Width;
		vm.height = view->VisibleBounds.Height;
	}

	set_video_mode(vm);

	gl_context->make_current();
	rasterizer = memnew(RasterizerGLES2);

	visual_server = memnew(VisualServerRaster(rasterizer));
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {

		visual_server = memnew(VisualServerWrapMT(visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}

	//
	physics_server = memnew(PhysicsServerSW);
	physics_server->init();

	physics_2d_server = memnew(Physics2DServerSW);
	physics_2d_server->init();

	visual_server->init();

	input = memnew(InputDefault);

	joystick = ref new JoystickWinrt(input);
	joystick->register_events();

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

	managed_object->update_clipboard();

	Clipboard::ContentChanged += ref new EventHandler<Platform::Object ^>(managed_object, &ManagedType::on_clipboard_changed);

	accelerometer = Accelerometer::GetDefault();
	if (accelerometer != nullptr) {
		// 60 FPS
		accelerometer->ReportInterval = (1.0f / 60.0f) * 1000;
		accelerometer->ReadingChanged +=
				ref new TypedEventHandler<Accelerometer ^, AccelerometerReadingChangedEventArgs ^>(managed_object, &ManagedType::on_accelerometer_reading_changed);
	}

	magnetometer = Magnetometer::GetDefault();
	if (magnetometer != nullptr) {
		// 60 FPS
		magnetometer->ReportInterval = (1.0f / 60.0f) * 1000;
		magnetometer->ReadingChanged +=
				ref new TypedEventHandler<Magnetometer ^, MagnetometerReadingChangedEventArgs ^>(managed_object, &ManagedType::on_magnetometer_reading_changed);
	}

	gyrometer = Gyrometer::GetDefault();
	if (gyrometer != nullptr) {
		// 60 FPS
		gyrometer->ReportInterval = (1.0f / 60.0f) * 1000;
		gyrometer->ReadingChanged +=
				ref new TypedEventHandler<Gyrometer ^, GyrometerReadingChangedEventArgs ^>(managed_object, &ManagedType::on_gyroscope_reading_changed);
	}

	_ensure_data_dir();

	if (is_keep_screen_on())
		display_request->RequestActive();

	set_keep_screen_on(GLOBAL_DEF("display/keep_screen_on", true));
}

void OSWinrt::set_clipboard(const String &p_text) {

	DataPackage ^ clip = ref new DataPackage();
	clip->RequestedOperation = DataPackageOperation::Copy;
	clip->SetText(ref new Platform::String((const wchar_t *)p_text.c_str()));

	Clipboard::SetContent(clip);
};

String OSWinrt::get_clipboard() const {

	if (managed_object->clipboard != nullptr)
		return managed_object->clipboard->Data();
	else
		return "";
};

void OSWinrt::input_event(InputEvent &p_event) {

	p_event.ID = ++last_id;

	input->parse_input_event(p_event);

	if (p_event.type == InputEvent::MOUSE_BUTTON && p_event.mouse_button.pressed && p_event.mouse_button.button_index > 3) {

		//send release for mouse wheel
		p_event.mouse_button.pressed = false;
		p_event.ID = ++last_id;
		input->parse_input_event(p_event);
	}
};

void OSWinrt::delete_main_loop() {

	if (main_loop)
		memdelete(main_loop);
	main_loop = NULL;
}

void OSWinrt::set_main_loop(MainLoop *p_main_loop) {

	input->set_main_loop(p_main_loop);
	main_loop = p_main_loop;
}

void OSWinrt::finalize() {

	if (main_loop)
		memdelete(main_loop);

	main_loop = NULL;

	visual_server->finish();
	memdelete(visual_server);
#ifdef OPENGL_ENABLED
	if (gl_context)
		memdelete(gl_context);
#endif
	if (rasterizer)
		memdelete(rasterizer);

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

	memdelete(input);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);

	joystick = nullptr;
}
void OSWinrt::finalize_core() {

	if (mempool_dynamic)
		memdelete(mempool_dynamic);
	delete mempool_static;
}

void OSWinrt::vprint(const char *p_format, va_list p_list, bool p_stderr) {

	char buf[16384 + 1];
	int len = vsnprintf(buf, 16384, p_format, p_list);
	if (len <= 0)
		return;
	buf[len] = 0;

	int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
	if (wlen < 0)
		return;

	wchar_t *wbuf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, buf, len, wbuf, wlen);
	wbuf[wlen] = 0;

	if (p_stderr)
		fwprintf(stderr, L"%s", wbuf);
	else
		wprintf(L"%s", wbuf);

	free(wbuf);

	fflush(stdout);
};

void OSWinrt::alert(const String &p_alert, const String &p_title) {

	Platform::String ^ alert = ref new Platform::String(p_alert.c_str());
	Platform::String ^ title = ref new Platform::String(p_title.c_str());

	MessageDialog ^ msg = ref new MessageDialog(alert, title);

	UICommand ^ close = ref new UICommand("Close", ref new UICommandInvokedHandler(managed_object, &OSWinrt::ManagedType::alert_close));
	msg->Commands->Append(close);
	msg->DefaultCommandIndex = 0;

	managed_object->alert_close_handle = true;

	msg->ShowAsync();
}

void OSWinrt::ManagedType::alert_close(IUICommand ^ command) {

	alert_close_handle = false;
}

void OSWinrt::ManagedType::on_clipboard_changed(Platform::Object ^ sender, Platform::Object ^ ev) {

	update_clipboard();
}

void OSWinrt::ManagedType::update_clipboard() {

	DataPackageView ^ data = Clipboard::GetContent();

	if (data->Contains(StandardDataFormats::Text)) {

		create_task(data->GetTextAsync()).then([this](Platform::String ^ clipboard_content) {
			this->clipboard = clipboard_content;
		});
	}
}

void OSWinrt::ManagedType::on_accelerometer_reading_changed(Accelerometer ^ sender, AccelerometerReadingChangedEventArgs ^ args) {

	AccelerometerReading ^ reading = args->Reading;

	os->input->set_accelerometer(Vector3(
			reading->AccelerationX,
			reading->AccelerationY,
			reading->AccelerationZ));
}

void OSWinrt::ManagedType::on_magnetometer_reading_changed(Magnetometer ^ sender, MagnetometerReadingChangedEventArgs ^ args) {

	MagnetometerReading ^ reading = args->Reading;

	os->input->set_magnetometer(Vector3(
			reading->MagneticFieldX,
			reading->MagneticFieldY,
			reading->MagneticFieldZ));
}

void OSWinrt::ManagedType::on_gyroscope_reading_changed(Gyrometer ^ sender, GyrometerReadingChangedEventArgs ^ args) {

	GyrometerReading ^ reading = args->Reading;

	os->input->set_magnetometer(Vector3(
			reading->AngularVelocityX,
			reading->AngularVelocityY,
			reading->AngularVelocityZ));
}

void OSWinrt::set_mouse_mode(MouseMode p_mode) {

	if (p_mode == MouseMode::MOUSE_MODE_CAPTURED) {

		CoreWindow::GetForCurrentThread()->SetPointerCapture();

	} else {

		CoreWindow::GetForCurrentThread()->ReleasePointerCapture();
	}

	if (p_mode == MouseMode::MOUSE_MODE_CAPTURED || p_mode == MouseMode::MOUSE_MODE_HIDDEN) {

		CoreWindow::GetForCurrentThread()->PointerCursor = nullptr;

	} else {

		CoreWindow::GetForCurrentThread()->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
	}

	mouse_mode = p_mode;

	SetEvent(mouse_mode_changed);
}

OSWinrt::MouseMode OSWinrt::get_mouse_mode() const {

	return mouse_mode;
}

Point2 OSWinrt::get_mouse_pos() const {

	return Point2(old_x, old_y);
}

int OSWinrt::get_mouse_button_state() const {

	return last_button_state;
}

void OSWinrt::set_window_title(const String &p_title) {
}

void OSWinrt::set_video_mode(const VideoMode &p_video_mode, int p_screen) {

	video_mode = p_video_mode;
}
OS::VideoMode OSWinrt::get_video_mode(int p_screen) const {

	return video_mode;
}
void OSWinrt::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
}

void OSWinrt::print_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type) {

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
}

String OSWinrt::get_name() {

	return "WinRT";
}

OS::Date OSWinrt::get_date(bool utc) const {

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
OS::Time OSWinrt::get_time(bool utc) const {

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

OS::TimeZoneInfo OSWinrt::get_time_zone_info() const {
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

uint64_t OSWinrt::get_unix_time() const {

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

void OSWinrt::delay_usec(uint32_t p_usec) const {

	int msec = p_usec < 1000 ? 1 : p_usec / 1000;

	// no Sleep()
	WaitForSingleObjectEx(GetCurrentThread(), msec, false);
}
uint64_t OSWinrt::get_ticks_usec() const {

	uint64_t ticks;
	uint64_t time;
	// This is the number of clock ticks since start
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks);
	// Divide by frequency to get the time in seconds
	time = ticks * 1000000L / ticks_per_second;
	// Subtract the time at game start to get
	// the time since the game started
	time -= ticks_start;
	return time;
}

void OSWinrt::process_events() {
	last_id = joystick->process_controllers(last_id);
	process_key_events();
}

void OSWinrt::process_key_events() {

	for (int i = 0; i < key_event_pos; i++) {

		KeyEvent &kev = key_event_buffer[i];
		InputEvent iev;

		iev.type = InputEvent::KEY;
		iev.key.mod = kev.mod_state;
		iev.key.echo = kev.echo;
		iev.key.scancode = kev.scancode;
		iev.key.unicode = kev.unicode;
		iev.key.pressed = kev.pressed;

		input_event(iev);
	}
	key_event_pos = 0;
}

void OSWinrt::queue_key_event(KeyEvent &p_event) {
	// This merges Char events with the previous Key event, so
	// the unicode can be retrieved without sending duplicate events.
	if (p_event.type == KeyEvent::MessageType::CHAR_EVENT_MESSAGE && key_event_pos > 0) {

		KeyEvent &old = key_event_buffer[key_event_pos - 1];
		ERR_FAIL_COND(old.type != KeyEvent::MessageType::KEY_EVENT_MESSAGE);

		key_event_buffer[key_event_pos - 1].unicode = p_event.unicode;
		return;
	}

	ERR_FAIL_COND(key_event_pos >= KEY_EVENT_BUFFER_SIZE);

	key_event_buffer[key_event_pos++] = p_event;
}

void OSWinrt::set_cursor_shape(CursorShape p_shape) {

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (cursor_shape == p_shape)
		return;

	static const CoreCursorType uwp_cursors[CURSOR_MAX] = {
		CoreCursorType::Arrow,
		CoreCursorType::IBeam,
		CoreCursorType::Hand,
		CoreCursorType::Cross,
		CoreCursorType::Wait,
		CoreCursorType::Wait,
		CoreCursorType::Arrow,
		CoreCursorType::Arrow,
		CoreCursorType::UniversalNo,
		CoreCursorType::SizeNorthSouth,
		CoreCursorType::SizeWestEast,
		CoreCursorType::SizeNortheastSouthwest,
		CoreCursorType::SizeNorthwestSoutheast,
		CoreCursorType::SizeAll,
		CoreCursorType::SizeNorthSouth,
		CoreCursorType::SizeWestEast,
		CoreCursorType::Help
	};

	CoreWindow::GetForCurrentThread()->PointerCursor = ref new CoreCursor(uwp_cursors[p_shape], 0);

	cursor_shape = p_shape;
}

void OSWinrt::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {

	// FIXME:
	// Not implemented, some work needs to be done for this one
	// Might be a good source: https://stackoverflow.com/questions/43793745/set-custom-mouse-cursor-on-windows-10-universal-app-uwp
}

Error OSWinrt::execute(const String &p_path, const List<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool read_stderr) {

	return FAILED;
};

Error OSWinrt::kill(const ProcessID &p_pid) {

	return FAILED;
};

Error OSWinrt::set_cwd(const String &p_cwd) {

	return FAILED;
}

String OSWinrt::get_executable_path() const {

	return "";
}

void OSWinrt::set_icon(const Image &p_icon) {
}

bool OSWinrt::has_environment(const String &p_var) const {

	return false;
};

String OSWinrt::get_environment(const String &p_var) const {

	return "";
};

String OSWinrt::get_stdin_string(bool p_block) {

	return String();
}

void OSWinrt::move_window_to_foreground() {
}

Error OSWinrt::shell_open(String p_uri) {

	return FAILED;
}

String OSWinrt::get_locale() const {

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP // this should work on phone 8.1, but it doesn't
	return "en";
#else
	Platform::String ^ language = Windows::Globalization::Language::CurrentInputMethodLanguageTag;
	return String(language->Data()).replace("-", "_");
#endif
}

void OSWinrt::release_rendering_thread() {

	gl_context->release_current();
}

void OSWinrt::make_rendering_thread() {

	gl_context->make_current();
}

void OSWinrt::swap_buffers() {

	gl_context->swap_buffers();
}

bool OSWinrt::has_touchscreen_ui_hint() const {

	TouchCapabilities ^ tc = ref new TouchCapabilities();
	return tc->TouchPresent != 0 || UIViewSettings::GetForCurrentView()->UserInteractionMode == UserInteractionMode::Touch;
}

bool OSWinrt::has_virtual_keyboard() const {

	return UIViewSettings::GetForCurrentView()->UserInteractionMode == UserInteractionMode::Touch;
}

void OSWinrt::show_virtual_keyboard(const String &p_existing_text, const Rect2 &p_screen_rect) {

	InputPane ^ pane = InputPane::GetForCurrentView();
	pane->TryShow();
}

void OSWinrt::hide_virtual_keyboard() {

	InputPane ^ pane = InputPane::GetForCurrentView();
	pane->TryHide();
}

void OSWinrt::run() {

	if (!main_loop)
		return;

	main_loop->init();

	uint64_t last_ticks = get_ticks_usec();

	int frames = 0;
	uint64_t frame = 0;

	while (!force_quit) {

		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
		if (managed_object->alert_close_handle) continue;
		process_events(); // get rid of pending events
		if (Main::iteration() == true)
			break;
	};

	main_loop->finish();
}

MainLoop *OSWinrt::get_main_loop() const {

	return main_loop;
}

String OSWinrt::get_data_dir() const {

	Windows::Storage::StorageFolder ^ data_folder = Windows::Storage::ApplicationData::Current->LocalFolder;

	return String(data_folder->Path->Data()).replace("\\", "/");
}

OSWinrt::OSWinrt() {

	key_event_pos = 0;
	force_quit = false;
	alt_mem = false;
	gr_mem = false;
	shift_mem = false;
	control_mem = false;
	meta_mem = false;
	minimized = false;

	pressrc = 0;
	old_invalid = true;
	last_id = 0;
	mouse_mode = MOUSE_MODE_VISIBLE;
#ifdef STDOUT_FILE
	stdo = fopen("stdout.txt", "wb");
#endif

	gl_context = NULL;

	display_request = ref new Windows::System::Display::DisplayRequest();

	managed_object = ref new ManagedType;
	managed_object->os = this;

	mouse_mode_changed = CreateEvent(NULL, TRUE, FALSE, L"os_mouse_mode_changed");

	AudioDriverManagerSW::add_driver(&audio_driver);
}

OSWinrt::~OSWinrt() {
#ifdef STDOUT_FILE
	fclose(stdo);
#endif
}
