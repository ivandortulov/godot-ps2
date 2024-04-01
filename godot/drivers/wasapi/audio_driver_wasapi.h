/*************************************************************************/
/*  audio_driver_wasapi.h                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
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
#ifndef AUDIO_DRIVER_WASAPI_H
#define AUDIO_DRIVER_WASAPI_H

#ifdef WASAPI_ENABLED

#include "servers/audio/audio_server_sw.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

class AudioDriverWASAPI : public AudioDriverSW {

	HANDLE event;
	IAudioClient *audio_client;
	IAudioRenderClient *render_client;
	Mutex *mutex;
	Thread *thread;

	WORD format_tag;
	WORD bits_per_sample;

	Vector<int32_t> samples_in;

	unsigned int buffer_size;
	unsigned int channels;
	unsigned int wasapi_channels;
	int mix_rate;
	int buffer_frames;

	bool thread_exited;
	mutable bool exit_thread;
	bool active;

	_FORCE_INLINE_ void write_sample(AudioDriverWASAPI *ad, BYTE *buffer, int i, int32_t sample);
	static void thread_func(void *p_udata);

	Error init_device(bool reinit = false);
	Error finish_device();
	Error reopen();

public:
	virtual const char *get_name() const {
		return "WASAPI";
	}

	virtual Error init();
	virtual void start();
	virtual int get_mix_rate() const;
	virtual OutputFormat get_output_format() const;
	virtual void lock();
	virtual void unlock();
	virtual void finish();

	AudioDriverWASAPI();
};

#endif // AUDIO_DRIVER_WASAPI_H
#endif
