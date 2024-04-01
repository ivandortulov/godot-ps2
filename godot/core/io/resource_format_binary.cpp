/*************************************************************************/
/*  resource_format_binary.cpp                                           */
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
#include "resource_format_binary.h"
#include "globals.h"
#include "io/file_access_compressed.h"
#include "io/marshalls.h"
#include "os/dir_access.h"
#include "version.h"
//#define print_bl(m_what) print_line(m_what)
#define print_bl(m_what)

enum {

	//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
	VARIANT_NIL = 1,
	VARIANT_BOOL = 2,
	VARIANT_INT = 3,
	VARIANT_REAL = 4,
	VARIANT_STRING = 5,
	VARIANT_VECTOR2 = 10,
	VARIANT_RECT2 = 11,
	VARIANT_VECTOR3 = 12,
	VARIANT_PLANE = 13,
	VARIANT_QUAT = 14,
	VARIANT_AABB = 15,
	VARIANT_MATRIX3 = 16,
	VARIANT_TRANSFORM = 17,
	VARIANT_MATRIX32 = 18,
	VARIANT_COLOR = 20,
	VARIANT_IMAGE = 21,
	VARIANT_NODE_PATH = 22,
	VARIANT_RID = 23,
	VARIANT_OBJECT = 24,
	VARIANT_INPUT_EVENT = 25,
	VARIANT_DICTIONARY = 26,
	VARIANT_ARRAY = 30,
	VARIANT_RAW_ARRAY = 31,
	VARIANT_INT_ARRAY = 32,
	VARIANT_REAL_ARRAY = 33,
	VARIANT_STRING_ARRAY = 34,
	VARIANT_VECTOR3_ARRAY = 35,
	VARIANT_COLOR_ARRAY = 36,
	VARIANT_VECTOR2_ARRAY = 37,

	IMAGE_ENCODING_EMPTY = 0,
	IMAGE_ENCODING_RAW = 1,
	IMAGE_ENCODING_LOSSLESS = 2,
	IMAGE_ENCODING_LOSSY = 3,

	IMAGE_FORMAT_GRAYSCALE = 0,
	IMAGE_FORMAT_INTENSITY = 1,
	IMAGE_FORMAT_GRAYSCALE_ALPHA = 2,
	IMAGE_FORMAT_RGB = 3,
	IMAGE_FORMAT_RGBA = 4,
	IMAGE_FORMAT_INDEXED = 5,
	IMAGE_FORMAT_INDEXED_ALPHA = 6,
	IMAGE_FORMAT_BC1 = 7,
	IMAGE_FORMAT_BC2 = 8,
	IMAGE_FORMAT_BC3 = 9,
	IMAGE_FORMAT_BC4 = 10,
	IMAGE_FORMAT_BC5 = 11,
	IMAGE_FORMAT_PVRTC2 = 12,
	IMAGE_FORMAT_PVRTC2_ALPHA = 13,
	IMAGE_FORMAT_PVRTC4 = 14,
	IMAGE_FORMAT_PVRTC4_ALPHA = 15,
	IMAGE_FORMAT_ETC = 16,
	IMAGE_FORMAT_ATC = 17,
	IMAGE_FORMAT_ATC_ALPHA_EXPLICIT = 18,
	IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED = 19,
	IMAGE_FORMAT_CUSTOM = 30,

	OBJECT_EMPTY = 0,
	OBJECT_EXTERNAL_RESOURCE = 1,
	OBJECT_INTERNAL_RESOURCE = 2,
	OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
	FORMAT_VERSION = 1,
	FORMAT_VERSION_CAN_RENAME_DEPS = 1

};

void ResourceInteractiveLoaderBinary::_advance_padding(uint32_t p_len) {

	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++)
			f->get_8(); //pad to 32
	}
}

