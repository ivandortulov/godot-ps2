/*************************************************************************/
/*  editor_settings.cpp                                                  */
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
#include "editor_settings.h"
#include "os/dir_access.h"
#include "os/file_access.h"
#include "os/os.h"

#include "editor_node.h"
#include "globals.h"
#include "io/compression.h"
#include "io/config_file.h"
#include "io/file_access_memory.h"
#include "io/resource_loader.h"
#include "io/resource_saver.h"
#include "io/translation_loader_po.h"
#include "os/keyboard.h"
#include "os/os.h"
#include "scene/main/node.h"
#include "scene/main/scene_main_loop.h"
#include "scene/main/viewport.h"
#include "translations.gen.h"
#include "version.h"

Ref<EditorSettings> EditorSettings::singleton = NULL;

EditorSettings *EditorSettings::get_singleton() {

	return singleton.ptr();
}

bool EditorSettings::_set(const StringName &p_name, const Variant &p_value) {

	_THREAD_SAFE_METHOD_

	if (p_name.operator String() == "shortcuts") {

		Array arr = p_value;
		ERR_FAIL_COND_V(arr.size() && arr.size() & 1, true);
		for (int i = 0; i < arr.size(); i += 2) {

			String name = arr[i];
			InputEvent shortcut = arr[i + 1];

			Ref<ShortCut> sc;
			sc.instance();
			sc->set_shortcut(shortcut);
			add_shortcut(name, sc);
		}

		return true;
	}

	if (p_value.get_type() == Variant::NIL)
		props.erase(p_name);
	else {

		if (props.has(p_name))
			props[p_name].variant = p_value;
		else
			props[p_name] = VariantContainer(p_value, last_order++);

		if (save_changed_setting) {
			props[p_name].save = true;
		}
	}

	emit_signal("settings_changed");
	return true;
}
bool EditorSettings::_get(const StringName &p_name, Variant &r_ret) const {

	_THREAD_SAFE_METHOD_

	if (p_name.operator String() == "shortcuts") {

		Array arr;
		for (const Map<String, Ref<ShortCut> >::Element *E = shortcuts.front(); E; E = E->next()) {

			Ref<ShortCut> sc = E->get();

			if (optimize_save) {
				if (!sc->has_meta("original")) {
					continue; //this came from settings but is not any longer used
				}

				InputEvent original = sc->get_meta("original");
				if (sc->is_shortcut(original) || (original.type == InputEvent::NONE && sc->get_shortcut().type == InputEvent::NONE))
					continue; //not changed from default, don't save
			}

			arr.push_back(E->key());
			arr.push_back(sc->get_shortcut());
		}
		r_ret = arr;
		return true;
	}

	const VariantContainer *v = props.getptr(p_name);
	if (!v)
		return false;
	r_ret = v->variant;
	return true;
}

struct _EVCSort {

	String name;
	Variant::Type type;
	int order;
	bool save;

	bool operator<(const _EVCSort &p_vcs) const { return order < p_vcs.order; }
};

void EditorSettings::_get_property_list(List<PropertyInfo> *p_list) const {

	_THREAD_SAFE_METHOD_

	const String *k = NULL;
	Set<_EVCSort> vclist;

	while ((k = props.next(k))) {

		const VariantContainer *v = props.getptr(*k);

		if (v->hide_from_editor)
			continue;

		_EVCSort vc;
		vc.name = *k;
		vc.order = v->order;
		vc.type = v->variant.get_type();
		vc.save = v->save;

		vclist.insert(vc);
	}

	for (Set<_EVCSort>::Element *E = vclist.front(); E; E = E->next()) {

		int pinfo = 0;
		if (E->get().save || !optimize_save) {
			pinfo |= PROPERTY_USAGE_STORAGE;
		}

		if (!E->get().name.begins_with("_") && !E->get().name.begins_with("projects/")) {
			pinfo |= PROPERTY_USAGE_EDITOR;
		} else {
			pinfo |= PROPERTY_USAGE_STORAGE; //hiddens must always be saved
		}

		PropertyInfo pi(E->get().type, E->get().name);
		pi.usage = pinfo;
		if (hints.has(E->get().name))
			pi = hints[E->get().name];

		p_list->push_back(pi);
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "shortcuts", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR)); //do not edit
}

bool EditorSettings::has(String p_var) const {

	_THREAD_SAFE_METHOD_

	return props.has(p_var);
}

void EditorSettings::erase(String p_var) {

	_THREAD_SAFE_METHOD_

	props.erase(p_var);
}

void EditorSettings::raise_order(const String &p_name) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!props.has(p_name));
	props[p_name].order = ++last_order;
}

Variant _EDITOR_DEF(const String &p_var, const Variant &p_default) {

	if (EditorSettings::get_singleton()->has(p_var))
		return EditorSettings::get_singleton()->get(p_var);
	EditorSettings::get_singleton()->set(p_var, p_default);
	return p_default;
}

