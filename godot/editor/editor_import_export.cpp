/*************************************************************************/
/*  editor_import_export.cpp                                             */
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
#include "editor_import_export.h"

#include "editor/editor_file_system.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_node.h"
#include "editor_settings.h"
#include "globals.h"
#include "io/config_file.h"
#include "io/resource_loader.h"
#include "io/resource_saver.h"
#include "io/zip_io.h"
#include "io_plugins/editor_texture_import_plugin.h"
#include "os/dir_access.h"
#include "os/file_access.h"
#include "script_language.h"
#include "version.h"

#include "thirdparty/misc/md5.h"

String EditorImportPlugin::validate_source_path(const String &p_path) {

	String gp = Globals::get_singleton()->globalize_path(p_path);
	String rp = Globals::get_singleton()->get_resource_path();
	if (!rp.ends_with("/"))
		rp += "/";

	return rp.path_to_file(gp);
}

String EditorImportPlugin::expand_source_path(const String &p_path) {

	if (p_path.is_rel_path()) {
		return Globals::get_singleton()->get_resource_path().plus_file(p_path).simplify_path();
	} else {
		return p_path;
	}
}

String EditorImportPlugin::_validate_source_path(const String &p_path) {

	return validate_source_path(p_path);
}

String EditorImportPlugin::_expand_source_path(const String &p_path) {

	return expand_source_path(p_path);
}

void EditorImportPlugin::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("validate_source_path", "path"), &EditorImportPlugin::_validate_source_path);
	ObjectTypeDB::bind_method(_MD("expand_source_path", "path"), &EditorImportPlugin::_expand_source_path);

	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::STRING, "get_name"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::STRING, "get_visible_name"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("import_dialog", PropertyInfo(Variant::STRING, "from")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::INT, "import", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "from", PROPERTY_HINT_RESOURCE_TYPE, "ResourceImportMetadata")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::RAW_ARRAY, "custom_export", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "platform", PROPERTY_HINT_RESOURCE_TYPE, "EditorExportPlatform")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("import_from_drop", PropertyInfo(Variant::STRING_ARRAY, "files"), PropertyInfo(Variant::STRING, "dest_path")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("reimport_multiple_files", PropertyInfo(Variant::STRING_ARRAY, "files")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::BOOL, "can_reimport_multiple_files"));

	//	BIND_VMETHOD( mi );
}

String EditorImportPlugin::get_name() const {

	if (get_script_instance() && get_script_instance()->has_method("get_name")) {
		return get_script_instance()->call("get_name");
	}

	ERR_FAIL_V("");
}

String EditorImportPlugin::get_visible_name() const {

	if (get_script_instance() && get_script_instance()->has_method("get_visible_name")) {
		return get_script_instance()->call("get_visible_name");
	}

	ERR_FAIL_V("");
}

void EditorImportPlugin::import_dialog(const String &p_from) {

	if (get_script_instance() && get_script_instance()->has_method("import_dialog")) {
		get_script_instance()->call("import_dialog", p_from);
		return;
	}

	ERR_FAIL();
}

Error EditorImportPlugin::import(const String &p_path, const Ref<ResourceImportMetadata> &p_from) {

	if (get_script_instance() && get_script_instance()->has_method("import")) {
		return Error(get_script_instance()->call("import", p_path, p_from).operator int());
	}

	ERR_FAIL_V(ERR_UNAVAILABLE);
}

Vector<uint8_t> EditorImportPlugin::custom_export(const String &p_path, const Ref<EditorExportPlatform> &p_platform) {

	if (get_script_instance() && get_script_instance()->has_method("custom_export")) {
		get_script_instance()->call("custom_export", p_path, p_platform);
	}

	return Vector<uint8_t>();
}

bool EditorImportPlugin::can_reimport_multiple_files() const {

	if (get_script_instance() && get_script_instance()->has_method("can_reimport_multiple_files")) {
		return get_script_instance()->call("can_reimport_multiple_files");
	}

	return false;
}
void EditorImportPlugin::reimport_multiple_files(const Vector<String> &p_list) {

	if (get_script_instance() && get_script_instance()->has_method("reimport_multiple_files")) {
		get_script_instance()->call("reimport_multiple_files", p_list);
	}
}

void EditorImportPlugin::import_from_drop(const Vector<String> &p_drop, const String &p_dest_path) {

	if (get_script_instance() && get_script_instance()->has_method("import_from_drop")) {
		get_script_instance()->call("import_from_drop", p_drop, p_dest_path);
	}
}

EditorImportPlugin::EditorImportPlugin() {
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void EditorExportPlugin::_bind_methods() {

	MethodInfo mi = MethodInfo("custom_export:Variant", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::OBJECT, "platform", PROPERTY_HINT_RESOURCE_TYPE, "EditorExportPlatform"));
	mi.return_val.type = Variant::RAW_ARRAY;

	BIND_VMETHOD(mi);
}

Vector<uint8_t> EditorExportPlugin::custom_export(String &p_path, const Ref<EditorExportPlatform> &p_platform) {

	if (get_script_instance()) {

		Variant d = get_script_instance()->call("custom_export", p_path, p_platform);
		if (d.get_type() == Variant::NIL)
			return Vector<uint8_t>();
		if (d.get_type() == Variant::RAW_ARRAY)
			return d;

		ERR_FAIL_COND_V(d.get_type() != Variant::DICTIONARY, Vector<uint8_t>());
		Dictionary dict = d;
		ERR_FAIL_COND_V(!dict.has("name"), Vector<uint8_t>());
		ERR_FAIL_COND_V(!dict.has("data"), Vector<uint8_t>());
		p_path = dict["name"];
		return dict["data"];
	}

	return Vector<uint8_t>();
}

EditorExportPlugin::EditorExportPlugin() {
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static void _split_by_comma(List<String> &p_list, const String &p_src) {

	if (p_src == "")
		return;
	Vector<String> split = p_src.split(",");
	for (int i = 0; i < split.size(); i++) {
		String f = split[i].strip_edges();
		if (f.empty())
			continue;
		p_list.push_back(f);
	}
}

static void _add_to_list(EditorFileSystemDirectory *p_efsd, Set<StringName> &r_list, const String &p_filter) {

	List<String> filters;
	_split_by_comma(filters, p_filter);

	for (int i = 0; i < p_efsd->get_subdir_count(); i++) {
		if (p_filter == "" || filters.find(p_efsd->get_subdir(i)->get_name()) == NULL)
			_add_to_list(p_efsd->get_subdir(i), r_list, p_filter);
		else
			print_line("Folder ignored by settings: " + p_efsd->get_subdir(i)->get_name());
	}

	for (int i = 0; i < p_efsd->get_file_count(); i++) {
		r_list.insert(p_efsd->get_file_path(i));
	}
}

struct __EESortDepCmp {

	_FORCE_INLINE_ bool operator()(const StringName &p_l, const StringName &p_r) const {
		return p_l.operator String() < p_r.operator String();
	}
};

static void _edit_files_with_filter(DirAccess *da, const List<String> &p_filters, Set<StringName> &r_list, bool exclude) {

	List<String> files;
	List<String> dirs;

	da->list_dir_begin();

	String f = da->get_next();
	while (f != "") {
		if (da->current_is_dir())
			dirs.push_back(f);
		else
			files.push_back(f);

		f = da->get_next();
	}

	String r = da->get_current_dir().replace("\\", "/");
	if (!r.ends_with("/"))
		r += "/";

	for (List<String>::Element *E = files.front(); E; E = E->next()) {
		String fullpath = r + E->get();
		for (const List<String>::Element *F = p_filters.front(); F; F = F->next()) {

			if (fullpath.matchn(F->get())) {
				String act = TTR("Added:") + " ";

				if (!exclude) {
					r_list.insert(fullpath);
				} else {
					act = TTR("Removed:") + " ";
					r_list.erase(fullpath);
				}

				print_line(act + fullpath);
			}
		}
	}

	da->list_dir_end();

	for (List<String>::Element *E = dirs.front(); E; E = E->next()) {
		if (E->get().begins_with("."))
			continue;
		da->change_dir(E->get());
		_edit_files_with_filter(da, p_filters, r_list, exclude);
		da->change_dir("..");
	}
}

static void _edit_filter_list(Set<StringName> &r_list, const String &p_filter, bool exclude) {

	if (p_filter == "")
		return;
	List<String> filters;
	_split_by_comma(filters, p_filter);

	DirAccess *da = DirAccess::open("res://");
	ERR_FAIL_NULL(da);
	_edit_files_with_filter(da, filters, r_list, exclude);
	memdelete(da);
}

static void _add_filter_to_list(Set<StringName> &r_list, const String &p_filter) {
	_edit_filter_list(r_list, p_filter, false);
}

static void _remove_filter_from_list(Set<StringName> &r_list, const String &p_filter) {
	_edit_filter_list(r_list, p_filter, true);
}

bool EditorExportPlatform::_set(const StringName &p_name, const Variant &p_value) {

	String n = p_name;

	if (n == "debug/debugging_enabled") {
		set_debugging_enabled(p_value);
	} else {
		return false;
	}

	return true;
}

bool EditorExportPlatform::_get(const StringName &p_name, Variant &r_ret) const {

	String n = p_name;

	if (n == "debug/debugging_enabled") {
		r_ret = is_debugging_enabled();
	} else {
		return false;
	}

	return true;
}

void EditorExportPlatform::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_front(PropertyInfo(Variant::BOOL, "debug/debugging_enabled"));
}

