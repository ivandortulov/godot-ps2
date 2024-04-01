/*************************************************************************/
/*  resource_loader.cpp                                                  */
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
#include "resource_loader.h"
#include "globals.h"
#include "os/file_access.h"
#include "os/os.h"
#include "path_remap.h"
#include "print_string.h"
ResourceFormatLoader *ResourceLoader::loader[MAX_LOADERS];

int ResourceLoader::loader_count = 0;

Error ResourceInteractiveLoader::wait() {

	Error err = poll();
	while (err == OK) {
		err = poll();
	}

	return err;
}

bool ResourceFormatLoader::recognize(const String &p_extension) const {

	List<String> extensions;
	get_recognized_extensions(&extensions);
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {

		if (E->get().nocasecmp_to(p_extension.extension()) == 0)
			return true;
	}

	return false;
}

void ResourceFormatLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {

	if (p_type == "" || handles_type(p_type))
		get_recognized_extensions(p_extensions);
}

void ResourceLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) {

	for (int i = 0; i < loader_count; i++) {
		loader[i]->get_recognized_extensions_for_type(p_type, p_extensions);
	}
}

void ResourceInteractiveLoader::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_resource"), &ResourceInteractiveLoader::get_resource);
	ObjectTypeDB::bind_method(_MD("poll"), &ResourceInteractiveLoader::poll);
	ObjectTypeDB::bind_method(_MD("wait"), &ResourceInteractiveLoader::wait);
	ObjectTypeDB::bind_method(_MD("get_stage"), &ResourceInteractiveLoader::get_stage);
	ObjectTypeDB::bind_method(_MD("get_stage_count"), &ResourceInteractiveLoader::get_stage_count);
}

class ResourceInteractiveLoaderDefault : public ResourceInteractiveLoader {

	OBJ_TYPE(ResourceInteractiveLoaderDefault, ResourceInteractiveLoader);

public:
	Ref<Resource> resource;

	virtual void set_local_path(const String &p_local_path) { /*scene->set_filename(p_local_path);*/
	}
	virtual Ref<Resource> get_resource() { return resource; }
	virtual Error poll() { return ERR_FILE_EOF; }
	virtual int get_stage() const { return 1; }
	virtual int get_stage_count() const { return 1; }

	ResourceInteractiveLoaderDefault() {}
};

Ref<ResourceInteractiveLoader> ResourceFormatLoader::load_interactive(const String &p_path, Error *r_error) {

	//either this
	Ref<Resource> res = load(p_path, p_path, r_error);
	if (res.is_null())
		return Ref<ResourceInteractiveLoader>();

	Ref<ResourceInteractiveLoaderDefault> ril = Ref<ResourceInteractiveLoaderDefault>(memnew(ResourceInteractiveLoaderDefault));
	ril->resource = res;
	return ril;
}

RES ResourceFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error) {

	String path = p_path;

	//or this must be implemented
	Ref<ResourceInteractiveLoader> ril = load_interactive(p_path, r_error);
	if (!ril.is_valid())
		return RES();
	ril->set_local_path(p_original_path);

	while (true) {

		Error err = ril->poll();

		if (err == ERR_FILE_EOF) {
			if (r_error)
				*r_error = OK;
			return ril->get_resource();
		}

		if (r_error)
			*r_error = err;

		ERR_FAIL_COND_V(err != OK, RES());
	}

	return RES();
}

void ResourceFormatLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {

	//do nothing by default
}

///////////////////////////////////

RES ResourceLoader::load(const String &p_path, const String &p_type_hint, bool p_no_cache, Error *r_error) {

	if (r_error)
		*r_error = ERR_CANT_OPEN;

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	local_path = find_complete_path(local_path, p_type_hint);
	ERR_FAIL_COND_V(local_path == "", RES());

	if (!p_no_cache && ResourceCache::has(local_path)) {

		if (OS::get_singleton()->is_stdout_verbose())
			print_line("load resource: " + local_path + " (cached)");

		return RES(ResourceCache::get(local_path));
	}

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);

	if (OS::get_singleton()->is_stdout_verbose())
		print_line("load resource: " + remapped_path);

	String extension = remapped_path.extension();
	bool found = false;

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;
		if (p_type_hint != "" && !loader[i]->handles_type(p_type_hint))
			continue;
		found = true;
		RES res = loader[i]->load(remapped_path, local_path, r_error);
		if (res.is_null())
			continue;
		if (!p_no_cache)
			res->set_path(local_path);
#ifdef TOOLS_ENABLED

		res->set_edited(false);
		if (timestamp_on_load) {
			uint64_t mt = FileAccess::get_modified_time(remapped_path);
			//printf("mt %s: %lli\n",remapped_path.utf8().get_data(),mt);
			res->set_last_modified_time(mt);
		}
#endif

		return res;
	}

	if (found) {
		ERR_EXPLAIN("Failed loading resource: " + p_path);
	} else {
		ERR_EXPLAIN("No loader found for resource: " + p_path);
	}
	ERR_FAIL_V(RES());
	return RES();
}

Ref<ResourceImportMetadata> ResourceLoader::load_import_metadata(const String &p_path) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	String extension = p_path.extension();
	Ref<ResourceImportMetadata> ret;

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;

		Error err = loader[i]->load_import_metadata(local_path, ret);
		if (err == OK)
			break;
	}

	return ret;
}

