#include "dir_access_ps2.hpp"

#include "os/memory.h"

#include <errno.h>
#include <stdio.h>

DirAccessPS2::DirAccessPS2() : DirAccess()
{
	dir_stream = 0;
	current_dir = ".";
	_cisdir = false;

	change_dir(current_dir);
}

DirAccessPS2::~DirAccessPS2()
{
	list_dir_end();
}

bool DirAccessPS2::list_dir_begin()
{
	list_dir_end();

	dir_stream = opendir(current_dir.utf8().get_data());
	if (!dir_stream)
	{
		return true;
	}

	return false;
}

bool DirAccessPS2::file_exists(String p_file)
{
	GLOBAL_LOCK_FUNCTION

		if (p_file.is_rel_path())
		{
			p_file = current_dir.plus_file(p_file);
		}

	p_file = fix_path(p_file);

	struct stat flags;
	bool success = (stat(p_file.utf8().get_data(), &flags) == 0);

	if (success && S_ISDIR(flags.st_mode))
	{
		success = false;
	}

	return success;
}

bool DirAccessPS2::dir_exists(String p_dir)
{
	GLOBAL_LOCK_FUNCTION

		if (p_dir.is_rel_path())
		{
			p_dir = get_current_dir().plus_file(p_dir);
		}

	p_dir = fix_path(p_dir);

	struct stat flags;
	bool success = (stat(p_dir.utf8().get_data(), &flags) == 0);

	if (success && S_ISDIR(flags.st_mode))
	{
		return true;
	}

	return false;
}

uint64_t DirAccessPS2::get_modified_time(String p_file)
{
	if (p_file.is_rel_path())
	{
		p_file = current_dir.plus_file(p_file);
	}

	p_file = fix_path(p_file);

	struct stat flags;
	bool success = (stat(p_file.utf8().get_data(), &flags) == 0);

	if (success)
	{
		return flags.st_mtime;
	}
	else
	{
		ERR_FAIL_V(0);
	};
	return 0;
};

String DirAccessPS2::get_next()
{
	if (!dir_stream)
	{
		return "";
	}
	dirent* entry;

	entry = readdir(dir_stream);

	if (entry == NULL)
	{
		list_dir_end();
		return "";
	}

	struct stat flags;

	String fname = fix_unicode_name(entry->d_name);

	String f = current_dir.plus_file(fname);

	if (stat(f.utf8().get_data(), &flags) == 0)
	{
		if (S_ISDIR(flags.st_mode))
		{
			_cisdir = true;
		}
		else
		{
			_cisdir = false;
		}

	}
	else
	{
		_cisdir = false;
	}

	_cishidden = (fname != "." && fname != ".." && fname.begins_with("."));

	return fname;
}

bool DirAccessPS2::current_is_dir() const
{
	return _cisdir;
}

bool DirAccessPS2::current_is_hidden() const
{
	return _cishidden;
}

void DirAccessPS2::list_dir_end()
{
	if (dir_stream)
	{
		closedir(dir_stream);
	}
	dir_stream = 0;
	_cisdir = false;
}

int DirAccessPS2::get_drive_count()
{
	return 0;
}

String DirAccessPS2::get_drive(int p_drive)
{
	return "";
}

Error DirAccessPS2::make_dir(String p_dir)
{
	GLOBAL_LOCK_FUNCTION

		if (p_dir.is_rel_path())
			p_dir = get_current_dir().plus_file(p_dir);

	p_dir = fix_path(p_dir);

	bool success = (mkdir(p_dir.utf8().get_data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0);
	int err = errno;

	if (success)
	{
		return OK;
	};

	if (err == EEXIST)
	{
		return ERR_ALREADY_EXISTS;
	};

	return ERR_CANT_CREATE;
}

Error DirAccessPS2::change_dir(String p_dir)
{
	GLOBAL_LOCK_FUNCTION
		p_dir = fix_path(p_dir);

	char real_current_dir_name[2048];
	getcwd(real_current_dir_name, 2048);
	String prev_dir;
	if (prev_dir.parse_utf8(real_current_dir_name))
		prev_dir = real_current_dir_name;

	chdir(current_dir.utf8().get_data());
	bool worked = (chdir(p_dir.utf8().get_data()) == 0);

	String base = _get_root_path();
	if (base != "")
	{
		getcwd(real_current_dir_name, 2048);
		String new_dir;
		new_dir.parse_utf8(real_current_dir_name);
		if (!new_dir.begins_with(base))
			worked = false;
	}

	if (worked)
	{
		getcwd(real_current_dir_name, 2048);
		if (current_dir.parse_utf8(real_current_dir_name))
			current_dir = real_current_dir_name; //no utf8, maybe latin?
	}

	chdir(prev_dir.utf8().get_data());
	return worked ? OK : ERR_INVALID_PARAMETER;
}

String DirAccessPS2::get_current_dir()
{
	String base = _get_root_path();
	if (base != "")
	{
		String bd = current_dir.replace_first(base, "");
		if (bd.begins_with("/"))
		{
			return _get_root_string() + bd.substr(1, bd.length());
		}
		else
		{
			return _get_root_string() + bd;
		}
	}
	return current_dir;
}

Error DirAccessPS2::rename(String p_path, String p_new_path)
{
	if (p_path.is_rel_path())
	{
		p_path = get_current_dir().plus_file(p_path);
	}

	p_path = fix_path(p_path);

	if (p_new_path.is_rel_path())
	{
		p_new_path = get_current_dir().plus_file(p_new_path);
	}

	p_new_path = fix_path(p_new_path);

	return ::rename(
		p_path.utf8().get_data(),
		p_new_path.utf8().get_data()) == 0 ? OK : FAILED;
}

Error DirAccessPS2::remove(String p_path)
{
	if (p_path.is_rel_path())
		p_path = get_current_dir().plus_file(p_path);

	p_path = fix_path(p_path);

	struct stat flags;
	if ((stat(p_path.utf8().get_data(), &flags) != 0))
		return FAILED;

	if (S_ISDIR(flags.st_mode))
		return ::rmdir(p_path.utf8().get_data()) == 0 ? OK : FAILED;
	else
		return ::unlink(p_path.utf8().get_data()) == 0 ? OK : FAILED;
}

size_t DirAccessPS2::get_space_left()
{
	return 0;
}

DirAccess* DirAccessPS2::create_ps2()
{
	return memnew(DirAccessPS2);
}