Vector<uint8_t> EditorExportPlatform::get_exported_file_default(String &p_fname) const {

	FileAccess *f = FileAccess::open(p_fname, FileAccess::READ);
	ERR_FAIL_COND_V(!f, Vector<uint8_t>());
	Vector<uint8_t> ret;
	ret.resize(f->get_len());
	int rbs = f->get_buffer(ret.ptr(), ret.size());
	memdelete(f);
	return ret;
}

Vector<uint8_t> EditorExportPlatform::get_exported_file(String &p_fname) const {

	Ref<EditorExportPlatform> ep = EditorImportExport::get_singleton()->get_export_platform(get_name());

	for (int i = 0; i < EditorImportExport::get_singleton()->get_export_plugin_count(); i++) {

		Vector<uint8_t> data = EditorImportExport::get_singleton()->get_export_plugin(i)->custom_export(p_fname, ep);
		if (data.size())
			return data;
	}

	return get_exported_file_default(p_fname);
}

Vector<StringName> EditorExportPlatform::get_dependencies(bool p_bundles) const {

	Set<StringName> exported;

	if (FileAccess::exists("res://engine.cfg"))
		exported.insert("res://engine.cfg");

	if (EditorImportExport::get_singleton()->get_export_filter() != EditorImportExport::EXPORT_SELECTED) {

		String filter;
		if (EditorImportExport::get_singleton()->get_export_filter() == EditorImportExport::EXPORT_ALL) {
			_add_filter_to_list(exported, "*");
		} else {
			String cf = EditorImportExport::get_singleton()->get_export_custom_filter_exclude_dir();
			_add_to_list(EditorFileSystem::get_singleton()->get_filesystem(), exported, cf);
			cf = EditorImportExport::get_singleton()->get_export_custom_filter();
			if (cf != "")
				cf += ",";
			cf += "*.flags";
			_add_filter_to_list(exported, cf);

			cf = EditorImportExport::get_singleton()->get_export_custom_filter_exclude();
			_remove_filter_from_list(exported, cf);
		}

	} else {

		Map<String, Map<String, String> > remapped_paths;

		Set<String> scene_extensions;
		Set<String> resource_extensions;

		{

			List<String> l;
			//		SceneLoader::get_recognized_extensions(&l);
			//		for(List<String>::Element *E=l.front();E;E=E->next()) {
			//
			//			scene_extensions.insert(E->get());
			//		}
			ResourceLoader::get_recognized_extensions_for_type("", &l);
			for (List<String>::Element *E = l.front(); E; E = E->next()) {

				resource_extensions.insert(E->get());
			}
		}

		List<StringName> toexport;

		EditorImportExport::get_singleton()->get_export_file_list(&toexport);

		print_line("TO EXPORT: " + itos(toexport.size()));

		for (List<StringName>::Element *E = toexport.front(); E; E = E->next()) {

			print_line("DEP: " + String(E->get()));
			exported.insert(E->get());
			if (p_bundles && EditorImportExport::get_singleton()->get_export_file_action(E->get()) == EditorImportExport::ACTION_BUNDLE) {
				print_line("NO BECAUSE OF BUNDLE!");
				continue; //no dependencies needed to be copied
			}

			List<String> testsubs;
			testsubs.push_back(E->get());

			while (testsubs.size()) {
				//recursive subdep search!
				List<String> deplist;
				ResourceLoader::get_dependencies(testsubs.front()->get(), &deplist);
				testsubs.pop_front();

				List<String> subdeps;

				for (List<String>::Element *F = deplist.front(); F; F = F->next()) {

					StringName dep = F->get();

					if (exported.has(dep) || EditorImportExport::get_singleton()->get_export_file_action(dep) != EditorImportExport::ACTION_NONE)
						continue; //dependency added or to be added
					print_line(" SUBDEP: " + String(dep));

					exported.insert(dep);
					testsubs.push_back(dep);
				}
			}
		}
		String cf = EditorImportExport::get_singleton()->get_export_custom_filter();
		if (cf != "")
			cf += ",";
		cf += "*.flags";
		_add_filter_to_list(exported, cf);

		cf = EditorImportExport::get_singleton()->get_export_custom_filter_exclude();
		_remove_filter_from_list(exported, cf);
	}

	Vector<StringName> ret;
	ret.resize(exported.size());

	int idx = 0;
	for (Set<StringName>::Element *E = exported.front(); E; E = E->next()) {

		ret[idx++] = E->get();
	}

	SortArray<StringName, __EESortDepCmp> sort; //some platforms work better if this is sorted
	sort.sort(ret.ptr(), ret.size());

	return ret;
}

String EditorExportPlatform::find_export_template(String template_file_name, String *err) const {
	String user_file = EditorSettings::get_singleton()->get_settings_path() + "/templates/" + template_file_name;
	String system_file = OS::get_singleton()->get_installed_templates_path();
	bool has_system_path = (system_file != "");
	system_file += template_file_name;

	// Prefer user file
	if (FileAccess::exists(user_file)) {
		return user_file;
	}

	// Now check system file
	if (has_system_path) {
		if (FileAccess::exists(system_file)) {
			return system_file;
		}
	}

	// Not found
	if (err) {
		*err += "No export template found at \"" + user_file + "\"";
		if (has_system_path)
			*err += "\n or \"" + system_file + "\".";
		else
			*err += ".";
	}
	return "";
}

bool EditorExportPlatform::exists_export_template(String template_file_name, String *err) const {
	return find_export_template(template_file_name, err) != "";
}

///////////////////////////////////////

bool EditorExportPlatform::is_debugging_enabled() const {

	return debugging_enabled;
}

void EditorExportPlatform::set_debugging_enabled(bool p_enabled) {

	debugging_enabled = p_enabled;
}

int EditorExportPlatform::get_chmod_flags() const {

	return chmod_flags;
}

void EditorExportPlatform::set_chmod_flags(int p_flags) {

	chmod_flags = p_flags;
}

bool EditorExportPlatformPC::_set(const StringName &p_name, const Variant &p_value) {

	String n = p_name;

	if (n == "custom_binary/release") {

		custom_release_binary = p_value;
	} else if (n == "custom_binary/debug") {

		custom_debug_binary = p_value;
	} else if (n == "resources/pack_mode") {

		export_mode = ExportMode(int(p_value));
	} else if (n == "resources/bundle_dependencies_(for_optical_disc)") {

		bundle = p_value;
	} else if (n == "binary/64_bits") {

		use64 = p_value;
	} else
		return false;

	return true;
}

bool EditorExportPlatformPC::_get(const StringName &p_name, Variant &r_ret) const {

	String n = p_name;

	if (n == "custom_binary/release") {

		r_ret = custom_release_binary;
	} else if (n == "custom_binary/debug") {

		r_ret = custom_debug_binary;
	} else if (n == "resources/pack_mode") {

		r_ret = export_mode;
	} else if (n == "resources/bundle_dependencies_(for_optical_disc)") {

		r_ret = bundle;
	} else if (n == "binary/64_bits") {

		r_ret = use64;
	} else
		return false;

	return true;
}

void EditorExportPlatformPC::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::STRING, "custom_binary/debug", PROPERTY_HINT_GLOBAL_FILE, binary_extension));
	p_list->push_back(PropertyInfo(Variant::STRING, "custom_binary/release", PROPERTY_HINT_GLOBAL_FILE, binary_extension));
	p_list->push_back(PropertyInfo(Variant::INT, "resources/pack_mode", PROPERTY_HINT_ENUM, "Pack into executable,Pack into binary file (.pck),Pack into archive file (.zip)"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "resources/bundle_dependencies_(for_optical_disc)"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "binary/64_bits"));
}