void EditorSettings::create() {

	if (singleton.ptr())
		return; //pointless

	DirAccess *dir = NULL;
	Variant meta;

	String config_path;
	String config_dir;
	//String config_file="editor_settings.xml";
	Ref<ConfigFile> extra_config = memnew(ConfigFile);

	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();
	DirAccess *d = DirAccess::create_for_path(exe_path);
	bool self_contained = false;

	if (d->file_exists(exe_path + "/._sc_")) {
		self_contained = true;
		extra_config->load(exe_path + "/._sc_");
	} else if (d->file_exists(exe_path + "/_sc_")) {
		self_contained = true;
		extra_config->load(exe_path + "/_sc_");
	}

	if (self_contained) {
		// editor is self contained
		config_path = exe_path;
		config_dir = "editor_data";
	} else {

		if (OS::get_singleton()->has_environment("APPDATA")) {
			// Most likely under windows, save here
			config_path = OS::get_singleton()->get_environment("APPDATA");
			config_dir = String(_MKSTR(VERSION_SHORT_NAME)).capitalize();
		} else if (OS::get_singleton()->has_environment("HOME")) {

			config_path = OS::get_singleton()->get_environment("HOME");
			config_dir = "." + String(_MKSTR(VERSION_SHORT_NAME)).to_lower();
		}
	};

	ObjectTypeDB::register_type<EditorSettings>(); //otherwise it can't be unserialized
	String config_file_path;

	if (config_path != "") {

		dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (dir->change_dir(config_path) != OK) {
			ERR_PRINT("Cannot find path for config directory!");
			memdelete(dir);
			goto fail;
		}

		if (dir->change_dir(config_dir) != OK) {
			dir->make_dir(config_dir);
			if (dir->change_dir(config_dir) != OK) {
				ERR_PRINT("Cannot create config directory!");
				memdelete(dir);
				goto fail;
			}
		}

		if (dir->change_dir("templates") != OK) {
			dir->make_dir("templates");
		} else {

			dir->change_dir("..");
		}

		if (dir->change_dir("text_editor_themes") != OK) {
			dir->make_dir("text_editor_themes");
		} else {
			dir->change_dir("..");
		}

		if (dir->change_dir("tmp") != OK) {
			dir->make_dir("tmp");
		} else {

			dir->change_dir("..");
		}

		if (dir->change_dir("config") != OK) {
			dir->make_dir("config");
		} else {

			dir->change_dir("..");
		}

		dir->change_dir("config");

		String pcp = Globals::get_singleton()->get_resource_path();
		if (pcp.ends_with("/"))
			pcp = config_path.substr(0, pcp.size() - 1);
		pcp = pcp.get_file() + "-" + pcp.md5_text();

		if (dir->change_dir(pcp)) {
			dir->make_dir(pcp);
		} else {
			dir->change_dir("..");
		}

		dir->change_dir("..");

		// path at least is validated, so validate config file

		config_file_path = config_path + "/" + config_dir + "/editor_settings.tres";

		String open_path = config_file_path;

		if (!dir->file_exists("editor_settings.tres")) {

			open_path = config_path + "/" + config_dir + "/editor_settings.xml";

			if (!dir->file_exists("editor_settings.xml")) {

				memdelete(dir);
				WARN_PRINT("Config file does not exist, creating.");
				goto fail;
			}
		}

		memdelete(dir);

		singleton = ResourceLoader::load(open_path, "EditorSettings");

		if (singleton.is_null()) {
			WARN_PRINT("Could not open config file.");
			goto fail;
		}

		singleton->save_changed_setting = true;
		singleton->config_file_path = config_file_path;
		singleton->project_config_path = pcp;
		singleton->settings_path = config_path + "/" + config_dir;

		if (OS::get_singleton()->is_stdout_verbose()) {

			print_line("EditorSettings: Load OK!");
		}

		singleton->setup_language();
		singleton->setup_network();
		singleton->load_favorites();
		singleton->list_text_editor_themes();

		return;
	}

fail:

	// patch init projects
	if (extra_config->has_section("init_projects")) {
		Vector<String> list = extra_config->get_value("init_projects", "list");
		for (int i = 0; i < list.size(); i++) {

			list[i] = exe_path + "/" + list[i];
		};
		extra_config->set_value("init_projects", "list", list);
	};

	singleton = Ref<EditorSettings>(memnew(EditorSettings));
	singleton->save_changed_setting = true;
	singleton->config_file_path = config_file_path;
	singleton->settings_path = config_path + "/" + config_dir;
	singleton->_load_defaults(extra_config);
	singleton->setup_language();
	singleton->setup_network();
	singleton->list_text_editor_themes();
}

String EditorSettings::get_settings_path() const {

	return settings_path;
}

void EditorSettings::setup_language() {

	String lang = get("global/editor_language");
	if (lang == "en")
		return; //none to do

	for (int i = 0; i < translations.size(); i++) {
		if (translations[i]->get_locale() == lang) {
			TranslationServer::get_singleton()->set_tool_translation(translations[i]);
			break;
		}
	}
}

