/*************************************************************************/
/*  audio_driver_dummy.cpp                                               */
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
#include "audio_driver_dummy.h"

#include "globals.h"
#include "os/os.h"

Error AudioDriverDummy::init() {

	active = false;
	thread_exited = false;
	exit_thread = false;
	pcm_open = false;
	samples_in = NULL;

	mix_rate = 44100;
	output_format = OUTPUT_STEREO;
	channels = 2;

	int latency = GLOBAL_DEF("audio/output_latency", 25);
	buffer_size = closest_power_of_2(latency * mix_rate / 1000);

	samples_in = memnew_arr(int32_t, buffer_size * channels);

	mutex = Mutex::create();
	thread = Thread::create(AudioDriverDummy::thread_func, this);

	return OK;
};

void AudioDriverDummy::thread_func(void *p_udata) {

	AudioDriverDummy *ad = (AudioDriverDummy *)p_udata;

	uint64_t usdelay = (ad->buffer_size / float(ad->mix_rate)) * 1000000;

	while (!ad->exit_thread) {

		if (!ad->active) {

		} else {

			ad->lock();

			ad->audio_server_process(ad->buffer_size, ad->samples_in);

			ad->unlock();
		};

		OS::get_singleton()->delay_usec(usdelay);
	};

	ad->thread_exited = true;
};

void AudioDriverDummy::start() {

	active = true;
};

int AudioDriverDummy::get_mix_rate() const {

	return mix_rate;
};

AudioDriverSW::OutputFormat AudioDriverDummy::get_output_format() const {

	return output_format;
};
void AudioDriverDummy::lock() {

	if (!thread || !mutex)
		return;
	mutex->lock();
};
void AudioDriverDummy::unlock() {

	if (!thread || !mutex)
		return;
	mutex->unlock();
};

void AudioDriverDummy::finish() {

	if (!thread)
		return;

	exit_thread = true;
	Thread::wait_to_finish(thread);

	if (samples_in) {
		memdelete_arr(samples_in);
	};

	memdelete(thread);
	if (mutex)
		memdelete(mutex);
	thread = NULL;
};

AudioDriverDummy::AudioDriverDummy() {

	mutex = NULL;
	thread = NULL;
};

AudioDriverDummy::~AudioDriverDummy(){

};