Error ResourceInteractiveLoaderBinary::parse_variant(Variant &r_v, bool p_for_export_data) {

	uint32_t type = f->get_32();
	print_bl("find property of type: " + itos(type));

	switch (type) {

		case VARIANT_NIL: {

			r_v = Variant();
		} break;
		case VARIANT_BOOL: {

			r_v = bool(f->get_32());
		} break;
		case VARIANT_INT: {

			r_v = int(f->get_32());
		} break;
		case VARIANT_REAL: {

			r_v = f->get_real();
		} break;
		case VARIANT_STRING: {

			r_v = get_unicode_string();
		} break;
		case VARIANT_VECTOR2: {

			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_RECT2: {

			Rect2 v;
			v.pos.x = f->get_real();
			v.pos.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_VECTOR3: {

			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_PLANE: {

			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VARIANT_QUAT: {
			Quat v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VARIANT_AABB: {

			AABB v;
			v.pos.x = f->get_real();
			v.pos.y = f->get_real();
			v.pos.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_MATRIX32: {

			Matrix32 v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_MATRIX3: {

			Matrix3 v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[0].z = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[1].z = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			v.elements[2].z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM: {

			Transform v;
			v.basis.elements[0].x = f->get_real();
			v.basis.elements[0].y = f->get_real();
			v.basis.elements[0].z = f->get_real();
			v.basis.elements[1].x = f->get_real();
			v.basis.elements[1].y = f->get_real();
			v.basis.elements[1].z = f->get_real();
			v.basis.elements[2].x = f->get_real();
			v.basis.elements[2].y = f->get_real();
			v.basis.elements[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_COLOR: {

			Color v;
			v.r = f->get_real();
			v.g = f->get_real();
			v.b = f->get_real();
			v.a = f->get_real();
			r_v = v;

		} break;
		case VARIANT_IMAGE: {

			uint32_t encoding = f->get_32();
			if (encoding == IMAGE_ENCODING_EMPTY) {
				r_v = Variant();
				break;
			} else if (encoding == IMAGE_ENCODING_RAW) {
				uint32_t width = f->get_32();
				uint32_t height = f->get_32();
				uint32_t mipmaps = f->get_32();
				uint32_t format = f->get_32();
				Image::Format fmt;
				switch (format) {

					case IMAGE_FORMAT_GRAYSCALE: {
						fmt = Image::FORMAT_GRAYSCALE;
					} break;
					case IMAGE_FORMAT_INTENSITY: {
						fmt = Image::FORMAT_INTENSITY;
					} break;
					case IMAGE_FORMAT_GRAYSCALE_ALPHA: {
						fmt = Image::FORMAT_GRAYSCALE_ALPHA;
					} break;
					case IMAGE_FORMAT_RGB: {
						fmt = Image::FORMAT_RGB;
					} break;
					case IMAGE_FORMAT_RGBA: {
						fmt = Image::FORMAT_RGBA;
					} break;
					case IMAGE_FORMAT_INDEXED: {
						fmt = Image::FORMAT_INDEXED;
					} break;
					case IMAGE_FORMAT_INDEXED_ALPHA: {
						fmt = Image::FORMAT_INDEXED_ALPHA;
					} break;
					case IMAGE_FORMAT_BC1: {
						fmt = Image::FORMAT_BC1;
					} break;
					case IMAGE_FORMAT_BC2: {
						fmt = Image::FORMAT_BC2;
					} break;
					case IMAGE_FORMAT_BC3: {
						fmt = Image::FORMAT_BC3;
					} break;
					case IMAGE_FORMAT_BC4: {
						fmt = Image::FORMAT_BC4;
					} break;
					case IMAGE_FORMAT_BC5: {
						fmt = Image::FORMAT_BC5;
					} break;
					case IMAGE_FORMAT_PVRTC2: {
						fmt = Image::FORMAT_PVRTC2;
					} break;
					case IMAGE_FORMAT_PVRTC2_ALPHA: {
						fmt = Image::FORMAT_PVRTC2_ALPHA;
					} break;
					case IMAGE_FORMAT_PVRTC4: {
						fmt = Image::FORMAT_PVRTC4;
					} break;
					case IMAGE_FORMAT_PVRTC4_ALPHA: {
						fmt = Image::FORMAT_PVRTC4_ALPHA;
					} break;
					case IMAGE_FORMAT_ETC: {
						fmt = Image::FORMAT_ETC;
					} break;
					case IMAGE_FORMAT_ATC: {
						fmt = Image::FORMAT_ATC;
					} break;
					case IMAGE_FORMAT_ATC_ALPHA_EXPLICIT: {
						fmt = Image::FORMAT_ATC_ALPHA_EXPLICIT;
					} break;
					case IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED: {
						fmt = Image::FORMAT_ATC_ALPHA_INTERPOLATED;
					} break;
					case IMAGE_FORMAT_CUSTOM: {
						fmt = Image::FORMAT_CUSTOM;
					} break;
					default: {

						ERR_FAIL_V(ERR_FILE_CORRUPT);
					}
				}

				uint32_t datalen = f->get_32();

				DVector<uint8_t> imgdata;
				imgdata.resize(datalen);
				DVector<uint8_t>::Write w = imgdata.write();
				f->get_buffer(w.ptr(), datalen);
				_advance_padding(datalen);
				w = DVector<uint8_t>::Write();

				r_v = Image(width, height, mipmaps, fmt, imgdata);

			} else {
				//compressed
				DVector<uint8_t> data;
				data.resize(f->get_32());
				DVector<uint8_t>::Write w = data.write();
				f->get_buffer(w.ptr(), data.size());
				w = DVector<uint8_t>::Write();

				Image img;

				if (encoding == IMAGE_ENCODING_LOSSY && Image::lossy_unpacker) {

					img = Image::lossy_unpacker(data);
				} else if (encoding == IMAGE_ENCODING_LOSSLESS && Image::lossless_unpacker) {

					img = Image::lossless_unpacker(data);
				}
				_advance_padding(data.size());

				r_v = img;
			}

		} break;
		case VARIANT_NODE_PATH: {

			Vector<StringName> names;
			Vector<StringName> subnames;
			StringName property;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;

			for (int i = 0; i < name_count; i++)
				names.push_back(string_map[f->get_32()]);
			for (uint32_t i = 0; i < subname_count; i++)
				subnames.push_back(string_map[f->get_32()]);
			property = string_map[f->get_32()];

			NodePath np = NodePath(names, subnames, absolute, property);
			//print_line("got path: "+String(np));

			r_v = np;

		} break;
		case VARIANT_RID: {

			r_v = f->get_32();
		} break;
		case VARIANT_OBJECT: {

			uint32_t type = f->get_32();

			switch (type) {

				case OBJECT_EMPTY: {
					//do none

				} break;
				case OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();

					if (p_for_export_data) {

						r_v = "@RESLOCAL:" + itos(index);
					} else {
						String path = res_path + "::" + itos(index);
						RES res = ResourceLoader::load(path);
						if (res.is_null()) {
							WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
						}
						r_v = res;
					}

				} break;
				case OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String type = get_unicode_string();
					String path = get_unicode_string();

					if (p_for_export_data) {

						r_v = "@RESPATH:" + type + ":" + path;
					} else {

						if (path.find("://") == -1 && path.is_rel_path()) {
							// path is relative to file being loaded, so convert to a resource path
							path = Globals::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
						}

						if (remaps.find(path)) {
							path = remaps[path];
						}

						RES res = ResourceLoader::load(path, type);

						if (res.is_null()) {
							WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
						}
						r_v = res;
					}

				} break;
				case OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					uint32_t erindex = f->get_32();

					if (p_for_export_data) {
						r_v = "@RESEXTERNAL:" + itos(erindex);
					} else {
						if (erindex >= external_resources.size()) {
							WARN_PRINT("Broken external resource! (index out of size");
							r_v = Variant();
						} else {

							String type = external_resources[erindex].type;
							String path = external_resources[erindex].path;

							if (path.find("://") == -1 && path.is_rel_path()) {
								// path is relative to file being loaded, so convert to a resource path
								path = Globals::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
							}

							RES res = ResourceLoader::load(path, type);

							if (res.is_null()) {
								WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
							}
							r_v = res;
						}
					}

				} break;
				default: {

					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}

		} break;
		case VARIANT_INPUT_EVENT: {

		} break;
		case VARIANT_DICTIONARY: {

			uint32_t len = f->get_32();
			Dictionary d(len & 0x80000000); //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variant(key, p_for_export_data);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				Variant value;
				err = parse_variant(value, p_for_export_data);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				d[key] = value;
			}
			r_v = d;
		} break;
		case VARIANT_ARRAY: {

			uint32_t len = f->get_32();
			Array a(len & 0x80000000); //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variant(val, p_for_export_data);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				a[i] = val;
			}
			r_v = a;

		} break;
		case VARIANT_RAW_ARRAY: {

			uint32_t len = f->get_32();

			DVector<uint8_t> array;
			array.resize(len);
			DVector<uint8_t>::Write w = array.write();
			f->get_buffer(w.ptr(), len);
			_advance_padding(len);
			w = DVector<uint8_t>::Write();
			r_v = array;

		} break;
		case VARIANT_INT_ARRAY: {

			uint32_t len = f->get_32();

			DVector<int> array;
			array.resize(len);
			DVector<int>::Write w = array.write();
			f->get_buffer((uint8_t *)w.ptr(), len * 4);
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {

					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif
			w = DVector<int>::Write();
			r_v = array;
		} break;
		case VARIANT_REAL_ARRAY: {

			uint32_t len = f->get_32();

			DVector<real_t> array;
			array.resize(len);
			DVector<real_t>::Write w = array.write();
			f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {

					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			w = DVector<real_t>::Write();
			r_v = array;
		} break;
		case VARIANT_STRING_ARRAY: {

			uint32_t len = f->get_32();
			DVector<String> array;
			array.resize(len);
			DVector<String>::Write w = array.write();
			for (uint32_t i = 0; i < len; i++)
				w[i] = get_unicode_string();
			w = DVector<String>::Write();
			r_v = array;

		} break;
		case VARIANT_VECTOR2_ARRAY: {

			uint32_t len = f->get_32();

			DVector<Vector2> array;
			array.resize(len);
			DVector<Vector2>::Write w = array.write();
			if (sizeof(Vector2) == 8) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 2);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 2; i++) {

						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_EXPLAIN("Vector2 size is NOT 8!");
				ERR_FAIL_V(ERR_UNAVAILABLE);
			}
			w = DVector<Vector2>::Write();
			r_v = array;

		} break;
		case VARIANT_VECTOR3_ARRAY: {

			uint32_t len = f->get_32();

			DVector<Vector3> array;
			array.resize(len);
			DVector<Vector3>::Write w = array.write();
			if (sizeof(Vector3) == 12) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 3);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 3; i++) {

						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_EXPLAIN("Vector3 size is NOT 12!");
				ERR_FAIL_V(ERR_UNAVAILABLE);
			}
			w = DVector<Vector3>::Write();
			r_v = array;

		} break;
		case VARIANT_COLOR_ARRAY: {

			uint32_t len = f->get_32();

			DVector<Color> array;
			array.resize(len);
			DVector<Color>::Write w = array.write();
			if (sizeof(Color) == 16) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 4);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 4; i++) {

						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_EXPLAIN("Color size is NOT 16!");
				ERR_FAIL_V(ERR_UNAVAILABLE);
			}
			w = DVector<Color>::Write();
			r_v = array;
		} break;

		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK; //never reach anyway
}

void ResourceInteractiveLoaderBinary::set_local_path(const String &p_local_path) {

	res_path = p_local_path;
}

Ref<Resource> ResourceInteractiveLoaderBinary::get_resource() {

	return resource;
}
Error ResourceInteractiveLoaderBinary::poll() {

	if (error != OK)
		return error;

	int s = stage;

	if (s < external_resources.size()) {

		String path = external_resources[s].path;
		if (remaps.has(path)) {
			path = remaps[path];
		}
		RES res = ResourceLoader::load(path, external_resources[s].type);
		if (res.is_null()) {

			if (!ResourceLoader::get_abort_on_missing_resources()) {

				ResourceLoader::notify_dependency_error(local_path, path, external_resources[s].type);
			} else {

				error = ERR_FILE_MISSING_DEPENDENCIES;
				ERR_EXPLAIN("Can't load dependency: " + path);
				ERR_FAIL_V(error);
			}

		} else {
			resource_cache.push_back(res);
		}

		stage++;
		return error;
	}

	s -= external_resources.size();

	if (s >= internal_resources.size()) {

		error = ERR_BUG;
		ERR_FAIL_COND_V(s >= internal_resources.size(), error);
	}

	bool main = s == (internal_resources.size() - 1);

	//maybe it is loaded already
	String path;
	int subindex = 0;

	if (!main) {

		path = internal_resources[s].path;
		if (path.begins_with("local://")) {
			path = path.replace_first("local://", "");
			subindex = path.to_int();
			path = res_path + "::" + path;
		}

		if (ResourceCache::has(path)) {
			//already loaded, don't do anything
			stage++;
			error = OK;
			return error;
		}
	} else {

		if (!ResourceCache::has(res_path))
			path = res_path;
	}

	uint64_t offset = internal_resources[s].offset;

	f->seek(offset);

	String t = get_unicode_string();

	Object *obj = ObjectTypeDB::instance(t);
	if (!obj) {
		error = ERR_FILE_CORRUPT;
		ERR_EXPLAIN(local_path + ":Resource of unrecognized type in file: " + t);
	}
	ERR_FAIL_COND_V(!obj, ERR_FILE_CORRUPT);

	Resource *r = obj->cast_to<Resource>();
	if (!r) {
		error = ERR_FILE_CORRUPT;
		memdelete(obj); //bye
		ERR_EXPLAIN(local_path + ":Resource type in resource field not a resource, type is: " + obj->get_type());
		ERR_FAIL_COND_V(!r, ERR_FILE_CORRUPT);
	}

	RES res = RES(r);

	r->set_path(path);
	r->set_subindex(subindex);

	int pc = f->get_32();

	//set properties

	for (int i = 0; i < pc; i++) {

		uint32_t name_idx = f->get_32();
		if (name_idx >= (uint32_t)string_map.size()) {
			error = ERR_FILE_CORRUPT;
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		}

		Variant value;

		error = parse_variant(value);
		if (error)
			return error;

		res->set(string_map[name_idx], value);
	}
#ifdef TOOLS_ENABLED
	res->set_edited(false);
#endif
	stage++;

	resource_cache.push_back(res);

	if (main) {
		if (importmd_ofs) {

			f->seek(importmd_ofs);
			Ref<ResourceImportMetadata> imd = memnew(ResourceImportMetadata);
			imd->set_editor(get_unicode_string());
			int sc = f->get_32();
			for (int i = 0; i < sc; i++) {

				String src = get_unicode_string();
				String md5 = get_unicode_string();
				imd->add_source(src, md5);
			}
			int pc = f->get_32();

			for (int i = 0; i < pc; i++) {

				String name = get_unicode_string();
				Variant val;
				parse_variant(val);
				imd->set_option(name, val);
			}
			res->set_import_metadata(imd);
		}
		f->close();
		resource = res;
		error = ERR_FILE_EOF;

	} else {
		error = OK;
	}

	return OK;
}
int ResourceInteractiveLoaderBinary::get_stage() const {

	return stage;
}
int ResourceInteractiveLoaderBinary::get_stage_count() const {

	return external_resources.size() + internal_resources.size();
}

static void save_ustring(FileAccess *f, const String &p_string) {

	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

static String get_ustring(FileAccess *f) {

	int len = f->get_32();
	Vector<char> str_buf;
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

String ResourceInteractiveLoaderBinary::get_unicode_string() {

	int len = f->get_32();
	if (len > str_buf.size()) {
		str_buf.resize(len);
	}
	if (len == 0)
		return String();
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

Error ResourceInteractiveLoaderBinary::get_export_data(ExportData &r_export_data) {

	for (int i = 0; i < external_resources.size(); i++) {
		ExportData::Dependency dep;
		dep.path = external_resources[i].path;
		dep.type = external_resources[i].type;
		r_export_data.dependencies[i] = dep;
	}

	for (int i = 0; i < internal_resources.size(); i++) {

		bool main = i == (internal_resources.size() - 1);

		//maybe it is loaded already

		r_export_data.resources.resize(r_export_data.resources.size() + 1);
		ExportData::ResourceData &res_data = r_export_data.resources[r_export_data.resources.size() - 1];

		res_data.index = -1;

		if (!main) {

			String path = internal_resources[i].path;
			if (path.begins_with("local://")) {
				path = path.replace_first("local://", "");
				res_data.index = path.to_int();
			}
		} else {
		}

		uint64_t offset = internal_resources[i].offset;

		f->seek(offset);

		String t = get_unicode_string();

		res_data.type = t;

		int pc = f->get_32();

		//set properties

		for (int i = 0; i < pc; i++) {

			uint32_t name_idx = f->get_32();
			if (name_idx >= (uint32_t)string_map.size()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			Variant value;

			error = parse_variant(value, true);
			if (error)
				return error;

			ExportData::PropertyData pdata;
			pdata.name = string_map[name_idx];
			pdata.value = value;

			res_data.properties.push_back(pdata);
		}
	}

	return OK;
}

void ResourceInteractiveLoaderBinary::get_dependencies(FileAccess *p_f, List<String> *p_dependencies, bool p_add_types) {

	open(p_f);
	if (error)
		return;

	for (int i = 0; i < external_resources.size(); i++) {

		String dep = external_resources[i].path;
		if (dep.ends_with("*")) {
			dep = ResourceLoader::guess_full_filename(dep, external_resources[i].type);
		}

		if (p_add_types && external_resources[i].type != String()) {
			dep += "::" + external_resources[i].type;
		}

		p_dependencies->push_back(dep);
	}
}

void ResourceInteractiveLoaderBinary::open(FileAccess *p_f) {

	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		//compressed
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->open_after_magic(f);
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		//not normal

		error = ERR_FILE_UNRECOGNIZED;
		ERR_EXPLAIN("Unrecognized binary resource file: " + local_path);
		ERR_FAIL();
	}

	bool big_endian = f->get_32();
#ifdef BIG_ENDIAN_ENABLED
	endian_swap = !big_endian;
#else
	bool endian_swap = big_endian;
#endif

	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	print_bl("big endian: " + itos(big_endian));
	print_bl("endian swap: " + itos(endian_swap));
	print_bl("real64: " + itos(use_real64));
	print_bl("major: " + itos(ver_major));
	print_bl("minor: " + itos(ver_minor));
	print_bl("format: " + itos(ver_format));

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {

		f->close();
		ERR_EXPLAIN("File Format '" + itos(FORMAT_VERSION) + "." + itos(ver_major) + "." + itos(ver_minor) + "' is too new! Please upgrade to a a new engine version: " + local_path);
		ERR_FAIL();
	}

	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	for (int i = 0; i < 14; i++)
		f->get_32(); //skip a few reserved fields

	uint32_t string_table_size = f->get_32();
	string_map.resize(string_table_size);
	for (uint32_t i = 0; i < string_table_size; i++) {

		StringName s = get_unicode_string();
		string_map[i] = s;
	}

	print_bl("strings: " + itos(string_table_size));

	uint32_t ext_resources_size = f->get_32();
	for (uint32_t i = 0; i < ext_resources_size; i++) {

		ExtResource er;
		er.type = get_unicode_string();
		er.path = get_unicode_string();
		external_resources.push_back(er);
	}

	//see if the exporter has different set of external resources for more efficient loading
	/*
	String preload_depts = "deps/"+res_path.md5_text();
	if (Globals::get_singleton()->has(preload_depts)) {
		external_resources.clear();
		//ignore external resources and use these
		NodePath depts=Globals::get_singleton()->get(preload_depts);
		external_resources.resize(depts.get_name_count());
		for(int i=0;i<depts.get_name_count();i++) {
			external_resources[i].path=depts.get_name(i);
		}
		print_line(res_path+" - EXTERNAL RESOURCES: "+itos(external_resources.size()));
	}*/

	print_bl("ext resources: " + itos(ext_resources_size));
	uint32_t int_resources_size = f->get_32();

	for (uint32_t i = 0; i < int_resources_size; i++) {

		IntResource ir;
		ir.path = get_unicode_string();
		ir.offset = f->get_64();
		internal_resources.push_back(ir);
	}

	print_bl("int resources: " + itos(int_resources_size));

	if (f->eof_reached()) {

		error = ERR_FILE_CORRUPT;
		ERR_EXPLAIN("Premature End Of File: " + local_path);
		ERR_FAIL();
	}
}

String ResourceInteractiveLoaderBinary::recognize(FileAccess *p_f) {

	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		//compressed
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->open_after_magic(f);
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		//not normal
		error = ERR_FILE_UNRECOGNIZED;
		return "";
	}

	bool big_endian = f->get_32();
#ifdef BIG_ENDIAN_ENABLED
	endian_swap = !big_endian;
#else
	bool endian_swap = big_endian;
#endif

	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {

		f->close();
		return "";
	}

	String type = get_unicode_string();

	return type;
}

ResourceInteractiveLoaderBinary::ResourceInteractiveLoaderBinary() {

	f = NULL;
	stage = 0;
	endian_swap = false;
	use_real64 = false;
	error = OK;
}

ResourceInteractiveLoaderBinary::~ResourceInteractiveLoaderBinary() {

	if (f)
		memdelete(f);
}

Ref<ResourceInteractiveLoader> ResourceFormatLoaderBinary::load_interactive(const String &p_path, Error *r_error) {

	if (r_error)
		*r_error = ERR_FILE_CANT_OPEN;

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (err != OK) {

		ERR_FAIL_COND_V(err != OK, Ref<ResourceInteractiveLoader>());
	}

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = Globals::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->open(f);

	return ria;
}

void ResourceFormatLoaderBinary::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {

	if (p_type == "") {
		get_recognized_extensions(p_extensions);
		return;
	}

	List<String> extensions;
	ObjectTypeDB::get_extensions_for_type(p_type, &extensions);

	extensions.sort();

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		String ext = E->get().to_lower();
		p_extensions->push_back(ext);
	}
}
void ResourceFormatLoaderBinary::get_recognized_extensions(List<String> *p_extensions) const {

	List<String> extensions;
	ObjectTypeDB::get_resource_base_extensions(&extensions);
	extensions.sort();

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		String ext = E->get().to_lower();
		p_extensions->push_back(ext);
	}
}

bool ResourceFormatLoaderBinary::handles_type(const String &p_type) const {

	return true; //handles all
}

Error ResourceFormatLoaderBinary::load_import_metadata(const String &p_path, Ref<ResourceImportMetadata> &r_var) const {

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		return ERR_FILE_CANT_OPEN;
	}

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = Globals::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->recognize(f);
	if (ria->error != OK)
		return ERR_FILE_UNRECOGNIZED;
	f = ria->f;
	uint64_t imp_ofs = f->get_64();

	if (imp_ofs == 0)
		return ERR_UNAVAILABLE;

	f->seek(imp_ofs);
	Ref<ResourceImportMetadata> imd = memnew(ResourceImportMetadata);
	imd->set_editor(ria->get_unicode_string());
	int sc = f->get_32();
	for (int i = 0; i < sc; i++) {

		String src = ria->get_unicode_string();
		String md5 = ria->get_unicode_string();
		imd->add_source(src, md5);
	}
	int pc = f->get_32();

	for (int i = 0; i < pc; i++) {

		String name = ria->get_unicode_string();
		Variant val;
		ria->parse_variant(val);
		imd->set_option(name, val);
	}

	r_var = imd;

	return OK;
}

ResourceFormatLoaderBinary *ResourceFormatLoaderBinary::singleton = NULL;

void ResourceFormatLoaderBinary::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND(!f);

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = Globals::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatLoaderBinary::get_export_data(const String &p_path, ExportData &r_export_data) {

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (err != OK) {

		ERR_FAIL_COND_V(err != OK, ERR_CANT_OPEN);
	}

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = Globals::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->open(f);

	return ria->get_export_data(r_export_data);
}

Error ResourceFormatLoaderBinary::rename_dependencies(const String &p_path, const Map<String, String> &p_map) {

	//	Error error=OK;

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!f, ERR_CANT_OPEN);

	FileAccess *fw = NULL; //=FileAccess::open(p_path+".depren");

	String local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		//compressed
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->open_after_magic(f);
		f = fac;

		FileAccessCompressed *facw = memnew(FileAccessCompressed);
		facw->configure("RSCC");
		Error err = facw->_open(p_path + ".depren", FileAccess::WRITE);
		if (err) {
			memdelete(fac);
			memdelete(facw);
			ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
		}

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		//not normal

		//error=ERR_FILE_UNRECOGNIZED;
		memdelete(f);
		ERR_EXPLAIN("Unrecognized binary resource file: " + local_path);
		ERR_FAIL_V(ERR_FILE_UNRECOGNIZED);
	} else {
		fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);
		if (!fw) {
			memdelete(f);
		}
		ERR_FAIL_COND_V(!fw, ERR_CANT_CREATE);
	}

	bool big_endian = f->get_32();
