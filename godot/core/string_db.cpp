/*************************************************************************/
/*  string_db.cpp                                                        */
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
#include "string_db.h"
#include "os/os.h"
#include "print_string.h"
StaticCString StaticCString::create(const char *p_ptr) {
	StaticCString scs;
	scs.ptr = p_ptr;
	return scs;
}

StringName::_Data *StringName::_table[STRING_TABLE_LEN];

StringName _scs_create(const char *p_chr) {

	return (p_chr[0] ? StringName(StaticCString::create(p_chr)) : StringName());
}

bool StringName::configured = false;

void StringName::setup() {

	ERR_FAIL_COND(configured);
	for (int i = 0; i < STRING_TABLE_LEN; i++) {

		_table[i] = NULL;
	}
	configured = true;
}

void StringName::cleanup() {

	_global_lock();
	int lost_strings = 0;
	for (int i = 0; i < STRING_TABLE_LEN; i++) {

		while (_table[i]) {

			_Data *d = _table[i];
			lost_strings++;
			if (OS::get_singleton()->is_stdout_verbose()) {

				if (d->cname) {
					print_line("Orphan StringName: " + String(d->cname));
				} else {
					print_line("Orphan StringName: " + String(d->name));
				}
			}

			_table[i] = _table[i]->next;
			memdelete(d);
		}
	}
	if (OS::get_singleton()->is_stdout_verbose() && lost_strings) {
		print_line("StringName: " + itos(lost_strings) + " unclaimed string names at exit.");
	}
	_global_unlock();
}

void StringName::unref() {

	ERR_FAIL_COND(!configured);

	if (_data && _data->refcount.unref()) {

		_global_lock();

		if (_data->prev) {
			_data->prev->next = _data->next;
		} else {
			if (_table[_data->idx] != _data) {
				ERR_PRINT("BUG!");
			}
			_table[_data->idx] = _data->next;
		}

		if (_data->next) {
			_data->next->prev = _data->prev;
		}
		memdelete(_data);
		_global_unlock();
	}

	_data = NULL;
}

bool StringName::operator==(const String &p_name) const {

	if (!_data) {

		return (p_name.length() == 0);
	}

	return (_data->get_name() == p_name);
}

bool StringName::operator==(const char *p_name) const {

	if (!_data) {

		return (p_name[0] == 0);
	}

	return (_data->get_name() == p_name);
}

bool StringName::operator!=(const String &p_name) const {

	return !(operator==(p_name));
}

bool StringName::operator!=(const StringName &p_name) const {

	// the real magic of all this mess happens here.
	// this is why path comparisons are very fast
	return _data != p_name._data;
}

void StringName::operator=(const StringName &p_name) {

	if (this == &p_name)
		return;

	unref();

	if (p_name._data && p_name._data->refcount.ref()) {

		_data = p_name._data;
	}
}
/* was inlined
StringName::operator String() const {

	if (_data)
		return _data->get_name();

	return "";
}
*/
StringName::StringName(const StringName &p_name) {

	ERR_FAIL_COND(!configured);
	_data = NULL;
	if (p_name._data && p_name._data->refcount.ref()) {

		_data = p_name._data;
	}
}

StringName::StringName(const char *p_name) {

	_data = NULL;

	ERR_FAIL_COND(!configured);

	ERR_FAIL_COND(!p_name || !p_name[0]);

	_global_lock();

	uint32_t hash = String::hash(p_name);

	uint32_t idx = hash & STRING_TABLE_MASK;

	_data = _table[idx];

	while (_data) {

		// compare hash first
		if (_data->hash == hash && _data->get_name() == p_name)
			break;
		_data = _data->next;
	}

	if (_data) {
		if (_data->refcount.ref()) {
			// exists
			_global_unlock();
			return;
		} else {
		}
	}

	_data = memnew(_Data);
	_data->name = p_name;
	_data->refcount.init();
	_data->hash = hash;
	_data->idx = idx;
	_data->cname = NULL;
	_data->next = _table[idx];
	_data->prev = NULL;
	if (_table[idx])
		_table[idx]->prev = _data;
	_table[idx] = _data;

	_global_unlock();
}

