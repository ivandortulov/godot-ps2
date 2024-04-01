/*************************************************************************/
/*  file_access_unix.cpp                                                 */
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
#include "file_access_unix.h"

#if defined(UNIX_ENABLED) || defined(LIBC_FILEIO_ENABLED)

#include "core/os/os.h"
#include "print_string.h"
#include <sys/stat.h>
#include <sys/types.h>

#ifndef ANDROID_ENABLED
#include <sys/statvfs.h>
#endif

#ifdef MSVC
#define S_ISREG(m) ((m)&_S_IFREG)
#endif
#ifndef S_ISREG
#define S_ISREG(m) ((m)&S_IFREG)
#endif

void FileAccessUnix::check_errors() const {

	ERR_FAIL_COND(!f);

	if (feof(f)) {

		last_error = ERR_FILE_EOF;
	}
}

Error FileAccessUnix::_open(const String &p_path, int p_mode_flags) {

	if (f)
		fclose(f);
	f = NULL;

	path = fix_path(p_path);
	//printf("opening %ls, %i\n", path.c_str(), Memory::get_static_mem_usage());

	ERR_FAIL_COND_V(f, ERR_ALREADY_IN_USE);
	const char *mode_string;

	if (p_mode_flags == READ)
		mode_string = "rb";
	else if (p_mode_flags == WRITE)
		mode_string = "wb";
	else if (p_mode_flags == READ_WRITE)
		mode_string = "rb+";
	else if (p_mode_flags == WRITE_READ)
		mode_string = "wb+";
	else
		return ERR_INVALID_PARAMETER;

	/* pretty much every implementation that uses fopen as primary
	   backend (unix-compatible mostly) supports utf8 encoding */

	//printf("opening %s as %s\n", p_path.utf8().get_data(), path.utf8().get_data());
	struct stat st;
	if (stat(path.utf8().get_data(), &st) == 0) {

		if (!S_ISREG(st.st_mode))
			return ERR_FILE_CANT_OPEN;
	};

	if (is_backup_save_enabled() && p_mode_flags & WRITE && !(p_mode_flags & READ)) {
		save_path = path;
		path = path + ".tmp";
		//print_line("saving instead to "+path);
	}

	f = fopen(path.utf8().get_data(), mode_string);

	if (f == NULL) {
		last_error = ERR_FILE_CANT_OPEN;
		return ERR_FILE_CANT_OPEN;
	} else {
		last_error = OK;
		flags = p_mode_flags;
		return OK;
	}
}
void FileAccessUnix::close() {

	if (!f)
		return;
	fclose(f);
	f = NULL;
	if (close_notification_func) {
		close_notification_func(path, flags);
	}
	if (save_path != "") {

		//unlink(save_path.utf8().get_data());
		//print_line("renaming..");
		int rename_error = rename((save_path + ".tmp").utf8().get_data(), save_path.utf8().get_data());

		if (rename_error && close_fail_notify) {
			close_fail_notify(save_path);
		}

		save_path = "";
		ERR_FAIL_COND(rename_error != 0);
	}
}
bool FileAccessUnix::is_open() const {

	return (f != NULL);
}
void FileAccessUnix::seek(size_t p_position) {

	ERR_FAIL_COND(!f);

	last_error = OK;
	if (fseek(f, p_position, SEEK_SET))
		check_errors();
}
void FileAccessUnix::seek_end(int64_t p_position) {

	ERR_FAIL_COND(!f);
	if (fseek(f, p_position, SEEK_END))
		check_errors();
}
size_t FileAccessUnix::get_pos() const {

	size_t aux_position = 0;
	if (!(aux_position = ftell(f))) {
		check_errors();
	};
	return aux_position;
}
size_t FileAccessUnix::get_len() const {

	ERR_FAIL_COND_V(!f, 0);

	FileAccessUnix *fau = const_cast<FileAccessUnix *>(this);
	int pos = fau->get_pos();
	fau->seek_end();
	int size = fau->get_pos();
	fau->seek(pos);

	return size;
}

bool FileAccessUnix::eof_reached() const {

	return last_error == ERR_FILE_EOF;
}

uint8_t FileAccessUnix::get_8() const {

	ERR_FAIL_COND_V(!f, 0);
	uint8_t b;
	if (fread(&b, 1, 1, f) == 0) {
		check_errors();
	};

	return b;
}

int FileAccessUnix::get_buffer(uint8_t *p_dst, int p_length) const {

	ERR_FAIL_COND_V(!f, -1);
	int read = fread(p_dst, 1, p_length, f);
	check_errors();
	return read;
};

Error FileAccessUnix::get_error() const {

	return last_error;
}

void FileAccessUnix::store_8(uint8_t p_dest) {

	ERR_FAIL_COND(!f);
	fwrite(&p_dest, 1, 1, f);
}

void FileAccessUnix::store_buffer(const uint8_t *p_src, int p_length) {
	ERR_FAIL_COND(!f);
	ERR_FAIL_COND(fwrite(p_src, 1, p_length, f) != p_length);
}

bool FileAccessUnix::file_exists(const String &p_path) {

	FILE *g;
	//printf("opening file %s\n", p_fname.c_str());
	String filename = fix_path(p_path);
	g = fopen(filename.utf8().get_data(), "rb");
	if (g == NULL) {

		return false;
	} else {

		fclose(g);
		return true;
	}
}

uint64_t FileAccessUnix::_get_modified_time(const String &p_file) {

	String file = fix_path(p_file);
	struct stat flags;
	bool success = (stat(file.utf8().get_data(), &flags) == 0);

	if (success) {
		return flags.st_mtime;
	} else {
		print_line("Cannot open: " + p_file);

		ERR_FAIL_V(0);
	};
}

Error FileAccessUnix::_chmod(const String &p_path, int p_mod) {
	int err = chmod(p_path.utf8().get_data(), p_mod);
	if (!err) {
		return OK;
	}

	return FAILED;
}

FileAccess *FileAccessUnix::create_libc() {

	return memnew(FileAccessUnix);
}

CloseNotificationFunc FileAccessUnix::close_notification_func = NULL;

FileAccessUnix::FileAccessUnix() {

	f = NULL;
	flags = 0;
	last_error = OK;
}
FileAccessUnix::~FileAccessUnix() {

	close();
}

#endif