void EditorSettings::setup_network() {

	List<IP_Address> local_ip;
	IP::get_singleton()->get_local_addresses(&local_ip);
	String lip = "127.0.0.1";
	String hint;
	String current = has("network/debug_host") ? get("network/debug_host") : "";
	int port = has("network/debug_port") ? (int)get("network/debug_port") : 6096;

	for (List<IP_Address>::Element *E = local_ip.front(); E; E = E->next()) {

		String ip = E->get();

		// link-local IPv6 addresses don't work, skipping them
		if (ip.begins_with("fe80:0:0:0:")) // fe80::/64
			continue;
		if (ip == current)
			lip = current; //so it saves
		if (hint != "")
			hint += ",";
		hint += ip;
	}

	set("network/debug_host", lip);
	add_property_hint(PropertyInfo(Variant::STRING, "network/debug_host", PROPERTY_HINT_ENUM, hint));

	set("network/debug_port", port);
	add_property_hint(PropertyInfo(Variant::INT, "network/debug_port", PROPERTY_HINT_RANGE, "1,65535,1"));
}

void EditorSettings::save() {

	//_THREAD_SAFE_METHOD_

	if (!singleton.ptr())
		return;

	if (singleton->config_file_path == "") {
		ERR_PRINT("Cannot save EditorSettings config, no valid path");
		return;
	}

	Error err = ResourceSaver::save(singleton->config_file_path, singleton);

	if (err != OK) {
		ERR_PRINT("Can't Save!");
		return;
	}

	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("EditorSettings Save OK!");
	}
}

void EditorSettings::destroy() {

	if (!singleton.ptr())
		return;
	save();
	singleton = Ref<EditorSettings>();
}