static void _exp_add_dep(Map<StringName, List<StringName> > &deps, const StringName &p_path) {

	if (deps.has(p_path))
		return; //already done

	deps.insert(p_path, List<StringName>());

	List<StringName> &deplist = deps[p_path];
	Set<StringName> depset;

	List<String> dl;
	ResourceLoader::get_dependencies(p_path, &dl);

	//added in order so child dependencies are always added bfore parent dependencies
	for (List<String>::Element *E = dl.front(); E; E = E->next()) {

		if (!deps.has(E->get()))
			_exp_add_dep(deps, E->get());

		for (List<StringName>::Element *F = deps[E->get()].front(); F; F = F->next()) {

			if (!depset.has(F->get())) {
				depset.insert(F->get());
				deplist.push_back(F->get());
			}
		}

		if (!depset.has(E->get())) {
			depset.insert(E->get());
			deplist.push_back(E->get());
		}
	}
}

Error EditorExportPlatform::export_project_files(EditorExportSaveFunction p_func, void *p_udata, bool p_make_bundles) {

	/* ALL FILES AND DEPENDENCIES */

	Vector<StringName> files = get_dependencies(p_make_bundles);

	Map<StringName, List<StringName> > deps;

	if (false) {
		for (int i = 0; i < files.size(); i++) {

			_exp_add_dep(deps, files[i]);
		}
	}

	/* GROUP ATLAS */

	List<StringName> groups;

	EditorImportExport::get_singleton()->image_export_get_groups(&groups);

	Map<StringName, StringName> remap_files;
	Set<StringName> saved;

	int counter = 0;

	for (List<StringName>::Element *E = groups.front(); E; E = E->next()) {

		if (!EditorImportExport::get_singleton()->image_export_group_get_make_atlas(E->get()))
			continue; //uninterested, only process for atlas!

		List<StringName> atlas_images;
		EditorImportExport::get_singleton()->image_export_get_images_in_group(E->get(), &atlas_images);
		atlas_images.sort_custom<StringName::AlphCompare>();

		for (List<StringName>::Element *F = atlas_images.front(); F;) {

			List<StringName>::Element *N = F->next();

			if (!FileAccess::exists(F->get())) {
				atlas_images.erase(F);
			}

			F = N;
		}

		if (atlas_images.size() <= 1)
			continue;

		int group_format = 0;
		float group_lossy_quality = EditorImportExport::get_singleton()->image_export_group_get_lossy_quality(E->get());
		int group_shrink = EditorImportExport::get_singleton()->image_export_group_get_shrink(E->get());
		group_shrink *= EditorImportExport::get_singleton()->get_export_image_shrink();

		switch (EditorImportExport::get_singleton()->image_export_group_get_image_action(E->get())) {
			case EditorImportExport::IMAGE_ACTION_KEEP:
			case EditorImportExport::IMAGE_ACTION_NONE: {

				switch (EditorImportExport::get_singleton()->get_export_image_action()) {
					case EditorImportExport::IMAGE_ACTION_NONE: {

						group_format = EditorTextureImportPlugin::IMAGE_FORMAT_COMPRESS_DISK_LOSSLESS; //?

					} break; //use default
					case EditorImportExport::IMAGE_ACTION_COMPRESS_DISK: {
						group_format = EditorTextureImportPlugin::IMAGE_FORMAT_COMPRESS_DISK_LOSSY;
					} break; //use default
					case EditorImportExport::IMAGE_ACTION_COMPRESS_RAM: {
						group_format = EditorTextureImportPlugin::IMAGE_FORMAT_COMPRESS_RAM;
					} break; //use default
				}

				group_lossy_quality = EditorImportExport::get_singleton()->get_export_image_quality();

			} break; //use default
			case EditorImportExport::IMAGE_ACTION_COMPRESS_DISK: {
				group_format = EditorTextureImportPlugin::IMAGE_FORMAT_COMPRESS_DISK_LOSSY;
			} break; //use default
			case EditorImportExport::IMAGE_ACTION_COMPRESS_RAM: {
				group_format = EditorTextureImportPlugin::IMAGE_FORMAT_COMPRESS_RAM;
			} break; //use default
		}

		String image_list_md5;

		{
			MD5_CTX ctx;
			MD5Init(&ctx);
			for (List<StringName>::Element *F = atlas_images.front(); F; F = F->next()) {

				String p = F->get();
				MD5Update(&ctx, (unsigned char *)p.utf8().get_data(), p.utf8().length());
			}

			MD5Final(&ctx);
			image_list_md5 = String::md5(ctx.digest);
		}
		//ok see if cached
		String md5;
		bool atlas_valid = true;
		String atlas_name;

		{
			MD5_CTX ctx;
			MD5Init(&ctx);
			String path = Globals::get_singleton()->get_resource_path() + "::" + String(E->get()) + "::" + get_name();
			MD5Update(&ctx, (unsigned char *)path.utf8().get_data(), path.utf8().length());
			MD5Final(&ctx);
			md5 = String::md5(ctx.digest);
		}

		FileAccess *f = NULL;

		if (!FileAccess::exists(EditorSettings::get_singleton()->get_settings_path() + "/tmp/atlas-" + md5)) {
			print_line("NO MD5 INVALID");
			atlas_valid = false;
		}

		if (atlas_valid)
			f = FileAccess::open(EditorSettings::get_singleton()->get_settings_path() + "/tmp/atlas-" + md5, FileAccess::READ);

		if (atlas_valid) {
			//compare options
			Dictionary options;
			options.parse_json(f->get_line());
			if (!options.has("lossy_quality") || float(options["lossy_quality"]) != group_lossy_quality)
				atlas_valid = false;
			else if (!options.has("shrink") || int(options["shrink"]) != group_shrink)
				atlas_valid = false;
			else if (!options.has("image_format") || int(options["image_format"]) != group_format)
				atlas_valid = false;

			if (!atlas_valid)
				print_line("JSON INVALID");
		}

		if (atlas_valid) {
			//check md5 of list of image /names/
			if (f->get_line().strip_edges() != image_list_md5) {
				atlas_valid = false;
				print_line("IMAGE MD5 INVALID!");
			}
		}

		Vector<Rect2> rects;
		bool resave_deps = false;

		if (atlas_valid) {

			//check if images were not modified
			for (List<StringName>::Element *F = atlas_images.front(); F; F = F->next()) {

				Vector<String> slices = f->get_line().strip_edges().split("::");

				if (slices.size() != 10) {
					atlas_valid = false;
					print_line("CANT SLICE IN 10");
					break;
				}
				uint64_t mod_time = slices[0].to_int64();
				uint64_t file_mod_time = FileAccess::get_modified_time(F->get());
				if (mod_time != file_mod_time) {

					String image_md5 = slices[1];
					String file_md5 = FileAccess::get_md5(F->get());

					if (image_md5 != file_md5) {
						atlas_valid = false;
						print_line("IMAGE INVALID " + slices[0]);
						break;
					} else {
						resave_deps = true;
					}
				}

				if (atlas_valid) {
					//push back region and margin
					rects.push_back(Rect2(slices[2].to_float(), slices[3].to_float(), slices[4].to_float(), slices[5].to_float()));
					rects.push_back(Rect2(slices[6].to_float(), slices[7].to_float(), slices[8].to_float(), slices[9].to_float()));
				}
			}
		}

		if (f) {
			memdelete(f);
			f = NULL;
		}

		print_line("ATLAS VALID? " + itos(atlas_valid) + " RESAVE DEPS? " + itos(resave_deps));
		if (!atlas_valid) {
			rects.clear();
			//oh well, atlas is not valid. need to make new one....

			String dst_file = EditorSettings::get_singleton()->get_settings_path() + "/tmp/atlas-" + md5 + ".tex";
			Ref<ResourceImportMetadata> imd = memnew(ResourceImportMetadata);
			//imd->set_editor();

			for (List<StringName>::Element *F = atlas_images.front(); F; F = F->next()) {

				imd->add_source(EditorImportPlugin::validate_source_path(F->get()), FileAccess::get_md5(F->get()));
			}

			imd->set_option("format", group_format);

			int flags = 0;

			if (Globals::get_singleton()->get("image_loader/filter"))
				flags |= EditorTextureImportPlugin::IMAGE_FLAG_FILTER;
			if (!Globals::get_singleton()->get("image_loader/gen_mipmaps"))
				flags |= EditorTextureImportPlugin::IMAGE_FLAG_NO_MIPMAPS;
			if (Globals::get_singleton()->get("image_loader/repeat"))
				flags |= EditorTextureImportPlugin::IMAGE_FLAG_REPEAT;

			flags |= EditorTextureImportPlugin::IMAGE_FLAG_FIX_BORDER_ALPHA;

			imd->set_option("flags", flags);
			imd->set_option("quality", group_lossy_quality);
			imd->set_option("atlas", true);
			imd->set_option("crop", true);
			imd->set_option("shrink", group_shrink);

			Ref<EditorTextureImportPlugin> plugin = EditorImportExport::get_singleton()->get_import_plugin_by_name("texture");
			Error err = plugin->import2(dst_file, imd, get_image_compression(), true);
			if (err) {

				EditorNode::add_io_error(TTR("Error saving atlas:") + " " + dst_file.get_file());
				return ERR_CANT_CREATE;
			}

			ERR_FAIL_COND_V(imd->get_option("rects") == Variant(), ERR_BUG);

			Array r_rects = imd->get_option("rects");
			rects.resize(r_rects.size());
			for (int i = 0; i < r_rects.size(); i++) {
				//get back region and margins
				rects[i] = r_rects[i];
			}

			resave_deps = true;
		}

		//atlas is valid (or it was just saved i guess), create the atex files and save them

		if (resave_deps) {
			f = FileAccess::open(EditorSettings::get_singleton()->get_settings_path() + "/tmp/atlas-" + md5, FileAccess::WRITE);
			Dictionary options;
			options["lossy_quality"] = group_lossy_quality;
			options["shrink"] = EditorImportExport::get_singleton()->image_export_group_get_shrink(E->get());
			options["image_format"] = group_format;
			f->store_line(options.to_json());
			f->store_line(image_list_md5);
		}

		//go through all ATEX files

		{
			Ref<ImageTexture> atlas = memnew(ImageTexture); //fake atlas!
			String atlas_path = "res://atlas-" + md5 + ".tex";
			atlas->set_path(atlas_path);
			int idx = 0;
			for (List<StringName>::Element *F = atlas_images.front(); F; F = F->next()) {

				String p = F->get();
				Ref<AtlasTexture> atex = memnew(AtlasTexture);
				atex->set_atlas(atlas);
				Rect2 region = rects[idx++];
				Rect2 margin = rects[idx++];
				atex->set_region(region);
				atex->set_margin(margin);

				String path = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmpatlas.atex";
				Error err = ResourceSaver::save(path, atex);
				if (err != OK) {
					EditorNode::add_io_error(TTR("Could not save atlas subtexture:") + " " + path);
					return ERR_CANT_CREATE;
				}
				Vector<uint8_t> data = FileAccess::get_file_as_array(path);
				String dst_path = F->get().operator String().basename() + ".atex";
				err = p_func(p_udata, dst_path, data, counter++, files.size());
				saved.insert(dst_path);
				if (err)
					return err;

				if (f) {
					//recreating deps..
					String depline;
					//					depline=String(F->get())+"::"+itos(FileAccess::get_modified_time(F->get()))+"::"+FileAccess::get_md5(F->get()); name unneccesary by top md5
					depline = itos(FileAccess::get_modified_time(F->get())) + "::" + FileAccess::get_md5(F->get());
					depline += "::" + itos(region.pos.x) + "::" + itos(region.pos.y) + "::" + itos(region.size.x) + "::" + itos(region.size.y);
					depline += "::" + itos(margin.pos.x) + "::" + itos(margin.pos.y) + "::" + itos(margin.size.x) + "::" + itos(margin.size.y);
					f->store_line(depline);
				}

				remap_files[F->get()] = dst_path;
			}

			Vector<uint8_t> atlas_data = FileAccess::get_file_as_array(EditorSettings::get_singleton()->get_settings_path() + "/tmp/atlas-" + md5 + ".tex");
			Error err = p_func(p_udata, atlas_path, atlas_data, counter, files.size());
			saved.insert(atlas_path);
			if (err)
				return err;
		}

		if (f) {
			memdelete(f);
		}
	}

	StringName engine_cfg = "res://engine.cfg";
	StringName boot_splash;
	{
		String splash = Globals::get_singleton()->get("application/boot_splash"); //avoid splash from being converted
		splash = splash.strip_edges();
		if (splash != String()) {
			if (!splash.begins_with("res://"))
				splash = "res://" + splash;
			splash = splash.simplify_path();
			boot_splash = splash;
		}
	}

	for (int i = 0; i < files.size(); i++) {

		if (remap_files.has(files[i]) || files[i] == engine_cfg) //gonna be remapped (happened before!)
			continue; //from atlas?
		String src = files[i];
		Vector<uint8_t> buf;

		if (src == boot_splash)
			buf = get_exported_file_default(src); //bootsplash must be kept if used
		else
			buf = get_exported_file(src);

		ERR_CONTINUE(saved.has(src));

		Error err = p_func(p_udata, src, buf, counter++, files.size());
		if (err)
			return err;

		saved.insert(src);
		if (src != String(files[i]))
			remap_files[files[i]] = src;
	}

	{

		//make binary engine.cfg config
		Map<String, Variant> custom;

		if (remap_files.size()) {
			Vector<String> remapsprop;
			for (Map<StringName, StringName>::Element *E = remap_files.front(); E; E = E->next()) {
				remapsprop.push_back(E->key());
				remapsprop.push_back(E->get());
			}

			custom["remap/all"] = remapsprop;
		}

		//add presaved dependencies
		for (Map<StringName, List<StringName> >::Element *E = deps.front(); E; E = E->next()) {

			if (E->get().size() == 0)
				continue; //no deps
			String key;
			Vector<StringName> deps;
			//if bundle continue (when bundles supported obviously)

			if (remap_files.has(E->key())) {
				key = remap_files[E->key()];
			} else {
				key = E->key();
			}

			deps.resize(E->get().size());
			int i = 0;

			for (List<StringName>::Element *F = E->get().front(); F; F = F->next()) {
				deps[i++] = F->get();
				print_line(" -" + String(F->get()));
			}

			NodePath prop(deps, true, String()); //seems best to use this for performance

			custom["deps/" + key.md5_text()] = prop;
		}

		String remap_file = "engine.cfb";
		String engine_cfb = EditorSettings::get_singleton()->get_settings_path() + "/tmp/tmp" + remap_file;
		Globals::get_singleton()->save_custom(engine_cfb, custom);
		Vector<uint8_t> data = FileAccess::get_file_as_array(engine_cfb);

		Error err = p_func(p_udata, "res://" + remap_file, data, counter, files.size());
		if (err)
			return err;
	}

	return OK;
}