String ResourceLoader::find_complete_path(const String &p_path, const String &p_type) {
	//this is an old vestige when the engine saved files without extension.
	//remains here for compatibility with old projects and only because it
	//can be sometimes nice to open files using .* from a script and have it guess
	//the right extension.

	String local_path = p_path;
	if (local_path.ends_with("*")) {

		//find the extension for resource that ends with *
		local_path = local_path.substr(0, local_path.length() - 1);
		List<String> extensions;
		get_recognized_extensions_for_type(p_type, &extensions);
		List<String> candidates;

		for (List<String>::Element *E = extensions.front(); E; E = E->next()) {

			String path = local_path + E->get();

			if (PathRemap::get_singleton()->has_remap(path) || FileAccess::exists(path)) {
				candidates.push_back(path);
			}
		}

		if (candidates.size() == 0) {
			return "";
		} else if (candidates.size() == 1 || p_type == "") {
			return candidates.front()->get();
		} else {

			for (List<String>::Element *E = candidates.front(); E; E = E->next()) {

				String rt = get_resource_type(E->get());
				if (ObjectTypeDB::is_type(rt, p_type)) {
					return E->get();
				}
			}

			return "";
		}
	}

	return local_path;
}

Ref<ResourceInteractiveLoader> ResourceLoader::load_interactive(const String &p_path, const String &p_type_hint, bool p_no_cache, Error *r_error) {

	if (r_error)
		*r_error = ERR_CANT_OPEN;

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	local_path = find_complete_path(local_path, p_type_hint);
	ERR_FAIL_COND_V(local_path == "", Ref<ResourceInteractiveLoader>());

	if (!p_no_cache && ResourceCache::has(local_path)) {

		if (OS::get_singleton()->is_stdout_verbose())
			print_line("load resource: " + local_path + " (cached)");

		Ref<Resource> res_cached = ResourceCache::get(local_path);
		Ref<ResourceInteractiveLoaderDefault> ril = Ref<ResourceInteractiveLoaderDefault>(memnew(ResourceInteractiveLoaderDefault));

		ril->resource = res_cached;
		return ril;
	}

	if (OS::get_singleton()->is_stdout_verbose())
		print_line("load resource: ");

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);

	String extension = remapped_path.extension();
	bool found = false;

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;
		if (p_type_hint != "" && !loader[i]->handles_type(p_type_hint))
			continue;
		found = true;
		Ref<ResourceInteractiveLoader> ril = loader[i]->load_interactive(remapped_path, r_error);
		if (ril.is_null())
			continue;
		if (!p_no_cache)
			ril->set_local_path(local_path);

		return ril;
	}

	if (found) {
		ERR_EXPLAIN("Failed loading resource: " + p_path);
	} else {
		ERR_EXPLAIN("No loader found for resource: " + p_path);
	}
	ERR_FAIL_V(Ref<ResourceInteractiveLoader>());
	return Ref<ResourceInteractiveLoader>();
}

void ResourceLoader::add_resource_format_loader(ResourceFormatLoader *p_format_loader, bool p_at_front) {

	ERR_FAIL_COND(loader_count >= MAX_LOADERS);
	if (p_at_front) {
		for (int i = loader_count; i > 0; i--) {
			loader[i] = loader[i - 1];
		}
		loader[0] = p_format_loader;
		loader_count++;
	} else {
		loader[loader_count++] = p_format_loader;
	}
}

void ResourceLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);

	String extension = remapped_path.extension();

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;
		//if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
		//	continue;

		loader[i]->get_dependencies(remapped_path, p_dependencies, p_add_types);
	}
}

Error ResourceLoader::get_export_data(const String &p_path, ExportData &r_export_data) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);

	String extension = remapped_path.extension();

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;
		//if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
		//	continue;

		return loader[i]->get_export_data(p_path, r_export_data);
	}

	return ERR_UNAVAILABLE;
}

Error ResourceLoader::rename_dependencies(const String &p_path, const Map<String, String> &p_map) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);

	String extension = remapped_path.extension();

	for (int i = 0; i < loader_count; i++) {

		if (!loader[i]->recognize(extension))
			continue;
		//if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
		//	continue;

		return loader[i]->rename_dependencies(p_path, p_map);
	}

	return OK; // ??
}

String ResourceLoader::guess_full_filename(const String &p_path, const String &p_type) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	return find_complete_path(local_path, p_type);
}

String ResourceLoader::get_resource_type(const String &p_path) {

	String local_path;
	if (p_path.is_rel_path())
		local_path = "res://" + p_path;
	else
		local_path = Globals::get_singleton()->localize_path(p_path);

	String remapped_path = PathRemap::get_singleton()->get_remap(local_path);
	String extension = remapped_path.extension();

	for (int i = 0; i < loader_count; i++) {

		String result = loader[i]->get_resource_type(local_path);
		if (result != "")
			return result;
	}

	return "";
}
ResourceLoadErrorNotify ResourceLoader::err_notify = NULL;
void *ResourceLoader::err_notify_ud = NULL;

DependencyErrorNotify ResourceLoader::dep_err_notify = NULL;
void *ResourceLoader::dep_err_notify_ud = NULL;

bool ResourceLoader::abort_on_missing_resource = true;
bool ResourceLoader::timestamp_on_load = false;