#ifdef BIG_ENDIAN_ENABLED
	endian_swap = !big_endian;
#else
	bool endian_swap = big_endian;
#endif

	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian
	fw->store_32(endian_swap);
	fw->set_endian_swap(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format < FORMAT_VERSION_CAN_RENAME_DEPS) {

		memdelete(f);
		memdelete(fw);
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->remove(p_path + ".depren");
		memdelete(da);
		//fuck it, use the old approach;

		WARN_PRINT(("This file is old, so it can't refactor dependencies, opening and resaving: " + p_path).utf8().get_data());

		Error err;
		f = FileAccess::open(p_path, FileAccess::READ, &err);
		if (err != OK) {
			ERR_FAIL_COND_V(err != OK, ERR_FILE_CANT_OPEN);
		}

		Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
		ria->local_path = Globals::get_singleton()->localize_path(p_path);
		ria->res_path = ria->local_path;
		ria->remaps = p_map;
		//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
		ria->open(f);

		err = ria->poll();

		while (err == OK) {
			err = ria->poll();
		}

		ERR_FAIL_COND_V(err != ERR_FILE_EOF, ERR_FILE_CORRUPT);
		RES res = ria->get_resource();
		ERR_FAIL_COND_V(!res.is_valid(), ERR_FILE_CORRUPT);

		return ResourceFormatSaverBinary::singleton->save(p_path, res);
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {

		memdelete(f);
		memdelete(fw);
		ERR_EXPLAIN("File Format '" + itos(FORMAT_VERSION) + "." + itos(ver_major) + "." + itos(ver_minor) + "' is too new! Please upgrade to a a new engine version: " + local_path);
		ERR_FAIL_V(ERR_FILE_UNRECOGNIZED);
	}

	fw->store_32(VERSION_MAJOR); //current version
	fw->store_32(VERSION_MINOR);
	fw->store_32(FORMAT_VERSION);

	save_ustring(fw, get_ustring(f)); //type

	size_t md_ofs = f->get_pos();
	size_t importmd_ofs = f->get_64();
	fw->store_64(0); //metadata offset

	for (int i = 0; i < 14; i++) {
		fw->store_32(0);
		f->get_32();
	}

	//string table
	uint32_t string_table_size = f->get_32();

	fw->store_32(string_table_size);

	for (uint32_t i = 0; i < string_table_size; i++) {

		String s = get_ustring(f);
		save_ustring(fw, s);
	}

	//external resources
	uint32_t ext_resources_size = f->get_32();
	fw->store_32(ext_resources_size);
	for (uint32_t i = 0; i < ext_resources_size; i++) {

		String type = get_ustring(f);
		String path = get_ustring(f);

		bool relative = false;
		if (!path.begins_with("res://")) {
			path = local_path.plus_file(path).simplify_path();
			relative = true;
		}

		if (p_map.has(path)) {
			String np = p_map[path];
			path = np;
		}

		if (relative) {
			//restore relative
			path = local_path.path_to_file(path);
		}

		save_ustring(fw, type);
		save_ustring(fw, path);
	}

	int64_t size_diff = (int64_t)fw->get_pos() - (int64_t)f->get_pos();

	//internal resources
	uint32_t int_resources_size = f->get_32();
	fw->store_32(int_resources_size);

	for (uint32_t i = 0; i < int_resources_size; i++) {

		String path = get_ustring(f);
		uint64_t offset = f->get_64();
		save_ustring(fw, path);
		fw->store_64(offset + size_diff);
	}

	//rest of file
	uint8_t b = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(b);
		b = f->get_8();
	}

	bool all_ok = fw->get_error() == OK;

	fw->seek(md_ofs);
	fw->store_64(importmd_ofs + size_diff);

	memdelete(f);
	memdelete(fw);

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	da->remove(p_path);
	da->rename(p_path + ".depren", p_path);
	memdelete(da);
	return OK;
}