static int _get_pad(int p_alignment, int p_n) {

	int rest = p_n % p_alignment;
	int pad = 0;
	if (rest > 0) {
		pad = p_alignment - rest;
	};

	return pad;
};

void EditorExportPlatform::gen_export_flags(Vector<String> &r_flags, int p_flags) {

	String host = EditorSettings::get_singleton()->get("network/debug_host");
	int remote_port = (int)EditorSettings::get_singleton()->get("network/debug_port");

	if (p_flags & EXPORT_REMOTE_DEBUG_LOCALHOST)
		host = "localhost";

	if (p_flags & EXPORT_DUMB_CLIENT) {
		int port = EditorSettings::get_singleton()->get("file_server/port");
		String passwd = EditorSettings::get_singleton()->get("file_server/password");
		r_flags.push_back("-rfs");
		r_flags.push_back(host + ":" + itos(port));
		if (passwd != "") {
			r_flags.push_back("-rfs_pass");
			r_flags.push_back(passwd);
		}
	}

	if (p_flags & EXPORT_REMOTE_DEBUG) {

		r_flags.push_back("-rdebug");

		r_flags.push_back(host + ":" + String::num(remote_port));

		List<String> breakpoints;
		ScriptEditor::get_singleton()->get_breakpoints(&breakpoints);

		if (breakpoints.size()) {

			r_flags.push_back("-bp");
			String bpoints;
			for (const List<String>::Element *E = breakpoints.front(); E; E = E->next()) {

				bpoints += E->get().replace(" ", "%20");
				if (E->next())
					bpoints += ",";
			}

			r_flags.push_back(bpoints);
		}
	}

	if (p_flags & EXPORT_VIEW_COLLISONS) {

		r_flags.push_back("-debugcol");
	}

	if (p_flags & EXPORT_VIEW_NAVIGATION) {

		r_flags.push_back("-debugnav");
	}
}

Error EditorExportPlatform::save_pack_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {

	PackData *pd = (PackData *)p_userdata;

	CharString cs = p_path.utf8();
	pd->f->store_32(cs.length());
	pd->f->store_buffer((uint8_t *)cs.get_data(), cs.length());
	TempData td;
	td.pos = pd->f->get_pos();
	td.ofs = pd->ftmp->get_pos();
	td.size = p_data.size();
	pd->file_ofs.push_back(td);
	pd->f->store_64(0); //ofs
	pd->f->store_64(0); //size
	{
		MD5_CTX ctx;
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char *)p_data.ptr(), p_data.size());
		MD5Final(&ctx);
		pd->f->store_buffer(ctx.digest, 16);
	}
	pd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false);
	pd->count++;
	pd->ftmp->store_buffer(p_data.ptr(), p_data.size());
	if (pd->alignment > 1) {

		int pad = _get_pad(pd->alignment, pd->ftmp->get_pos());
		for (int i = 0; i < pad; i++) {

			pd->ftmp->store_8(0);
		};
	};
	return OK;
}

