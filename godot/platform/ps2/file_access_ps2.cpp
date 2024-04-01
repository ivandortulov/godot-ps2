#include "file_access_ps2.hpp"

#include "core/os/os.h"

#include <sys/stat.h>
#include <sys/types.h>


CloseNotificationFunc FileAccessPS2::close_notification_func = NULL;

FileAccessPS2::FileAccessPS2() : FileAccess()
{
	f = NULL;
	flags = 0;
	last_error = OK;
}

FileAccessPS2::~FileAccessPS2()
{
	close();
}

Error FileAccessPS2::_open(const String& p_path, int p_mode_flags)
{
	if (f)
		fclose(f);
	f = NULL;

	path = fix_path(p_path);

	ERR_FAIL_COND_V(f, ERR_ALREADY_IN_USE);
	const char* mode_string;

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

	struct stat st;
	if (stat(path.utf8().get_data(), &st) == 0)
	{

		if (!S_ISREG(st.st_mode))
			return ERR_FILE_CANT_OPEN;
	};

	if (is_backup_save_enabled() && p_mode_flags & WRITE && !(p_mode_flags & READ))
	{
		save_path = path;
		path = path + ".tmp";
	}

	f = fopen(path.utf8().get_data(), mode_string);

	if (f == NULL)
	{
		last_error = ERR_FILE_CANT_OPEN;
		return ERR_FILE_CANT_OPEN;
	}
	else
	{
		last_error = OK;
		flags = p_mode_flags;
		return OK;
	}
}

void FileAccessPS2::close()
{
	if (!f)
		return;
	fclose(f);
	f = NULL;
	if (close_notification_func)
	{
		close_notification_func(path, flags);
	}
	if (save_path != "")
	{
		int rename_error = rename(
			(save_path + ".tmp").utf8().get_data(),
			save_path.utf8().get_data());

		if (rename_error && close_fail_notify) {
			close_fail_notify(save_path);
		}

		save_path = "";
		ERR_FAIL_COND(rename_error != 0);
	}
}

bool FileAccessPS2::is_open() const
{
	return (f != NULL);
}

void FileAccessPS2::seek(size_t p_position)
{
	ERR_FAIL_COND(!f);

	last_error = OK;
	if (fseek(f, p_position, SEEK_SET))
	{
		check_errors();
	}
}

void FileAccessPS2::seek_end(int64_t p_position)
{
	ERR_FAIL_COND(!f);
	if (fseek(f, p_position, SEEK_END))
	{
		check_errors();
	}
}

size_t FileAccessPS2::get_pos() const
{
	size_t aux_position = 0;
	if (!(aux_position = ftell(f)))
	{
		check_errors();
	};
	return aux_position;
}

size_t FileAccessPS2::get_len() const
{
	ERR_FAIL_COND_V(!f, 0);

	FileAccessPS2* faps2 = const_cast<FileAccessPS2*>(this);
	int pos = faps2->get_pos();
	faps2->seek_end();
	int size = faps2->get_pos();
	faps2->seek(pos);

	return size;
}

bool FileAccessPS2::eof_reached() const
{
	return last_error == ERR_FILE_EOF;
}

uint8_t FileAccessPS2::get_8() const
{
	ERR_FAIL_COND_V(!f, 0);
	uint8_t b;
	if (fread(&b, 1, 1, f) == 0)
	{
		check_errors();
	};

	return b;
}

int FileAccessPS2::get_buffer(uint8_t* p_dst, int p_length) const
{
	ERR_FAIL_COND_V(!f, -1);
	int read = fread(p_dst, 1, p_length, f);
	check_errors();
	return read;
}

Error FileAccessPS2::get_error() const
{
	return last_error;
}

void FileAccessPS2::store_8(uint8_t p_dest)
{
	ERR_FAIL_COND(!f);
	fwrite(&p_dest, 1, 1, f);
}

void FileAccessPS2::store_buffer(const uint8_t* p_src, int p_length)
{
	ERR_FAIL_COND(!f);
	ERR_FAIL_COND(fwrite(p_src, 1, p_length, f) != p_length);
}

bool FileAccessPS2::file_exists(const String& p_path)
{
	FILE* g;
	String filename = fix_path(p_path);
	g = fopen(filename.utf8().get_data(), "rb");
	if (g == NULL)
	{
		return false;
	}
	else
	{
		fclose(g);
		return true;
	}
}

uint64_t FileAccessPS2::_get_modified_time(const String& p_file)
{
	String file = fix_path(p_file);
	struct stat flags;
	bool success = (stat(file.utf8().get_data(), &flags) == 0);

	if (success)
	{
		return flags.st_mtime;
	}
	else
	{
		print_line("Cannot open: " + p_file);

		ERR_FAIL_V(0);
	};
}

Error FileAccessPS2::_chmod(const String& p_path, int p_mod)
{
	int err = chmod(p_path.utf8().get_data(), p_mod);
	if (!err) {
		return OK;
	}

	return FAILED;
}

void FileAccessPS2::check_errors() const
{
	ERR_FAIL_COND(!f);

	if (feof(f)) {

		last_error = ERR_FILE_EOF;
	}
}

FileAccess* FileAccessPS2::create_ps2()
{
	return memnew(FileAccessPS2);
}