String ResourceFormatLoaderBinary::get_resource_type(const String &p_path) const {

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		return ""; //could not rwead
	}

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = Globals::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//	ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	String r = ria->recognize(f);
	return r;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

void ResourceFormatSaverBinaryInstance::_pad_buffer(int p_bytes) {

	int extra = 4 - (p_bytes % 4);
	if (extra < 4) {
		for (int i = 0; i < extra; i++)
			f->store_8(0); //pad to 32
	}
}

void ResourceFormatSaverBinaryInstance::write_variant(const Variant &p_property, const PropertyInfo &p_hint) {

	switch (p_property.get_type()) {

		case Variant::NIL: {

			f->store_32(VARIANT_NIL);
			// don't store anything
		} break;
		case Variant::BOOL: {

			f->store_32(VARIANT_BOOL);
			bool val = p_property;
			f->store_32(val);
		} break;
		case Variant::INT: {

			f->store_32(VARIANT_INT);
			int val = p_property;
			f->store_32(val);
		} break;
		case Variant::REAL: {

			f->store_32(VARIANT_REAL);
			real_t val = p_property;
			f->store_real(val);

		} break;
		case Variant::STRING: {

			f->store_32(VARIANT_STRING);
			String val = p_property;
			save_unicode_string(val);

		} break;
		case Variant::VECTOR2: {

			f->store_32(VARIANT_VECTOR2);
			Vector2 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);

		} break;
		case Variant::RECT2: {

			f->store_32(VARIANT_RECT2);
			Rect2 val = p_property;
			f->store_real(val.pos.x);
			f->store_real(val.pos.y);
			f->store_real(val.size.x);
			f->store_real(val.size.y);

		} break;
		case Variant::VECTOR3: {

			f->store_32(VARIANT_VECTOR3);
			Vector3 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);

		} break;
		case Variant::PLANE: {

			f->store_32(VARIANT_PLANE);
			Plane val = p_property;
			f->store_real(val.normal.x);
			f->store_real(val.normal.y);
			f->store_real(val.normal.z);
			f->store_real(val.d);

		} break;
		case Variant::QUAT: {

			f->store_32(VARIANT_QUAT);
			Quat val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::_AABB: {

			f->store_32(VARIANT_AABB);
			AABB val = p_property;
			f->store_real(val.pos.x);
			f->store_real(val.pos.y);
			f->store_real(val.pos.z);
			f->store_real(val.size.x);
			f->store_real(val.size.y);
			f->store_real(val.size.z);

		} break;
		case Variant::MATRIX32: {

			f->store_32(VARIANT_MATRIX32);
			Matrix32 val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);

		} break;
		case Variant::MATRIX3: {

			f->store_32(VARIANT_MATRIX3);
			Matrix3 val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[0].z);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[1].z);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);
			f->store_real(val.elements[2].z);

		} break;
		case Variant::TRANSFORM: {

			f->store_32(VARIANT_TRANSFORM);
			Transform val = p_property;
			f->store_real(val.basis.elements[0].x);
			f->store_real(val.basis.elements[0].y);
			f->store_real(val.basis.elements[0].z);
			f->store_real(val.basis.elements[1].x);
			f->store_real(val.basis.elements[1].y);
			f->store_real(val.basis.elements[1].z);
			f->store_real(val.basis.elements[2].x);
			f->store_real(val.basis.elements[2].y);
			f->store_real(val.basis.elements[2].z);
			f->store_real(val.origin.x);
			f->store_real(val.origin.y);
			f->store_real(val.origin.z);

		} break;
		case Variant::COLOR: {

			f->store_32(VARIANT_COLOR);
			Color val = p_property;
			f->store_real(val.r);
			f->store_real(val.g);
			f->store_real(val.b);
			f->store_real(val.a);

		} break;
		case Variant::IMAGE: {

			f->store_32(VARIANT_IMAGE);
			Image val = p_property;
			if (val.empty()) {
				f->store_32(IMAGE_ENCODING_EMPTY);
				break;
			}

			int encoding = IMAGE_ENCODING_RAW;
			float quality = 0.7;

			if (val.get_format() <= Image::FORMAT_INDEXED_ALPHA) {
				//can only compress uncompressed stuff

				if (p_hint.hint == PROPERTY_HINT_IMAGE_COMPRESS_LOSSY && Image::lossy_packer) {
					encoding = IMAGE_ENCODING_LOSSY;
					float qs = p_hint.hint_string.to_double();
					if (qs != 0.0)
						quality = qs;

				} else if (p_hint.hint == PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS && Image::lossless_packer) {
					encoding = IMAGE_ENCODING_LOSSLESS;
				}
			}

			f->store_32(encoding); //raw encoding

			if (encoding == IMAGE_ENCODING_RAW) {

				f->store_32(val.get_width());
				f->store_32(val.get_height());
				f->store_32(val.get_mipmaps());
				switch (val.get_format()) {

					case Image::FORMAT_GRAYSCALE:
						f->store_32(IMAGE_FORMAT_GRAYSCALE);
						break; ///< one byte per pixel: f->store_32(IMAGE_FORMAT_ ); break; 0-255
					case Image::FORMAT_INTENSITY:
						f->store_32(IMAGE_FORMAT_INTENSITY);
						break; ///< one byte per pixel: f->store_32(IMAGE_FORMAT_ ); break; 0-255
					case Image::FORMAT_GRAYSCALE_ALPHA:
						f->store_32(IMAGE_FORMAT_GRAYSCALE_ALPHA);
						break; ///< two bytes per pixel: f->store_32(IMAGE_FORMAT_ ); break; 0-255. alpha 0-255
					case Image::FORMAT_RGB:
						f->store_32(IMAGE_FORMAT_RGB);
						break; ///< one byte R: f->store_32(IMAGE_FORMAT_ ); break; one byte G: f->store_32(IMAGE_FORMAT_ ); break; one byte B
					case Image::FORMAT_RGBA:
						f->store_32(IMAGE_FORMAT_RGBA);
						break; ///< one byte R: f->store_32(IMAGE_FORMAT_ ); break; one byte G: f->store_32(IMAGE_FORMAT_ ); break; one byte B: f->store_32(IMAGE_FORMAT_ ); break; one byte A
					case Image::FORMAT_INDEXED:
						f->store_32(IMAGE_FORMAT_INDEXED);
						break; ///< index byte 0-256: f->store_32(IMAGE_FORMAT_ ); break; and after image end: f->store_32(IMAGE_FORMAT_ ); break; 256*3 bytes of palette
					case Image::FORMAT_INDEXED_ALPHA:
						f->store_32(IMAGE_FORMAT_INDEXED_ALPHA);
						break; ///< index byte 0-256: f->store_32(IMAGE_FORMAT_ ); break; and after image end: f->store_32(IMAGE_FORMAT_ ); break; 256*4 bytes of palette (alpha)
					case Image::FORMAT_BC1:
						f->store_32(IMAGE_FORMAT_BC1);
						break; // DXT1
					case Image::FORMAT_BC2:
						f->store_32(IMAGE_FORMAT_BC2);
						break; // DXT3
					case Image::FORMAT_BC3:
						f->store_32(IMAGE_FORMAT_BC3);
						break; // DXT5
					case Image::FORMAT_BC4:
						f->store_32(IMAGE_FORMAT_BC4);
						break; // ATI1
					case Image::FORMAT_BC5:
						f->store_32(IMAGE_FORMAT_BC5);
						break; // ATI2
					case Image::FORMAT_PVRTC2: f->store_32(IMAGE_FORMAT_PVRTC2); break;
					case Image::FORMAT_PVRTC2_ALPHA: f->store_32(IMAGE_FORMAT_PVRTC2_ALPHA); break;
					case Image::FORMAT_PVRTC4: f->store_32(IMAGE_FORMAT_PVRTC4); break;
					case Image::FORMAT_PVRTC4_ALPHA: f->store_32(IMAGE_FORMAT_PVRTC4_ALPHA); break;
					case Image::FORMAT_ETC: f->store_32(IMAGE_FORMAT_ETC); break;
					case Image::FORMAT_ATC: f->store_32(IMAGE_FORMAT_ATC); break;
					case Image::FORMAT_ATC_ALPHA_EXPLICIT: f->store_32(IMAGE_FORMAT_ATC_ALPHA_EXPLICIT); break;
					case Image::FORMAT_ATC_ALPHA_INTERPOLATED: f->store_32(IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED); break;
					case Image::FORMAT_CUSTOM: f->store_32(IMAGE_FORMAT_CUSTOM); break;
					default: {}
				}

				int dlen = val.get_data().size();
				f->store_32(dlen);
				DVector<uint8_t>::Read r = val.get_data().read();
				f->store_buffer(r.ptr(), dlen);
				_pad_buffer(dlen);
			} else {

				DVector<uint8_t> data;
				if (encoding == IMAGE_ENCODING_LOSSY) {
					data = Image::lossy_packer(val, quality);

				} else if (encoding == IMAGE_ENCODING_LOSSLESS) {
					data = Image::lossless_packer(val);
				}

				int ds = data.size();
				f->store_32(ds);
				if (ds > 0) {
					DVector<uint8_t>::Read r = data.read();
					f->store_buffer(r.ptr(), ds);

					_pad_buffer(ds);
				}
			}

		} break;
		case Variant::NODE_PATH: {
			f->store_32(VARIANT_NODE_PATH);
			NodePath np = p_property;
			f->store_16(np.get_name_count());
			uint16_t snc = np.get_subname_count();
			if (np.is_absolute())
				snc |= 0x8000;
			f->store_16(snc);
			for (int i = 0; i < np.get_name_count(); i++)
				f->store_32(get_string_index(np.get_name(i)));
			for (int i = 0; i < np.get_subname_count(); i++)
				f->store_32(get_string_index(np.get_subname(i)));
			f->store_32(get_string_index(np.get_property()));

		} break;
		case Variant::_RID: {

			f->store_32(VARIANT_RID);
			WARN_PRINT("Can't save RIDs");
			RID val = p_property;
			f->store_32(val.get_id());
		} break;
		case Variant::OBJECT: {

			f->store_32(VARIANT_OBJECT);
			RES res = p_property;
			if (res.is_null()) {
				f->store_32(OBJECT_EMPTY);
				return; // don't save it
			}

			if (res->get_path().length() && res->get_path().find("::") == -1) {
				f->store_32(OBJECT_EXTERNAL_RESOURCE_INDEX);
				f->store_32(external_resources[res]);
			} else {

				if (!resource_set.has(res)) {
					f->store_32(OBJECT_EMPTY);
					ERR_EXPLAIN("Resource was not pre cached for the resource section, bug?");
					ERR_FAIL();
				}

				f->store_32(OBJECT_INTERNAL_RESOURCE);
				f->store_32(res->get_subindex());
				//internal resource
			}

		} break;
		case Variant::INPUT_EVENT: {

			f->store_32(VARIANT_INPUT_EVENT);
			WARN_PRINT("Can't save InputEvent (maybe it could..)");
		} break;
		case Variant::DICTIONARY: {

			f->store_32(VARIANT_DICTIONARY);
			Dictionary d = p_property;
			f->store_32(uint32_t(d.size()) | (d.is_shared() ? 0x80000000 : 0));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				//if (!_check_type(dict[E->get()]))
				//	continue;

				write_variant(E->get());
				write_variant(d[E->get()]);
			}

		} break;
		case Variant::ARRAY: {

			f->store_32(VARIANT_ARRAY);
			Array a = p_property;
			f->store_32(uint32_t(a.size()) | (a.is_shared() ? 0x80000000 : 0));
			for (int i = 0; i < a.size(); i++) {

				write_variant(a[i]);
			}

		} break;
		case Variant::RAW_ARRAY: {

			f->store_32(VARIANT_RAW_ARRAY);
			DVector<uint8_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<uint8_t>::Read r = arr.read();
			f->store_buffer(r.ptr(), len);
			_pad_buffer(len);

		} break;
		case Variant::INT_ARRAY: {

			f->store_32(VARIANT_INT_ARRAY);
			DVector<int> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<int>::Read r = arr.read();
			for (int i = 0; i < len; i++)
				f->store_32(r[i]);

		} break;
		case Variant::REAL_ARRAY: {

			f->store_32(VARIANT_REAL_ARRAY);
			DVector<real_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<real_t>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i]);
			}

		} break;
		case Variant::STRING_ARRAY: {

			f->store_32(VARIANT_STRING_ARRAY);
			DVector<String> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<String>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				save_unicode_string(r[i]);
			}

		} break;
		case Variant::VECTOR3_ARRAY: {

			f->store_32(VARIANT_VECTOR3_ARRAY);
			DVector<Vector3> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<Vector3>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
				f->store_real(r[i].z);
			}

		} break;
		case Variant::VECTOR2_ARRAY: {

			f->store_32(VARIANT_VECTOR2_ARRAY);
			DVector<Vector2> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<Vector2>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
			}

		} break;
		case Variant::COLOR_ARRAY: {

			f->store_32(VARIANT_COLOR_ARRAY);
			DVector<Color> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			DVector<Color>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].r);
				f->store_real(r[i].g);
				f->store_real(r[i].b);
				f->store_real(r[i].a);
			}

		} break;
		default: {

			ERR_EXPLAIN("Invalid variant");
			ERR_FAIL();
		}
	}
}