Error EditorExportPlatform::save_zip_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total) {

	String path = p_path.replace_first("res://", "");

	ZipData *zd = (ZipData *)p_userdata;

	zipFile zip = (zipFile)zd->zip;

	zipOpenNewFileInZip(zip,
			path.utf8().get_data(),
			NULL,
			NULL,
			0,
			NULL,
			0,
			NULL,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);

	zipWriteInFileInZip(zip, p_data.ptr(), p_data.size());
	zipCloseFileInZip(zip);

	zd->ep->step(TTR("Storing File:") + " " + p_path, 2 + p_file * 100 / p_total, false);
	zd->count++;
	return OK;
}

Error EditorExportPlatform::save_zip(const String &p_path, bool p_make_bundles) {

	EditorProgress ep("savezip", TTR("Packing"), 102);

	//FileAccess *tmp = FileAccess::open(tmppath,FileAccess::WRITE);

	FileAccess *src_f;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);
	zipFile zip = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, NULL, &io);

	ZipData zd;
	zd.count = 0;
	zd.ep = &ep;
	zd.zip = zip;

	Error err = export_project_files(save_zip_file, &zd, p_make_bundles);

	zipClose(zip, NULL);

	return err;
}

Error EditorExportPlatform::save_pack(FileAccess *dst, bool p_make_bundles, int p_alignment) {

	EditorProgress ep("savepack", TTR("Packing"), 102);

	String tmppath = EditorSettings::get_singleton()->get_settings_path() + "/tmp/packtmp";
	FileAccess *tmp = FileAccess::open(tmppath, FileAccess::WRITE);
	uint64_t ofs_begin = dst->get_pos();

	dst->store_32(0x43504447); //GDPK
	dst->store_32(0); //pack version
	dst->store_32(VERSION_MAJOR);
	dst->store_32(VERSION_MINOR);
	dst->store_32(0); //hmph
	for (int i = 0; i < 16; i++) {
		//reserved
		dst->store_32(0);
	}

	size_t fcountpos = dst->get_pos();
	dst->store_32(0);

	PackData pd;
	pd.ep = &ep;
	pd.f = dst;
	pd.ftmp = tmp;
	pd.count = 0;
	pd.alignment = p_alignment;
	Error err = export_project_files(save_pack_file, &pd, p_make_bundles);
	memdelete(tmp);
	if (err)
		return err;

	if (p_alignment > 1) {
		int pad = _get_pad(p_alignment, dst->get_pos());
		for (int i = 0; i < pad; i++) {

			dst->store_8(0);
		};
	};

	size_t ofsplus = dst->get_pos();
	//append file

	tmp = FileAccess::open(tmppath, FileAccess::READ);

	ERR_FAIL_COND_V(!tmp, ERR_CANT_OPEN;)
	const int bufsize = 16384;
	uint8_t buf[bufsize];

	while (true) {

		int got = tmp->get_buffer(buf, bufsize);
		if (got <= 0)
			break;
		dst->store_buffer(buf, got);
	}

	memdelete(tmp);

	dst->store_64(dst->get_pos() - ofs_begin);
	dst->store_32(0x43504447); //GDPK

	//fix offsets

	dst->seek(fcountpos);
	dst->store_32(pd.count);
	for (int i = 0; i < pd.file_ofs.size(); i++) {

		dst->seek(pd.file_ofs[i].pos);
		dst->store_64(pd.file_ofs[i].ofs + ofsplus);
		dst->store_64(pd.file_ofs[i].size);
	}

	return OK;
}

EditorExportPlatform::EditorExportPlatform() {

	debugging_enabled = true;
	chmod_flags = 0;
}

Error EditorExportPlatformPC::export_project(const String &p_path, bool p_debug, int p_flags) {

	EditorProgress ep("export", vformat(TTR("Exporting for %s"), get_name()), 102);

	const int BUFSIZE = 32768;

	ep.step(TTR("Setting Up.."), 0);

	String exe_path = "";

	if (p_debug)
		exe_path = custom_debug_binary;
	else
		exe_path = custom_release_binary;

	if (exe_path == "") {
		String fname;
		if (use64) {
			if (p_debug)
				fname = debug_binary64;
			else
				fname = release_binary64;
		} else {
			if (p_debug)
				fname = debug_binary32;
			else
				fname = release_binary32;
		}
		String err = "";
		exe_path = find_export_template(fname, &err);
		if (exe_path == "") {
			EditorNode::add_io_error(err);
			return ERR_FILE_CANT_READ;
		}
	}

	FileAccess *src_exe = FileAccess::open(exe_path, FileAccess::READ);
	if (!src_exe) {

		EditorNode::add_io_error("Couldn't read source executable at:\n " + exe_path);
		return ERR_FILE_CANT_READ;
	}

	FileAccess *dst = FileAccess::open(p_path, FileAccess::WRITE);
	if (!dst) {

		EditorNode::add_io_error("Can't copy executable file to:\n " + p_path);
		return ERR_FILE_CANT_WRITE;
	}

	uint8_t buff[32768];

	while (true) {

		int c = src_exe->get_buffer(buff, BUFSIZE);
		if (c > 0) {

			dst->store_buffer(buff, c);
		} else {
			break;
		}
	}

	String dstfile = p_path.replace_first("res://", "").replace("\\", "/");
	if (export_mode != EXPORT_EXE) {

		String dstfile_extension = export_mode == EXPORT_ZIP ? ".zip" : ".pck";
		if (dstfile.find("/") != -1)
			dstfile = dstfile.get_base_dir() + "/data" + dstfile_extension;
		else
			dstfile = "data" + dstfile_extension;
		if (export_mode == EXPORT_PACK) {

			memdelete(dst);

			dst = FileAccess::open(dstfile, FileAccess::WRITE);
			if (!dst) {

				EditorNode::add_io_error("Can't write data pack to:\n " + p_path);
				return ERR_FILE_CANT_WRITE;
			}
		}
	}

	memdelete(src_exe);

	Error err = export_mode == EXPORT_ZIP ? save_zip(dstfile, bundle) : save_pack(dst, bundle);
	dst->close();

	if (err == OK) {
		int flags = get_chmod_flags();
		if (flags) {
			err = dst->_chmod(p_path, flags);
			// If exporting from a platform with no chmod support (i.e., Windows), don't fail
			if (err == ERR_UNAVAILABLE)
				err = OK;
		}
	}

	memdelete(dst);
	return err;
}

void EditorExportPlatformPC::set_binary_extension(const String &p_extension) {

	binary_extension = p_extension;
}

bool EditorExportPlatformPC::can_export(String *r_error) const {

	String err;
	bool valid = true;

	if (use64 && (!exists_export_template(debug_binary64)) || !exists_export_template(release_binary64)) {
		valid = false;
		err = "No 64 bits export templates found.\nDownload and install export templates.\n";
	}

	if (!use64 && (!exists_export_template(debug_binary32) || !exists_export_template(release_binary32))) {
		valid = false;
		err = "No 32 bits export templates found.\nDownload and install export templates.\n";
	}

	if (custom_debug_binary == "" && custom_release_binary == "") {
		if (r_error) *r_error = err;
		return valid;
	}

	bool dvalid = true;
	bool rvalid = true;

	if (!FileAccess::exists(custom_debug_binary)) {
		dvalid = false;
		err = "Custom debug binary not found.\n";
	}

	if (!FileAccess::exists(custom_release_binary)) {
		rvalid = false;
		err = "Custom release binary not found.\n";
	}

	if (dvalid || rvalid)
		valid = true;
	else
		valid = false;

	if (r_error)
		*r_error = err;
	return valid;
}

EditorExportPlatformPC::EditorExportPlatformPC() {

	export_mode = EXPORT_PACK;
	use64 = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

EditorImportExport *EditorImportExport::singleton = NULL;

void EditorImportExport::add_import_plugin(const Ref<EditorImportPlugin> &p_plugin) {

	// Need to make sure the name is unique if we are going to lookup by it
	ERR_FAIL_COND(by_idx.has(p_plugin->get_name()));

	by_idx[p_plugin->get_name()] = plugins.size();
	plugins.push_back(p_plugin);
}

void EditorImportExport::remove_import_plugin(const Ref<EditorImportPlugin> &p_plugin) {

	String plugin_name = p_plugin->get_name();

	// Keep the indices the same
	// Find the index of the target plugin
	ERR_FAIL_COND(!by_idx.has(plugin_name));
	int idx = by_idx[plugin_name];
	int last_idx = plugins.size() - 1;

	// Swap the last plugin and the target one
	SWAP(plugins[idx], plugins[last_idx]);

	// Update the index of the old last one
	by_idx[plugins[idx]->get_name()] = idx;

	// Remove the target plugin's by_idx entry
	by_idx.erase(plugin_name);

	// Erase the plugin
	plugins.remove(last_idx);
}

int EditorImportExport::get_import_plugin_count() const {

	return plugins.size();
}
Ref<EditorImportPlugin> EditorImportExport::get_import_plugin(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, plugins.size(), Ref<EditorImportPlugin>());
	return plugins[p_idx];
}

