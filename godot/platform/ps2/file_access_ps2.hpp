#ifndef _FILE_ACCESS_PS2_
#define _FILE_ACCESS_PS2_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "core/os/file_access.h"

typedef void (*CloseNotificationFunc)(const String& p_file, int p_flags);

class FileAccessPS2 : public FileAccess
{
	static FileAccess* create_ps2();

public:
	FileAccessPS2();
	virtual ~FileAccessPS2();

public:
	virtual Error _open(const String& p_path, int p_mode_flags); ///< open a file
	virtual void close(); ///< close a file
	virtual bool is_open() const; ///< true when file is open

	virtual void seek(size_t p_position); ///< seek to a given position
	virtual void seek_end(int64_t p_position = 0); ///< seek from the end of file
	virtual size_t get_pos() const; ///< get position in the file
	virtual size_t get_len() const; ///< get size of the file

	virtual bool eof_reached() const; ///< reading passed EOF

	virtual uint8_t get_8() const; ///< get a byte
	virtual int get_buffer(uint8_t* p_dst, int p_length) const;

	virtual Error get_error() const; ///< get last error

	virtual void store_8(uint8_t p_dest); ///< store a byte
	virtual void store_buffer(const uint8_t* p_src, int p_length); ///< store an array of bytes

	virtual bool file_exists(const String& p_path); ///< return true if a file exists

	virtual uint64_t _get_modified_time(const String& p_file);

	virtual Error _chmod(const String& p_path, int p_mod);

	static CloseNotificationFunc close_notification_func;

private:
	FILE* f;
	int flags;
	void check_errors() const;
	mutable Error last_error;
	String save_path;
	String path;
};

#endif