void ResourceFormatSaverBinaryInstance::_find_resources(const Variant &p_variant, bool p_main) {

	switch (p_variant.get_type()) {
		case Variant::OBJECT: {

			RES res = p_variant.operator RefPtr();

			if (res.is_null() || external_resources.has(res))
				return;

			if (!p_main && (!bundle_resources) && res->get_path().length() && res->get_path().find("::") == -1) {
				int idx = external_resources.size();
				external_resources[res] = idx;
				return;
			}

			if (resource_set.has(res))
				return;

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);

			for (List<PropertyInfo>::Element *E = property_list.front(); E; E = E->next()) {

				if (E->get().usage & PROPERTY_USAGE_STORAGE || (bundle_resources && E->get().usage & PROPERTY_USAGE_BUNDLE)) {

					_find_resources(res->get(E->get().name));
				}
			}

			resource_set.insert(res);
			saved_resources.push_back(res);

		} break;

		case Variant::ARRAY: {

			Array varray = p_variant;
			int len = varray.size();
			for (int i = 0; i < len; i++) {

				Variant v = varray.get(i);
				_find_resources(v);
			}

		} break;

		case Variant::DICTIONARY: {

			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				_find_resources(E->get());
				Variant v = d[E->get()];
				_find_resources(v);
			}
		} break;
		case Variant::NODE_PATH: {
			//take the chance and save node path strings
			NodePath np = p_variant;
			for (int i = 0; i < np.get_name_count(); i++)
				get_string_index(np.get_name(i));
			for (int i = 0; i < np.get_subname_count(); i++)
				get_string_index(np.get_subname(i));
			get_string_index(np.get_property());

		} break;

		default: {}
	}
}
#if 0
Error ResourceFormatSaverBinary::_save_obj(const Object *p_object,SavedObject *so) {

	//use classic way
	List<PropertyInfo> property_list;
	p_object->get_property_list( &property_list );

	for(List<PropertyInfo>::Element *E=property_list.front();E;E=E->next()) {

		if (skip_editor && E->get().name.begins_with("__editor"))
			continue;
		if (E->get().usage&PROPERTY_USAGE_STORAGE || (bundle_resources && E->get().usage&PROPERTY_USAGE_BUNDLE)) {

			SavedObject::SavedProperty sp;
			sp.name_idx=get_string_index(E->get().name);
			sp.value = p_object->get(E->get().name);
			_find_resources(sp.value);
			so->properties.push_back(sp);
		}
	}

	return OK;

}