Ref<EditorImportPlugin> EditorImportExport::get_import_plugin_by_name(const String &p_string) const {

	ERR_FAIL_COND_V(!by_idx.has(p_string), Ref<EditorImportPlugin>());
	return plugins[by_idx[p_string]];
}

void EditorImportExport::add_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {

	ERR_FAIL_COND(p_plugin.is_null());

	export_plugins.push_back(p_plugin);
}

void EditorImportExport::remove_export_plugin(const Ref<EditorExportPlugin> &p_plugin) {

	ERR_FAIL_COND(p_plugin.is_null());
	export_plugins.erase(p_plugin);
}

int EditorImportExport::get_export_plugin_count() const {

	return export_plugins.size();
}
Ref<EditorExportPlugin> EditorImportExport::get_export_plugin(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, export_plugins.size(), Ref<EditorExportPlugin>());
	return export_plugins[p_idx];
}

void EditorImportExport::set_export_file_action(const StringName &p_file, FileAction p_action) {

	if (p_action == ACTION_NONE) {

		files.erase(p_file);
	} else {

		files[p_file] = p_action;
	}
}

EditorImportExport::FileAction EditorImportExport::get_export_file_action(const StringName &p_file) const {

	if (files.has(p_file))
		return files[p_file];

	return ACTION_NONE;
}

void EditorImportExport::get_export_file_list(List<StringName> *p_files) {

	for (Map<StringName, FileAction>::Element *E = files.front(); E; E = E->next()) {

		p_files->push_back(E->key());
	}
}

void EditorImportExport::add_export_platform(const Ref<EditorExportPlatform> &p_export) {

	exporters[p_export->get_name()] = p_export;
}

void EditorImportExport::get_export_platforms(List<StringName> *r_platforms) {

	for (Map<StringName, Ref<EditorExportPlatform> >::Element *E = exporters.front(); E; E = E->next()) {

		r_platforms->push_back(E->key());
	}
}

Ref<EditorExportPlatform> EditorImportExport::get_export_platform(const StringName &p_platform) {

	if (exporters.has(p_platform)) {
		return exporters[p_platform];
	} else {
		return Ref<EditorExportPlatform>();
	}
}

bool EditorImportExport::poll_export_platforms() {

	bool changed = false;
	for (Map<StringName, Ref<EditorExportPlatform> >::Element *E = exporters.front(); E; E = E->next()) {

		if (E->get()->poll_devices())
			changed = true;
	}

	return changed;
}

void EditorImportExport::set_export_filter(ExportFilter p_enable) {

	export_filter = p_enable;
}

EditorImportExport::ExportFilter EditorImportExport::get_export_filter() const {

	return export_filter;
}

void EditorImportExport::set_export_custom_filter(const String &p_custom_filter) {
	export_custom_filter = p_custom_filter;
}
void EditorImportExport::set_export_custom_filter_exclude(const String &p_custom_filter) {
	export_custom_filter_exclude = p_custom_filter;
}
void EditorImportExport::set_export_custom_filter_exclude_dir(const String &p_custom_filter) {
	export_custom_filter_exclude_dir = p_custom_filter;
}
String EditorImportExport::get_export_custom_filter() const {
	return export_custom_filter;
}
String EditorImportExport::get_export_custom_filter_exclude() const {
	return export_custom_filter_exclude;
}
String EditorImportExport::get_export_custom_filter_exclude_dir() const {
	return export_custom_filter_exclude_dir;
}

void EditorImportExport::set_export_image_action(ImageAction p_action) {

	image_action = p_action;
}

EditorImportExport::ImageAction EditorImportExport::get_export_image_action() const {

	return image_action;
}

void EditorImportExport::set_export_image_shrink(float p_shrink) {

	image_shrink = p_shrink;
}

float EditorImportExport::get_export_image_shrink() const {

	return image_shrink;
}

void EditorImportExport::set_export_image_quality(float p_quality) {

	image_action_compress_quality = p_quality;
}

float EditorImportExport::get_export_image_quality() const {

	return image_action_compress_quality;
}

void EditorImportExport::image_export_group_create(const StringName &p_name) {

	ERR_FAIL_COND(image_groups.has(p_name));
	ImageGroup ig;
	ig.action = IMAGE_ACTION_NONE; //default
	ig.make_atlas = false;
	ig.shrink = 1;
	ig.lossy_quality = 0.7;
	image_groups[p_name] = ig;
}

bool EditorImportExport::image_export_has_group(const StringName &p_name) const {

	return image_groups.has(p_name);
}
void EditorImportExport::image_export_get_groups(List<StringName> *r_name) const {

	for (Map<StringName, ImageGroup>::Element *E = image_groups.front(); E; E = E->next()) {

		r_name->push_back(E->key());
	}
}

void EditorImportExport::image_export_group_remove(const StringName &p_name) {

	ERR_FAIL_COND(!image_groups.has(p_name));
	image_groups.erase(p_name);
}
void EditorImportExport::image_export_group_set_image_action(const StringName &p_export_group, ImageAction p_action) {

	ERR_FAIL_COND(!image_groups.has(p_export_group));
	image_groups[p_export_group].action = p_action;
}
EditorImportExport::ImageAction EditorImportExport::image_export_group_get_image_action(const StringName &p_export_group) const {

	ERR_FAIL_COND_V(!image_groups.has(p_export_group), IMAGE_ACTION_NONE);
	return image_groups[p_export_group].action;
}
void EditorImportExport::image_export_group_set_make_atlas(const StringName &p_export_group, bool p_make) {

	ERR_FAIL_COND(!image_groups.has(p_export_group));
	image_groups[p_export_group].make_atlas = p_make;
}
bool EditorImportExport::image_export_group_get_make_atlas(const StringName &p_export_group) const {

	ERR_FAIL_COND_V(!image_groups.has(p_export_group), false);
	return image_groups[p_export_group].make_atlas;
}
void EditorImportExport::image_export_group_set_shrink(const StringName &p_export_group, float p_amount) {
	ERR_FAIL_COND(!image_groups.has(p_export_group));
	image_groups[p_export_group].shrink = p_amount;
}
float EditorImportExport::image_export_group_get_shrink(const StringName &p_export_group) const {

	ERR_FAIL_COND_V(!image_groups.has(p_export_group), 1);
	return image_groups[p_export_group].shrink;
}

void EditorImportExport::image_export_group_set_lossy_quality(const StringName &p_export_group, float p_amount) {
	ERR_FAIL_COND(!image_groups.has(p_export_group));
	image_groups[p_export_group].lossy_quality = p_amount;
}
float EditorImportExport::image_export_group_get_lossy_quality(const StringName &p_export_group) const {

	ERR_FAIL_COND_V(!image_groups.has(p_export_group), 1);
	return image_groups[p_export_group].lossy_quality;
}

StringName EditorImportExport::image_get_export_group(const StringName &p_image) const {

	if (image_group_files.has(p_image))
		return image_group_files[p_image];
	else
		return StringName();
}

void EditorImportExport::image_add_to_export_group(const StringName &p_image, const StringName &p_export_group) {

	bool emptygroup = String(p_export_group) == String();
	ERR_FAIL_COND(!emptygroup && !image_groups.has(p_export_group));

	if (emptygroup)
		image_group_files.erase(p_image);
	else
		image_group_files[p_image] = p_export_group;
}

void EditorImportExport::image_export_get_images_in_group(const StringName &p_group, List<StringName> *r_images) const {

	for (Map<StringName, StringName>::Element *E = image_group_files.front(); E; E = E->next()) {

		if (p_group == E->get())
			r_images->push_back(E->key());
	}
}

void EditorImportExport::set_convert_text_scenes(bool p_convert) {

	convert_text_scenes = p_convert;
}

bool EditorImportExport::get_convert_text_scenes() const {

	return convert_text_scenes;
}

