#ifndef _DIR_ACCESS_PS2_H_
#define _DIR_ACCESS_PS2_H_

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "os/dir_access.h"

class DirAccessPS2 : public DirAccess
{
public:
	DirAccessPS2();
	~DirAccessPS2();

protected:
	virtual String fix_unicode_name(const char* p_name) const { return String::utf8(p_name); }

public:
	virtual bool list_dir_begin();
	virtual String get_next();
	virtual bool current_is_dir() const;
	virtual bool current_is_hidden() const;

	virtual void list_dir_end();

	virtual int get_drive_count();
	virtual String get_drive(int p_drive);

	virtual Error change_dir(String p_dir);
	virtual String get_current_dir();
	virtual Error make_dir(String p_dir);

	virtual bool file_exists(String p_file);
	virtual bool dir_exists(String p_dir);

	virtual uint64_t get_modified_time(String p_file);

	virtual Error rename(String p_from, String p_to);
	virtual Error remove(String p_name);

	virtual size_t get_space_left();

private:
	static DirAccess* create_ps2();

private:
	DIR* dir_stream;

	String current_dir;
	bool _cisdir;
	bool _cishidden;
};

#endif