Error ResourceFormatSaverBinary::save(const Object *p_object,const Variant &p_meta) {

	ERR_FAIL_COND_V(!f,ERR_UNCONFIGURED);
	ERR_EXPLAIN("write_object should supply either an object, a meta, or both");
	ERR_FAIL_COND_V(!p_object && p_meta.get_type()==Variant::NIL, ERR_INVALID_PARAMETER);

	SavedObject *so = memnew( SavedObject );

	if (p_object)
		so->type=p_object->get_type();

	_find_resources(p_meta);
	so->meta=p_meta;
	Error err = _save_obj(p_object,so);
	ERR_FAIL_COND_V( err, ERR_INVALID_DATA );

	saved_objects.push_back(so);

	return OK;
}
#endif

void ResourceFormatSaverBinaryInstance::save_unicode_string(const String &p_string) {

	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

int ResourceFormatSaverBinaryInstance::get_string_index(const String &p_string) {

	StringName s = p_string;
	if (string_map.has(s))
		return string_map[s];

	string_map[s] = strings.size();
	strings.push_back(s);
	return strings.size() - 1;
}

Error ResourceFormatSaverBinaryInstance::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {

	Error err;
	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->configure("RSCC");
		f = fac;
		err = fac->_open(p_path, FileAccess::WRITE);
		if (err)
			memdelete(f);

	} else {
		f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V(err, err);
	FileAccessRef _fref(f);

	relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	big_endian = p_flags & ResourceSaver::FLAG_SAVE_BIG_ENDIAN;
	takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	if (!p_path.begins_with("res://"))
		takeover_paths = false;

	local_path = p_path.get_base_dir();
	//bin_meta_idx = get_string_index("__bin_meta__"); //is often used, so create

	_find_resources(p_resource, true);

	if (!(p_flags & ResourceSaver::FLAG_COMPRESS)) {
		//save header compressed
		static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
		f->store_buffer(header, 4);
	}

	if (big_endian) {
		f->store_32(1);
		f->set_endian_swap(true);
	} else
		f->store_32(0);

	f->store_32(0); //64 bits file, false for now
	f->store_32(VERSION_MAJOR);
	f->store_32(VERSION_MINOR);
	f->store_32(FORMAT_VERSION);

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		f->close();
		return ERR_CANT_CREATE;
	}

	//f->store_32(saved_resources.size()+external_resources.size()); // load steps -not needed
	save_unicode_string(p_resource->get_type());
	uint64_t md_at = f->get_pos();
	f->store_64(0); //offset to impoty metadata
	for (int i = 0; i < 14; i++)
		f->store_32(0); // reserved

	List<ResourceData> resources;

	{

		for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {

			ResourceData &rd = resources.push_back(ResourceData())->get();
			rd.type = E->get()->get_type();

			List<PropertyInfo> property_list;
			E->get()->get_property_list(&property_list);

			for (List<PropertyInfo>::Element *F = property_list.front(); F; F = F->next()) {

				if (skip_editor && F->get().name.begins_with("__editor"))
					continue;
				if (F->get().usage & PROPERTY_USAGE_STORAGE || (bundle_resources && F->get().usage & PROPERTY_USAGE_BUNDLE)) {
					Property p;
					p.name_idx = get_string_index(F->get().name);
					p.value = E->get()->get(F->get().name);
					if ((F->get().usage & PROPERTY_USAGE_STORE_IF_NONZERO && p.value.is_zero()) || (F->get().usage & PROPERTY_USAGE_STORE_IF_NONONE && p.value.is_one()))
						continue;
					p.pi = F->get();

					rd.properties.push_back(p);
				}
			}
		}
	}

	f->store_32(strings.size()); //string table size
	for (int i = 0; i < strings.size(); i++) {
		//print_bl("saving string: "+strings[i]);
		save_unicode_string(strings[i]);
	}

	// save external resource table
	f->store_32(external_resources.size()); //amount of external resources
	Vector<RES> save_order;
	save_order.resize(external_resources.size());

	for (Map<RES, int>::Element *E = external_resources.front(); E; E = E->next()) {
		save_order[E->get()] = E->key();
	}

	for (int i = 0; i < save_order.size(); i++) {

		save_unicode_string(save_order[i]->get_save_type());
		String path = save_order[i]->get_path();
		path = relative_paths ? local_path.path_to_file(path) : path;
		save_unicode_string(path);
	}
	// save internal resource table
	f->store_32(saved_resources.size()); //amount of internal resources
	Vector<uint64_t> ofs_pos;
	Set<int> used_indices;

	for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {

		RES r = E->get();
		if (r->get_path() == "" || r->get_path().find("::") != -1) {

			if (r->get_subindex() != 0) {
				if (used_indices.has(r->get_subindex())) {
					r->set_subindex(0); //repeated
				} else {
					used_indices.insert(r->get_subindex());
				}
			}
		}
	}

	for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {

		RES r = E->get();
		if (r->get_path() == "" || r->get_path().find("::") != -1) {
			if (r->get_subindex() == 0) {
				int new_subindex = 1;
				if (used_indices.size()) {
					new_subindex = used_indices.back()->get() + 1;
				}

				r->set_subindex(new_subindex);
				used_indices.insert(new_subindex);
			}

			save_unicode_string("local://" + itos(r->get_subindex()));
			if (takeover_paths) {
				r->set_path(p_path + "::" + itos(r->get_subindex()), true);
			}
		} else {
			save_unicode_string(r->get_path()); //actual external
		}
		ofs_pos.push_back(f->get_pos());
		f->store_64(0); //offset in 64 bits
	}

	Vector<uint64_t> ofs_table;
	//	int saved_idx=0;
	//now actually save the resources

	for (List<ResourceData>::Element *E = resources.front(); E; E = E->next()) {

		ResourceData &rd = E->get();

		ofs_table.push_back(f->get_pos());
		save_unicode_string(rd.type);
		f->store_32(rd.properties.size());

		for (List<Property>::Element *F = rd.properties.front(); F; F = F->next()) {

			Property &p = F->get();
			f->store_32(p.name_idx);
			write_variant(p.value, F->get().pi);
		}
	}

	for (int i = 0; i < ofs_table.size(); i++) {
		f->seek(ofs_pos[i]);
		f->store_64(ofs_table[i]);
	}

	f->seek_end();
	print_line("Saving: " + p_path);
	if (p_resource->get_import_metadata().is_valid()) {
		uint64_t md_pos = f->get_pos();
		Ref<ResourceImportMetadata> imd = p_resource->get_import_metadata();
		save_unicode_string(imd->get_editor());
		f->store_32(imd->get_source_count());
		for (int i = 0; i < imd->get_source_count(); i++) {
			save_unicode_string(imd->get_source_path(i));
			save_unicode_string(imd->get_source_md5(i));
		}
		List<String> options;
		imd->get_options(&options);
		f->store_32(options.size());
		for (List<String>::Element *E = options.front(); E; E = E->next()) {
			save_unicode_string(E->get());
			write_variant(imd->get_option(E->get()));
		}

		f->seek(md_at);
		f->store_64(md_pos);
		f->seek_end();
	}

	f->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		f->close();
		return ERR_CANT_CREATE;
	}

	f->close();

	return OK;
}

Error ResourceFormatSaverBinary::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {

	String local_path = Globals::get_singleton()->localize_path(p_path);
	ResourceFormatSaverBinaryInstance saver;
	return saver.save(local_path, p_resource, p_flags);
}

bool ResourceFormatSaverBinary::recognize(const RES &p_resource) const {

	return true; //all recognized
}

void ResourceFormatSaverBinary::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {

	String base = p_resource->get_base_extension().to_lower();
	p_extensions->push_back(base);
}

ResourceFormatSaverBinary *ResourceFormatSaverBinary::singleton = NULL;

ResourceFormatSaverBinary::ResourceFormatSaverBinary() {

	singleton = this;
}