void EditorSettings::_load_defaults(Ref<ConfigFile> p_extra_config) {

	_THREAD_SAFE_METHOD_

	{
		String lang_hint = "en";
		String host_lang = OS::get_singleton()->get_locale();

		String best;

		for (int i = 0; i < translations.size(); i++) {
			String locale = translations[i]->get_locale();
			lang_hint += ",";
			lang_hint += locale;

			if (host_lang == locale) {
				best = locale;
			}

			if (best == String() && host_lang.begins_with(locale)) {
				best = locale;
			}
		}

		if (best == String()) {
			best = "en";
		}

		set("global/editor_language", best);
		hints["global/editor_language"] = PropertyInfo(Variant::STRING, "global/editor_language", PROPERTY_HINT_ENUM, lang_hint, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	}

	set("global/hidpi_mode", 0);
	hints["global/hidpi_mode"] = PropertyInfo(Variant::INT, "global/hidpi_mode", PROPERTY_HINT_ENUM, "Auto,LoDPI,HiDPI", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	set("global/show_script_in_scene_tabs", false);
	set("global/font_size", 14);
	hints["global/font_size"] = PropertyInfo(Variant::INT, "global/font_size", PROPERTY_HINT_RANGE, "10,40,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	set("global/source_font_size", 14);
	hints["global/source_font_size"] = PropertyInfo(Variant::INT, "global/source_font_size", PROPERTY_HINT_RANGE, "8,96,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	set("global/custom_font", "");
	hints["global/custom_font"] = PropertyInfo(Variant::STRING, "global/custom_font", PROPERTY_HINT_GLOBAL_FILE, "*.fnt", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	set("global/custom_theme", "");
	hints["global/custom_theme"] = PropertyInfo(Variant::STRING, "global/custom_theme", PROPERTY_HINT_GLOBAL_FILE, "*.res,*.tres,*.theme", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

	set("global/autoscan_project_path", "");
	hints["global/autoscan_project_path"] = PropertyInfo(Variant::STRING, "global/autoscan_project_path", PROPERTY_HINT_GLOBAL_DIR);
	set("global/default_project_path", "");
	hints["global/default_project_path"] = PropertyInfo(Variant::STRING, "global/default_project_path", PROPERTY_HINT_GLOBAL_DIR);
	set("global/default_project_export_path", "");
	hints["global/default_project_export_path"] = PropertyInfo(Variant::STRING, "global/default_project_export_path", PROPERTY_HINT_GLOBAL_DIR);
	set("global/show_script_in_scene_tabs", false);

	set("text_editor/color_theme", "Default");
	hints["text_editor/color_theme"] = PropertyInfo(Variant::STRING, "text_editor/color_theme", PROPERTY_HINT_ENUM, "Default");

	_load_default_text_editor_theme();

	set("text_editor/syntax_highlighting", true);

	set("text_editor/highlight_all_occurrences", true);
	set("text_editor/scroll_past_end_of_file", false);

	set("text_editor/tab_size", 4);
	hints["text_editor/tab_size"] = PropertyInfo(Variant::INT, "text_editor/tab_size", PROPERTY_HINT_RANGE, "1, 64, 1"); // size of 0 crashes.
	set("text_editor/draw_tabs", true);

	set("text_editor/line_numbers_zero_padded", true);

	set("text_editor/show_line_numbers", true);
	set("text_editor/show_breakpoint_gutter", true);

	set("text_editor/show_line_length_guideline", false);
	set("text_editor/line_length_guideline_column", 80);
	hints["text_editor/line_length_guideline_column"] = PropertyInfo(Variant::INT, "text_editor/line_length_guideline_column", PROPERTY_HINT_RANGE, "20, 160, 10");

	set("text_editor/show_members_overview", true);

	set("text_editor/trim_trailing_whitespace_on_save", false);
	set("text_editor/idle_parse_delay", 2);
	set("text_editor/create_signal_callbacks", true);
	set("text_editor/autosave_interval_secs", 0);

	set("text_editor/block_caret", false);
	set("text_editor/caret_blink", false);
	set("text_editor/caret_blink_speed", 0.65);
	hints["text_editor/caret_blink_speed"] = PropertyInfo(Variant::REAL, "text_editor/caret_blink_speed", PROPERTY_HINT_RANGE, "0.1, 10, 0.1");

	set("text_editor/font", "");
	hints["text_editor/font"] = PropertyInfo(Variant::STRING, "text_editor/font", PROPERTY_HINT_GLOBAL_FILE, "*.fnt");
	set("text_editor/auto_brace_complete", false);
	set("text_editor/restore_scripts_on_load", true);

	set("scenetree_editor/duplicate_node_name_num_separator", 0);
	hints["scenetree_editor/duplicate_node_name_num_separator"] = PropertyInfo(Variant::INT, "scenetree_editor/duplicate_node_name_num_separator", PROPERTY_HINT_ENUM, "None,Space,Underscore,Dash");
	//set("scenetree_editor/display_old_action_buttons",false);
	set("scenetree_editor/start_create_dialog_fully_expanded", false);
	set("scenetree_editor/draw_relationship_lines", false);
	set("scenetree_editor/relationship_line_color", Color::html("464646"));

	set("grid_map/pick_distance", 5000.0);

	set("3d_editor/grid_color", Color(0, 1, 0, 0.2));
	hints["3d_editor/grid_color"] = PropertyInfo(Variant::COLOR, "3d_editor/grid_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

	set("3d_editor/default_fov", 55.0);
	set("3d_editor/default_z_near", 0.1);
	set("3d_editor/default_z_far", 500.0);

	set("3d_editor/navigation_scheme", 0);
	hints["3d_editor/navigation_scheme"] = PropertyInfo(Variant::INT, "3d_editor/navigation_scheme", PROPERTY_HINT_ENUM, "Godot,Maya,Modo");
	set("3d_editor/zoom_style", 0);
	hints["3d_editor/zoom_style"] = PropertyInfo(Variant::INT, "3d_editor/zoom_style", PROPERTY_HINT_ENUM, "Vertical, Horizontal");
	set("3d_editor/orbit_modifier", 0);
	hints["3d_editor/orbit_modifier"] = PropertyInfo(Variant::INT, "3d_editor/orbit_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	set("3d_editor/pan_modifier", 1);
	hints["3d_editor/pan_modifier"] = PropertyInfo(Variant::INT, "3d_editor/pan_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	set("3d_editor/zoom_modifier", 4);
	hints["3d_editor/zoom_modifier"] = PropertyInfo(Variant::INT, "3d_editor/zoom_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	set("3d_editor/emulate_numpad", false);
	set("3d_editor/emulate_3_button_mouse", false);
	set("3d_editor/warped_mouse_panning", true);

	set("2d_editor/bone_width", 5);
	set("2d_editor/bone_color1", Color(1.0, 1.0, 1.0, 0.9));
	set("2d_editor/bone_color2", Color(0.75, 0.75, 0.75, 0.9));
	set("2d_editor/bone_selected_color", Color(0.9, 0.45, 0.45, 0.9));
	set("2d_editor/bone_ik_color", Color(0.9, 0.9, 0.45, 0.9));

	set("2d_editor/keep_margins_when_changing_anchors", false);

	set("2d_editor/warped_mouse_panning", true);
	set("2d_editor/scroll_to_pan", false);
	set("2d_editor/pan_speed", 20);

	set("game_window_placement/rect", 1);
	hints["game_window_placement/rect"] = PropertyInfo(Variant::INT, "game_window_placement/rect", PROPERTY_HINT_ENUM, "Top Left,Centered,Custom Position,Force Maximized,Force Fullscreen");
	String screen_hints = TTR("Default (Same as Editor)");
	for (int i = 0; i < OS::get_singleton()->get_screen_count(); i++) {
		screen_hints += ",Monitor " + itos(i + 1);
	}
	set("game_window_placement/rect_custom_position", Vector2());
	set("game_window_placement/screen", 0);
	hints["game_window_placement/screen"] = PropertyInfo(Variant::INT, "game_window_placement/screen", PROPERTY_HINT_ENUM, screen_hints);

	set("on_save/compress_binary_resources", true);
	set("on_save/save_modified_external_resources", true);
	//set("on_save/save_paths_as_relative",false);
	//set("on_save/save_paths_without_extension",false);

	set("text_editor/create_signal_callbacks", true);

	set("file_dialog/show_hidden_files", false);
	set("file_dialog/display_mode", 0);
	hints["file_dialog/display_mode"] = PropertyInfo(Variant::INT, "file_dialog/display_mode", PROPERTY_HINT_ENUM, "Thumbnails,List");
	set("file_dialog/thumbnail_size", 64);
	hints["file_dialog/thumbnail_size"] = PropertyInfo(Variant::INT, "file_dialog/thumbnail_size", PROPERTY_HINT_RANGE, "32,128,16");

	set("filesystem_dock/display_mode", 0);
	hints["filesystem_dock/display_mode"] = PropertyInfo(Variant::INT, "filesystem_dock/display_mode", PROPERTY_HINT_ENUM, "Thumbnails,List");
	set("filesystem_dock/thumbnail_size", 64);
	hints["filesystem_dock/thumbnail_size"] = PropertyInfo(Variant::INT, "filesystem_dock/thumbnail_size", PROPERTY_HINT_RANGE, "32,128,16");

	set("animation/autorename_animation_tracks", true);
	set("animation/confirm_insert_track", true);

	set("property_editor/texture_preview_width", 48);
	set("property_editor/auto_refresh_interval", 0.3);
	set("help/doc_path", "");

	set("import/ask_save_before_reimport", false);

	set("import/pvrtc_texture_tool", "");
#ifdef WINDOWS_ENABLED
	hints["import/pvrtc_texture_tool"] = PropertyInfo(Variant::STRING, "import/pvrtc_texture_tool", PROPERTY_HINT_GLOBAL_FILE, "*.exe");
#else
	hints["import/pvrtc_texture_tool"] = PropertyInfo(Variant::STRING, "import/pvrtc_texture_tool", PROPERTY_HINT_GLOBAL_FILE, "");
#endif
	// TODO: Rename to "import/pvrtc_fast_conversion" to match other names?
	set("PVRTC/fast_conversion", false);

	set("run/auto_save_before_running", true);
	set("resources/save_compressed_resources", true);
	set("resources/auto_reload_modified_images", true);

	set("run/always_close_output_on_stop", false);

	set("import/automatic_reimport_on_sources_changed", true);

	if (p_extra_config.is_valid()) {

		if (p_extra_config->has_section("init_projects") && p_extra_config->has_section_key("init_projects", "list")) {

			Vector<String> list = p_extra_config->get_value("init_projects", "list");
			for (int i = 0; i < list.size(); i++) {

				String name = list[i].replace("/", "::");
				set("projects/" + name, list[i]);
			};
		};

		if (p_extra_config->has_section("presets")) {

			List<String> keys;
			p_extra_config->get_section_keys("presets", &keys);

			for (List<String>::Element *E = keys.front(); E; E = E->next()) {

				String key = E->get();
				Variant val = p_extra_config->get_value("presets", key);
				set(key, val);
			};
		};
	};
}

void EditorSettings::_load_default_text_editor_theme() {

	set("text_editor/background_color", Color::html("3b000000"));
	set("text_editor/completion_background_color", Color::html("2C2A32"));
	set("text_editor/completion_selected_color", Color::html("434244"));
	set("text_editor/completion_existing_color", Color::html("21dfdfdf"));
	set("text_editor/completion_scroll_color", Color::html("ffffff"));
	set("text_editor/completion_font_color", Color::html("aaaaaa"));
	set("text_editor/caret_color", Color::html("aaaaaa"));
	set("text_editor/caret_background_color", Color::html("000000"));
	set("text_editor/line_number_color", Color::html("66aaaaaa"));
	set("text_editor/text_color", Color::html("aaaaaa"));
	set("text_editor/text_selected_color", Color::html("000000"));
	set("text_editor/keyword_color", Color::html("ffffb3"));
	set("text_editor/base_type_color", Color::html("a4ffd4"));
	set("text_editor/engine_type_color", Color::html("83d3ff"));
	set("text_editor/function_color", Color::html("66a2ce"));
	set("text_editor/member_variable_color", Color::html("e64e59"));
	set("text_editor/comment_color", Color::html("676767"));
	set("text_editor/string_color", Color::html("ef6ebe"));
	set("text_editor/number_color", Color::html("EB9532"));
	set("text_editor/symbol_color", Color::html("badfff"));
	set("text_editor/selection_color", Color::html("6ca9c2"));
	set("text_editor/brace_mismatch_color", Color(1, 0.2, 0.2));
	set("text_editor/current_line_color", Color(0.3, 0.5, 0.8, 0.15));
	set("text_editor/line_length_guideline_color", Color(0.3, 0.5, 0.8, 0.1));
	set("text_editor/mark_color", Color(1.0, 0.4, 0.4, 0.4));
	set("text_editor/breakpoint_color", Color(0.8, 0.8, 0.4, 0.2));
	set("text_editor/word_highlighted_color", Color(0.8, 0.9, 0.9, 0.15));
	set("text_editor/search_result_color", Color(0.05, 0.25, 0.05, 1));
	set("text_editor/search_result_border_color", Color(0.1, 0.45, 0.1, 1));
}

void EditorSettings::notify_changes() {

	_THREAD_SAFE_METHOD_

	SceneTree *sml = NULL;

	if (OS::get_singleton()->get_main_loop())
		sml = OS::get_singleton()->get_main_loop()->cast_to<SceneTree>();

	if (!sml) {
		return;
	}

	Node *root = sml->get_root()->get_child(0);

	if (!root) {
		return;
	}
	root->propagate_notification(NOTIFICATION_EDITOR_SETTINGS_CHANGED);
}

void EditorSettings::_add_property_info_bind(const Dictionary &p_info) {

	ERR_FAIL_COND(!p_info.has("name"));
	ERR_FAIL_COND(!p_info.has("type"));

	PropertyInfo pinfo;
	pinfo.name = p_info["name"];
	ERR_FAIL_COND(!props.has(pinfo.name));
	pinfo.type = Variant::Type(p_info["type"].operator int());
	ERR_FAIL_INDEX(pinfo.type, Variant::VARIANT_MAX);

	if (p_info.has("hint"))
		pinfo.hint = PropertyHint(p_info["hint"].operator int());
	if (p_info.has("hint_string"))
		pinfo.hint_string = p_info["hint_string"];

	add_property_hint(pinfo);
}

void EditorSettings::add_property_hint(const PropertyInfo &p_hint) {

	_THREAD_SAFE_METHOD_

	hints[p_hint.name] = p_hint;
}

void EditorSettings::set_favorite_dirs(const Vector<String> &p_favorites) {

	favorite_dirs = p_favorites;
	FileAccess *f = FileAccess::open(get_project_settings_path().plus_file("favorite_dirs"), FileAccess::WRITE);
	if (f) {
		for (int i = 0; i < favorite_dirs.size(); i++)
			f->store_line(favorite_dirs[i]);
		memdelete(f);
	}
}

Vector<String> EditorSettings::get_favorite_dirs() const {

	return favorite_dirs;
}

void EditorSettings::set_recent_dirs(const Vector<String> &p_recent) {

	recent_dirs = p_recent;
	FileAccess *f = FileAccess::open(get_project_settings_path().plus_file("recent_dirs"), FileAccess::WRITE);
	if (f) {
		for (int i = 0; i < recent_dirs.size(); i++)
			f->store_line(recent_dirs[i]);
		memdelete(f);
	}
}

Vector<String> EditorSettings::get_recent_dirs() const {

	return recent_dirs;
}

String EditorSettings::get_project_settings_path() const {

	return get_settings_path().plus_file("config").plus_file(project_config_path);
}

void EditorSettings::load_favorites() {

	FileAccess *f = FileAccess::open(get_project_settings_path().plus_file("favorite_dirs"), FileAccess::READ);
	if (f) {
		String line = f->get_line().strip_edges();
		while (line != "") {
			favorite_dirs.push_back(line);
			line = f->get_line().strip_edges();
		}
		memdelete(f);
	}

	f = FileAccess::open(get_project_settings_path().plus_file("recent_dirs"), FileAccess::READ);
	if (f) {
		String line = f->get_line().strip_edges();
		while (line != "") {
			recent_dirs.push_back(line);
			line = f->get_line().strip_edges();
		}
		memdelete(f);
	}
}

void EditorSettings::list_text_editor_themes() {
	String themes = "Default";
	DirAccess *d = DirAccess::open(settings_path + "/text_editor_themes");
	if (d) {
		d->list_dir_begin();
		String file = d->get_next();
		while (file != String()) {
			if (file.extension() == "tet" && file.basename().to_lower() != "default") {
				themes += "," + file.basename();
			}
			file = d->get_next();
		}
		d->list_dir_end();
		memdelete(d);
	}
	add_property_hint(PropertyInfo(Variant::STRING, "text_editor/color_theme", PROPERTY_HINT_ENUM, themes));
}

void EditorSettings::load_text_editor_theme() {
	if (get("text_editor/color_theme") == "Default") {
		_load_default_text_editor_theme(); // sorry for "Settings changed" console spam
		return;
	}

	String theme_path = get_settings_path() + "/text_editor_themes/" + get("text_editor/color_theme") + ".tet";

	Ref<ConfigFile> cf = memnew(ConfigFile);
	Error err = cf->load(theme_path);

	if (err != OK) {
		return;
	}

	List<String> keys;
	cf->get_section_keys("color_theme", &keys);

	for (List<String>::Element *E = keys.front(); E; E = E->next()) {
		String key = E->get();
		String val = cf->get_value("color_theme", key);

		// don't load if it's not already there!
		if (has("text_editor/" + key)) {

			// make sure it is actually a color
			if (val.is_valid_html_color() && key.find("color") >= 0) {
				props["text_editor/" + key].variant = Color::html(val); // change manually to prevent "Settings changed" console spam
			}
		}
	}
	emit_signal("settings_changed");
	// if it doesn't load just use what is currently loaded
}

bool EditorSettings::import_text_editor_theme(String p_file) {

	if (!p_file.ends_with(".tet")) {
		return false;
	} else {
		if (p_file.get_file().to_lower() == "default.tet") {
			return false;
		}

		DirAccess *d = DirAccess::open(settings_path + "/text_editor_themes");
		if (d) {
			d->copy(p_file, settings_path + "/text_editor_themes/" + p_file.get_file());
			memdelete(d);
			return true;
		}
	}
	return false;
}

bool EditorSettings::save_text_editor_theme() {

	String p_file = get("text_editor/color_theme");

	if (p_file.get_file().to_lower() == "default") {
		return false;
	}
	String theme_path = get_settings_path() + "/text_editor_themes/" + p_file + ".tet";
	return _save_text_editor_theme(theme_path);
}

bool EditorSettings::save_text_editor_theme_as(String p_file) {
	if (!p_file.ends_with(".tet")) {
		p_file += ".tet";
	}

	if (p_file.get_file().to_lower() == "default.tet") {
		return false;
	}
	if (_save_text_editor_theme(p_file)) {

		// switch to theme is saved in the theme directory
		list_text_editor_themes();
		String theme_name = p_file.substr(0, p_file.length() - 4).get_file();

		if (p_file.get_base_dir() == get_settings_path() + "/text_editor_themes") {
			set("text_editor/color_theme", theme_name);
			load_text_editor_theme();
		}
		return true;
	}
	return false;
}

bool EditorSettings::_save_text_editor_theme(String p_file) {
	String theme_section = "color_theme";
	Ref<ConfigFile> cf = memnew(ConfigFile); // hex is better?

	cf->set_value(theme_section, "background_color", ((Color)get("text_editor/background_color")).to_html());
	cf->set_value(theme_section, "completion_background_color", ((Color)get("text_editor/completion_background_color")).to_html());
	cf->set_value(theme_section, "completion_selected_color", ((Color)get("text_editor/completion_selected_color")).to_html());
	cf->set_value(theme_section, "completion_existing_color", ((Color)get("text_editor/completion_existing_color")).to_html());
	cf->set_value(theme_section, "completion_scroll_color", ((Color)get("text_editor/completion_scroll_color")).to_html());
	cf->set_value(theme_section, "completion_font_color", ((Color)get("text_editor/completion_font_color")).to_html());
	cf->set_value(theme_section, "caret_color", ((Color)get("text_editor/caret_color")).to_html());
	cf->set_value(theme_section, "caret_background_color", ((Color)get("text_editor/caret_background_color")).to_html());
	cf->set_value(theme_section, "line_number_color", ((Color)get("text_editor/line_number_color")).to_html());
	cf->set_value(theme_section, "text_color", ((Color)get("text_editor/text_color")).to_html());
	cf->set_value(theme_section, "text_selected_color", ((Color)get("text_editor/text_selected_color")).to_html());
	cf->set_value(theme_section, "keyword_color", ((Color)get("text_editor/keyword_color")).to_html());
	cf->set_value(theme_section, "base_type_color", ((Color)get("text_editor/base_type_color")).to_html());
	cf->set_value(theme_section, "engine_type_color", ((Color)get("text_editor/engine_type_color")).to_html());
	cf->set_value(theme_section, "function_color", ((Color)get("text_editor/function_color")).to_html());
	cf->set_value(theme_section, "member_variable_color", ((Color)get("text_editor/member_variable_color")).to_html());
	cf->set_value(theme_section, "comment_color", ((Color)get("text_editor/comment_color")).to_html());
	cf->set_value(theme_section, "string_color", ((Color)get("text_editor/string_color")).to_html());
	cf->set_value(theme_section, "number_color", ((Color)get("text_editor/number_color")).to_html());
	cf->set_value(theme_section, "symbol_color", ((Color)get("text_editor/symbol_color")).to_html());
	cf->set_value(theme_section, "selection_color", ((Color)get("text_editor/selection_color")).to_html());
	cf->set_value(theme_section, "brace_mismatch_color", ((Color)get("text_editor/brace_mismatch_color")).to_html());
	cf->set_value(theme_section, "current_line_color", ((Color)get("text_editor/current_line_color")).to_html());
	cf->set_value(theme_section, "line_length_guideline_color", ((Color)get("text_editor/line_length_guideline_color")).to_html());
	cf->set_value(theme_section, "mark_color", ((Color)get("text_editor/mark_color")).to_html());
	cf->set_value(theme_section, "breakpoint_color", ((Color)get("text_editor/breakpoint_color")).to_html());
	cf->set_value(theme_section, "word_highlighted_color", ((Color)get("text_editor/word_highlighted_color")).to_html());
	cf->set_value(theme_section, "search_result_color", ((Color)get("text_editor/search_result_color")).to_html());
	cf->set_value(theme_section, "search_result_border_color", ((Color)get("text_editor/search_result_border_color")).to_html());

	Error err = cf->save(p_file);

	if (err == OK) {
		return true;
	}
	return false;
}

void EditorSettings::add_shortcut(const String &p_name, Ref<ShortCut> &p_shortcut) {

	shortcuts[p_name] = p_shortcut;
}

bool EditorSettings::is_shortcut(const String &p_name, const InputEvent &p_event) const {

	const Map<String, Ref<ShortCut> >::Element *E = shortcuts.find(p_name);
	if (!E) {
		ERR_EXPLAIN("Unknown Shortcut: " + p_name);
		ERR_FAIL_V(false);
	}

	return E->get()->is_shortcut(p_event);
}

Ref<ShortCut> EditorSettings::get_shortcut(const String &p_name) const {

	const Map<String, Ref<ShortCut> >::Element *E = shortcuts.find(p_name);
	if (!E)
		return Ref<ShortCut>();

	return E->get();
}

void EditorSettings::get_shortcut_list(List<String> *r_shortcuts) {

	for (const Map<String, Ref<ShortCut> >::Element *E = shortcuts.front(); E; E = E->next()) {

		r_shortcuts->push_back(E->key());
	}
}

void EditorSettings::set_optimize_save(bool p_optimize) {

	optimize_save = p_optimize;
}

void EditorSettings::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("erase", "property"), &EditorSettings::erase);
	ObjectTypeDB::bind_method(_MD("get_settings_path"), &EditorSettings::get_settings_path);
	ObjectTypeDB::bind_method(_MD("get_project_settings_path"), &EditorSettings::get_project_settings_path);

	ObjectTypeDB::bind_method(_MD("add_property_info", "info"), &EditorSettings::_add_property_info_bind);

	ObjectTypeDB::bind_method(_MD("set_favorite_dirs", "dirs"), &EditorSettings::set_favorite_dirs);
	ObjectTypeDB::bind_method(_MD("get_favorite_dirs"), &EditorSettings::get_favorite_dirs);

	ObjectTypeDB::bind_method(_MD("set_recent_dirs", "dirs"), &EditorSettings::set_recent_dirs);
	ObjectTypeDB::bind_method(_MD("get_recent_dirs"), &EditorSettings::get_recent_dirs);

	ADD_SIGNAL(MethodInfo("settings_changed"));
}

EditorSettings::EditorSettings() {

	//singleton=this;
	last_order = 0;
	optimize_save = true;
	save_changed_setting = true;

	EditorTranslationList *etl = _editor_translations;

	while (etl->data) {

		Vector<uint8_t> data;
		data.resize(etl->uncomp_size);
		Compression::decompress(data.ptr(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);

		FileAccessMemory *fa = memnew(FileAccessMemory);
		fa->open_custom(data.ptr(), data.size());

		Ref<Translation> tr = TranslationLoaderPO::load_translation(fa, NULL, "translation_" + String(etl->lang));

		if (tr.is_valid()) {
			tr->set_locale(etl->lang);
			translations.push_back(tr);
		}

		etl++;
	}

	_load_defaults();
}

EditorSettings::~EditorSettings() {

	//	singleton=NULL;
}

Ref<ShortCut> ED_GET_SHORTCUT(const String &p_path) {

	Ref<ShortCut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);
	if (!sc.is_valid()) {
		ERR_EXPLAIN("Used ED_GET_SHORTCUT with invalid shortcut: " + p_path);
		ERR_FAIL_COND_V(!sc.is_valid(), sc);
	}

	return sc;
}

Ref<ShortCut> ED_SHORTCUT(const String &p_path, const String &p_name, uint32_t p_keycode) {

	InputEvent ie;
	if (p_keycode) {
		ie.type = InputEvent::KEY;
		ie.key.unicode = p_keycode & KEY_CODE_MASK;
		ie.key.scancode = p_keycode & KEY_CODE_MASK;
		ie.key.mod.shift = bool(p_keycode & KEY_MASK_SHIFT);
		ie.key.mod.alt = bool(p_keycode & KEY_MASK_ALT);
		ie.key.mod.control = bool(p_keycode & KEY_MASK_CTRL);
		ie.key.mod.meta = bool(p_keycode & KEY_MASK_META);
	}

	Ref<ShortCut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);
	if (sc.is_valid()) {

		sc->set_name(p_name); //keep name (the ones that come from disk have no name)
		sc->set_meta("original", ie); //to compare against changes
		return sc;
	}

	sc.instance();
	sc->set_name(p_name);
	sc->set_shortcut(ie);
	sc->set_meta("original", ie); //to compare against changes
	EditorSettings::get_singleton()->add_shortcut(p_path, sc);

	return sc;
}