StringName::StringName(const StaticCString &p_static_string) {

	_data = NULL;

	ERR_FAIL_COND(!configured);

	ERR_FAIL_COND(!p_static_string.ptr || !p_static_string.ptr[0]);

	_global_lock();

	uint32_t hash = String::hash(p_static_string.ptr);

	uint32_t idx = hash & STRING_TABLE_MASK;

	_data = _table[idx];

	while (_data) {

		// compare hash first
		if (_data->hash == hash && _data->get_name() == p_static_string.ptr)
			break;
		_data = _data->next;
	}

	if (_data) {
		if (_data->refcount.ref()) {
			// exists
			_global_unlock();
			return;
		} else {
		}
	}

	_data = memnew(_Data);

	_data->refcount.init();
	_data->hash = hash;
	_data->idx = idx;
	_data->cname = p_static_string.ptr;
	_data->next = _table[idx];
	_data->prev = NULL;
	if (_table[idx])
		_table[idx]->prev = _data;
	_table[idx] = _data;

	_global_unlock();
}

StringName::StringName(const String &p_name) {

	_data = NULL;

	ERR_FAIL_COND(!configured);

	if (p_name.empty())
		return;

	_global_lock();

	uint32_t hash = p_name.hash();

	uint32_t idx = hash & STRING_TABLE_MASK;

	_data = _table[idx];

	while (_data) {

		if (_data->hash == hash && _data->get_name() == p_name)
			break;
		_data = _data->next;
	}

	if (_data) {
		if (_data->refcount.ref()) {
			// exists
			_global_unlock();
			return;
		} else {
		}
	}

	_data = memnew(_Data);
	_data->name = p_name;
	_data->refcount.init();
	_data->hash = hash;
	_data->idx = idx;
	_data->cname = NULL;
	_data->next = _table[idx];
	_data->prev = NULL;
	if (_table[idx])
		_table[idx]->prev = _data;
	_table[idx] = _data;

	_global_unlock();
}

StringName StringName::search(const char *p_name) {

	ERR_FAIL_COND_V(!configured, StringName());

	ERR_FAIL_COND_V(!p_name, StringName());
	if (!p_name[0])
		return StringName();

	_global_lock();

	uint32_t hash = String::hash(p_name);

	uint32_t idx = hash & STRING_TABLE_MASK;

	_Data *_data = _table[idx];

	while (_data) {

		// compare hash first
		if (_data->hash == hash && _data->get_name() == p_name)
			break;
		_data = _data->next;
	}

	if (_data && _data->refcount.ref()) {
		_global_unlock();
		return StringName(_data);
	}

	_global_unlock();
	return StringName(); //does not exist
}

StringName StringName::search(const CharType *p_name) {

	ERR_FAIL_COND_V(!configured, StringName());

	ERR_FAIL_COND_V(!p_name, StringName());
	if (!p_name[0])
		return StringName();

	_global_lock();

	uint32_t hash = String::hash(p_name);

	uint32_t idx = hash & STRING_TABLE_MASK;

	_Data *_data = _table[idx];

	while (_data) {

		// compare hash first
		if (_data->hash == hash && _data->get_name() == p_name)
			break;
		_data = _data->next;
	}

	if (_data && _data->refcount.ref()) {
		_global_unlock();
		return StringName(_data);
	}

	_global_unlock();
	return StringName(); //does not exist
}
StringName StringName::search(const String &p_name) {

	ERR_FAIL_COND_V(p_name == "", StringName());

	_global_lock();

	uint32_t hash = p_name.hash();

	uint32_t idx = hash & STRING_TABLE_MASK;

	_Data *_data = _table[idx];

	while (_data) {

		// compare hash first
		if (_data->hash == hash && p_name == _data->get_name())
			break;
		_data = _data->next;
	}

	if (_data && _data->refcount.ref()) {
		_global_unlock();
		return StringName(_data);
	}

	_global_unlock();
	return StringName(); //does not exist
}

StringName::StringName() {

	_data = NULL;
}

StringName::~StringName() {

	unref();
}