void EditorImportExport::load_config() {

	Ref<ConfigFile> cf = memnew(ConfigFile);

	Error err = cf->load("res://export.cfg");
	if (err != OK)
		return; //no export config to be loaded!

	export_custom_filter = cf->get_value("export_filter", "filter");
	export_custom_filter_exclude = cf->get_value("export_filter", "filter_exclude");
	export_custom_filter_exclude_dir = cf->get_value("export_filter", "filter_exclude_dir");
	String t = cf->get_value("export_filter", "type");
	if (t == "selected")
		export_filter = EXPORT_SELECTED;
	else if (t == "resources")
		export_filter = EXPORT_RESOURCES;
	else if (t == "all")
		export_filter = EXPORT_ALL;

	if (cf->has_section("convert_images")) {

		String ci = "convert_images";
		String action = cf->get_value(ci, "action");
		if (action == "none")
			image_action = IMAGE_ACTION_NONE;
		else if (action == "compress_ram")
			image_action = IMAGE_ACTION_COMPRESS_RAM;
		else if (action == "compress_disk")
			image_action = IMAGE_ACTION_COMPRESS_DISK;

		image_action_compress_quality = cf->get_value(ci, "compress_quality");
		if (cf->has_section_key(ci, "shrink"))
			image_shrink = cf->get_value(ci, "shrink");
		else
			image_shrink = 1;
		String formats = cf->get_value(ci, "formats");
		Vector<String> f = formats.split(",");
		image_formats.clear();
		for (int i = 0; i < f.size(); i++) {
			image_formats.insert(f[i].strip_edges());
		}
	}

	if (cf->has_section("convert_scenes")) {

		convert_text_scenes = cf->get_value("convert_scenes", "convert_text_scenes");
	}

	if (cf->has_section("export_filter_files")) {

		String eff = "export_filter_files";
		List<String> k;
		cf->get_section_keys(eff, &k);
		for (List<String>::Element *E = k.front(); E; E = E->next()) {

			String val = cf->get_value(eff, E->get());
			if (val == "copy") {
				files[E->get()] = ACTION_COPY;
			} else if (val == "bundle") {
				files[E->get()] = ACTION_BUNDLE;
			}
		}
	}

	List<String> sect;

	cf->get_sections(&sect);

	for (List<String>::Element *E = sect.front(); E; E = E->next()) {

		String s = E->get();
		if (!s.begins_with("platform:"))
			continue;
		String p = s.substr(s.find(":") + 1, s.length());

		if (!exporters.has(p))
			continue;

		Ref<EditorExportPlatform> ep = exporters[p];
		if (!ep.is_valid()) {
			continue;
		}
		List<String> keys;
		cf->get_section_keys(s, &keys);
		for (List<String>::Element *F = keys.front(); F; F = F->next()) {
			ep->set(F->get(), cf->get_value(s, F->get()));
		}
	}

	//save image groups

	if (cf->has_section("image_groups")) {

		sect.clear();
		cf->get_section_keys("image_groups", &sect);
		for (List<String>::Element *E = sect.front(); E; E = E->next()) {

			Dictionary d = cf->get_value("image_groups", E->get());
			ImageGroup g;
			g.action = IMAGE_ACTION_NONE;
			g.make_atlas = false;
			g.lossy_quality = 0.7;
			g.shrink = 1;

			if (d.has("action")) {
				String action = d["action"];
				if (action == "compress_ram")
					g.action = IMAGE_ACTION_COMPRESS_RAM;
				else if (action == "compress_disk")
					g.action = IMAGE_ACTION_COMPRESS_DISK;
				else if (action == "keep")
					g.action = IMAGE_ACTION_KEEP;
			}

			if (d.has("atlas"))
				g.make_atlas = d["atlas"];
			if (d.has("lossy_quality"))
				g.lossy_quality = d["lossy_quality"];
			if (d.has("shrink")) {

				g.shrink = d["shrink"];
				g.shrink = CLAMP(g.shrink, 1, 8);
			}

			image_groups[E->get()] = g;
		}

		if (cf->has_section_key("image_group_files", "files")) {

			Vector<String> sa = cf->get_value("image_group_files", "files");
			if (sa.size() % 2 == 0) {
				for (int i = 0; i < sa.size(); i += 2) {
					image_group_files[sa[i]] = sa[i + 1];
				}
			}
		}
	}

	if (cf->has_section("script")) {

		if (cf->has_section_key("script", "action")) {

			String action = cf->get_value("script", "action");
			if (action == "compile")
				script_action = SCRIPT_ACTION_COMPILE;
			else if (action == "encrypt")
				script_action = SCRIPT_ACTION_ENCRYPT;
			else
				script_action = SCRIPT_ACTION_NONE;
		}

		if (cf->has_section_key("script", "encrypt_key")) {

			script_key = cf->get_value("script", "encrypt_key");
		}
	}

	if (cf->has_section("convert_samples")) {

		if (cf->has_section_key("convert_samples", "action")) {
			String action = cf->get_value("convert_samples", "action");
			if (action == "none") {
				sample_action = SAMPLE_ACTION_NONE;
			} else if (action == "compress_ram") {
				sample_action = SAMPLE_ACTION_COMPRESS_RAM;
			}
		}

		if (cf->has_section_key("convert_samples", "max_hz"))
			sample_action_max_hz = cf->get_value("convert_samples", "max_hz");

		if (cf->has_section_key("convert_samples", "trim"))
			sample_action_trim = cf->get_value("convert_samples", "trim");
	}
}

void EditorImportExport::save_config() {

	Ref<ConfigFile> cf = memnew(ConfigFile);

	switch (export_filter) {
		case EXPORT_SELECTED: cf->set_value("export_filter", "type", "selected"); break;
		case EXPORT_RESOURCES: cf->set_value("export_filter", "type", "resources"); break;
		case EXPORT_ALL: cf->set_value("export_filter", "type", "all"); break;
	}

	cf->set_value("export_filter", "filter", export_custom_filter);
	cf->set_value("export_filter", "filter_exclude", export_custom_filter_exclude);
	cf->set_value("export_filter", "filter_exclude_dir", export_custom_filter_exclude_dir);

	String file_action_section = "export_filter_files";

	for (Map<StringName, FileAction>::Element *E = files.front(); E; E = E->next()) {

		String f = E->key();
		String a;
		switch (E->get()) {
			case ACTION_NONE: {
			}
			case ACTION_COPY: a = "copy"; break;
			case ACTION_BUNDLE: a = "bundle"; break;
		}

		cf->set_value(file_action_section, f, a);
	}

	for (Map<StringName, Ref<EditorExportPlatform> >::Element *E = exporters.front(); E; E = E->next()) {

		String pname = "platform:" + String(E->key());

		Ref<EditorExportPlatform> ep = E->get();

		List<PropertyInfo> pl;
		ep->get_property_list(&pl);

		for (List<PropertyInfo>::Element *F = pl.front(); F; F = F->next()) {

			cf->set_value(pname, F->get().name, ep->get(F->get().name));
		}
	}

	switch (image_action) {
		case IMAGE_ACTION_NONE: cf->set_value("convert_images", "action", "none"); break;
		case IMAGE_ACTION_COMPRESS_RAM: cf->set_value("convert_images", "action", "compress_ram"); break;
		case IMAGE_ACTION_COMPRESS_DISK: cf->set_value("convert_images", "action", "compress_disk"); break;
	}

	cf->set_value("convert_images", "shrink", image_shrink);
	cf->set_value("convert_images", "compress_quality", image_action_compress_quality);

	String formats;
	for (Set<String>::Element *E = image_formats.front(); E; E = E->next()) {

		if (E != image_formats.front())
			formats += ",";
		formats += E->get();
	}

	cf->set_value("convert_images", "formats", formats);

	//save image groups

	for (Map<StringName, ImageGroup>::Element *E = image_groups.front(); E; E = E->next()) {

		Dictionary d;
		switch (E->get().action) {
			case IMAGE_ACTION_NONE: d["action"] = "default"; break;
			case IMAGE_ACTION_COMPRESS_RAM: d["action"] = "compress_ram"; break;
			case IMAGE_ACTION_COMPRESS_DISK: d["action"] = "compress_disk"; break;
			case IMAGE_ACTION_KEEP: d["action"] = "keep"; break;
		}

		d["atlas"] = E->get().make_atlas;
		d["shrink"] = E->get().shrink;
		d["lossy_quality"] = E->get().lossy_quality;
		cf->set_value("image_groups", E->key(), d);
	}

	if (image_groups.size() && image_group_files.size()) {

		Vector<String> igfkeys;
		igfkeys.resize(image_group_files.size());
		int idx = 0;
		for (Map<StringName, StringName>::Element *E = image_group_files.front(); E; E = E->next()) {
			igfkeys[idx++] = E->key();
		}
		igfkeys.sort();

		Vector<String> igfsave;
		igfsave.resize(image_group_files.size() * 2);
		idx = 0;
		for (int i = 0; i < igfkeys.size(); ++i) {

			igfsave[idx++] = igfkeys[i];
			igfsave[idx++] = image_group_files[igfkeys[i]];
		}
		cf->set_value("image_group_files", "files", igfsave);
	}

	switch (script_action) {
		case SCRIPT_ACTION_NONE: cf->set_value("script", "action", "none"); break;
		case SCRIPT_ACTION_COMPILE: cf->set_value("script", "action", "compile"); break;
		case SCRIPT_ACTION_ENCRYPT: cf->set_value("script", "action", "encrypt"); break;
	}

	cf->set_value("convert_scenes", "convert_text_scenes", convert_text_scenes);

	cf->set_value("script", "encrypt_key", script_key);

	switch (sample_action) {
		case SAMPLE_ACTION_NONE: cf->set_value("convert_samples", "action", "none"); break;
		case SAMPLE_ACTION_COMPRESS_RAM: cf->set_value("convert_samples", "action", "compress_ram"); break;
	}

	cf->set_value("convert_samples", "max_hz", sample_action_max_hz);
	cf->set_value("convert_samples", "trim", sample_action_trim);

	cf->save("res://export.cfg");
}

void EditorImportExport::script_set_action(ScriptAction p_action) {

	script_action = p_action;
}

EditorImportExport::ScriptAction EditorImportExport::script_get_action() const {

	return script_action;
}

void EditorImportExport::script_set_encryption_key(const String &p_key) {

	script_key = p_key;
}
String EditorImportExport::script_get_encryption_key() const {

	return script_key;
}

void EditorImportExport::sample_set_action(SampleAction p_action) {

	sample_action = p_action;
}

EditorImportExport::SampleAction EditorImportExport::sample_get_action() const {

	return sample_action;
}

void EditorImportExport::sample_set_max_hz(int p_hz) {

	sample_action_max_hz = p_hz;
}
int EditorImportExport::sample_get_max_hz() const {

	return sample_action_max_hz;
}

void EditorImportExport::sample_set_trim(bool p_trim) {

	sample_action_trim = p_trim;
}
bool EditorImportExport::sample_get_trim() const {

	return sample_action_trim;
}

DVector<String> EditorImportExport::_get_export_file_list() {

	DVector<String> fl;
	for (Map<StringName, FileAction>::Element *E = files.front(); E; E = E->next()) {

		fl.push_back(E->key());
	}

	return fl;
}

DVector<String> EditorImportExport::_get_export_platforms() {

	DVector<String> ep;
	for (Map<StringName, Ref<EditorExportPlatform> >::Element *E = exporters.front(); E; E = E->next()) {

		ep.push_back(E->key());
	}

	return ep;
}

void EditorImportExport::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_import_plugin", "plugin:EditorImportPlugin"), &EditorImportExport::add_import_plugin);
	ObjectTypeDB::bind_method(_MD("remove_import_plugin", "plugin:EditorImportPlugin"), &EditorImportExport::remove_import_plugin);
	ObjectTypeDB::bind_method(_MD("get_import_plugin_count"), &EditorImportExport::get_import_plugin_count);
	ObjectTypeDB::bind_method(_MD("get_import_plugin:EditorImportPlugin", "idx"), &EditorImportExport::get_import_plugin);
	ObjectTypeDB::bind_method(_MD("get_import_plugin_by_name:EditorImportPlugin", "name"), &EditorImportExport::get_import_plugin_by_name);

	ObjectTypeDB::bind_method(_MD("add_export_plugin", "plugin:EditorExportPlugin"), &EditorImportExport::add_export_plugin);
	ObjectTypeDB::bind_method(_MD("remove_export_plugin", "plugin:EditorExportPlugin"), &EditorImportExport::remove_export_plugin);
	ObjectTypeDB::bind_method(_MD("get_export_plugin_count"), &EditorImportExport::get_export_plugin_count);
	ObjectTypeDB::bind_method(_MD("get_export_plugin:EditorExportPlugin", "idx"), &EditorImportExport::get_export_plugin);

	ObjectTypeDB::bind_method(_MD("set_export_file_action", "file", "action"), &EditorImportExport::set_export_file_action);
	ObjectTypeDB::bind_method(_MD("get_export_file_action", "file"), &EditorImportExport::get_export_file_action);
	ObjectTypeDB::bind_method(_MD("get_export_file_list"), &EditorImportExport::_get_export_file_list);

	ObjectTypeDB::bind_method(_MD("add_export_platform", "platform:EditorExportplatform"), &EditorImportExport::add_export_platform);
	//ObjectTypeDB::bind_method(_MD("remove_export_platform","platform:EditorExportplatform"),&EditorImportExport::add_export_platform);
	ObjectTypeDB::bind_method(_MD("get_export_platform:EditorExportPlatform", "name"), &EditorImportExport::get_export_platform);
	ObjectTypeDB::bind_method(_MD("get_export_platforms"), &EditorImportExport::_get_export_platforms);

	ObjectTypeDB::bind_method(_MD("set_export_filter", "filter"), &EditorImportExport::set_export_filter);
	ObjectTypeDB::bind_method(_MD("get_export_filter"), &EditorImportExport::get_export_filter);

	ObjectTypeDB::bind_method(_MD("set_export_custom_filter", "filter"), &EditorImportExport::set_export_custom_filter);
	ObjectTypeDB::bind_method(_MD("get_export_custom_filter"), &EditorImportExport::get_export_custom_filter);

	ObjectTypeDB::bind_method(_MD("set_export_custom_filter_exclude", "filter_exclude"), &EditorImportExport::set_export_custom_filter_exclude);
	ObjectTypeDB::bind_method(_MD("get_export_custom_filter_exclude"), &EditorImportExport::get_export_custom_filter_exclude);

	ObjectTypeDB::bind_method(_MD("set_export_custom_filter_exclude_dir", "filter_exclude_dir"), &EditorImportExport::set_export_custom_filter_exclude_dir);
	ObjectTypeDB::bind_method(_MD("get_export_custom_filter_exclude_dir"), &EditorImportExport::get_export_custom_filter_exclude_dir);

	ObjectTypeDB::bind_method(_MD("image_export_group_create"), &EditorImportExport::image_export_group_create);
	ObjectTypeDB::bind_method(_MD("image_export_group_remove"), &EditorImportExport::image_export_group_remove);
	ObjectTypeDB::bind_method(_MD("image_export_group_set_image_action"), &EditorImportExport::image_export_group_set_image_action);
	ObjectTypeDB::bind_method(_MD("image_export_group_set_make_atlas"), &EditorImportExport::image_export_group_set_make_atlas);
	ObjectTypeDB::bind_method(_MD("image_export_group_set_shrink"), &EditorImportExport::image_export_group_set_shrink);
	ObjectTypeDB::bind_method(_MD("image_export_group_get_image_action"), &EditorImportExport::image_export_group_get_image_action);
	ObjectTypeDB::bind_method(_MD("image_export_group_get_make_atlas"), &EditorImportExport::image_export_group_get_make_atlas);
	ObjectTypeDB::bind_method(_MD("image_export_group_get_shrink"), &EditorImportExport::image_export_group_get_shrink);
	ObjectTypeDB::bind_method(_MD("image_add_to_export_group"), &EditorImportExport::image_add_to_export_group);
	ObjectTypeDB::bind_method(_MD("script_set_action"), &EditorImportExport::script_set_action);
	ObjectTypeDB::bind_method(_MD("script_set_encryption_key"), &EditorImportExport::script_set_encryption_key);
	ObjectTypeDB::bind_method(_MD("script_get_action"), &EditorImportExport::script_get_action);
	ObjectTypeDB::bind_method(_MD("script_get_encryption_key"), &EditorImportExport::script_get_encryption_key);

	BIND_CONSTANT(ACTION_NONE);
	BIND_CONSTANT(ACTION_COPY);
	BIND_CONSTANT(ACTION_BUNDLE);

	BIND_CONSTANT(EXPORT_SELECTED);
	BIND_CONSTANT(EXPORT_RESOURCES);
	BIND_CONSTANT(EXPORT_ALL);

	BIND_CONSTANT(IMAGE_ACTION_NONE);
	BIND_CONSTANT(IMAGE_ACTION_COMPRESS_DISK);
	BIND_CONSTANT(IMAGE_ACTION_COMPRESS_RAM);
	BIND_CONSTANT(IMAGE_ACTION_KEEP);

	BIND_CONSTANT(SCRIPT_ACTION_NONE);
	BIND_CONSTANT(SCRIPT_ACTION_COMPILE);
	BIND_CONSTANT(SCRIPT_ACTION_ENCRYPT);
};

EditorImportExport::EditorImportExport() {

	export_filter = EXPORT_RESOURCES;
	singleton = this;
	image_action = IMAGE_ACTION_NONE;
	image_action_compress_quality = 0.7;
	image_formats.insert("png");
	image_shrink = 1;

	script_action = SCRIPT_ACTION_COMPILE;

	sample_action = SAMPLE_ACTION_NONE;
	sample_action_max_hz = 44100;
	sample_action_trim = false;

	convert_text_scenes = true;
}

EditorImportExport::~EditorImportExport() {
}
