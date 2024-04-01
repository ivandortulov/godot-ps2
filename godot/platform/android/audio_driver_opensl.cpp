/*************************************************************************/
/*  audio_driver_opensl.cpp                                             */
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
#include "audio_driver_opensl.h"
#include <string.h>

#define MAX_NUMBER_INTERFACES 3
#define MAX_NUMBER_OUTPUT_DEVICES 6

/* Structure for passing information to callback function */

void AudioDriverOpenSL::_buffer_callback(SLAndroidSimpleBufferQueueItf queueItf) {

	bool mix = true;

	if (pause) {
		mix = false;
	} else if (mutex) {
		mix = mutex->try_lock() == OK;
	}

	if (mix) {
		audio_server_process(buffer_size, mixdown_buffer);
	} else {

		int32_t *src_buff = mixdown_buffer;
		for (int i = 0; i < buffer_size * 2; i++) {
			src_buff[i] = 0;
		}
	}

	if (mutex && mix)
		mutex->unlock();

	const int32_t *src_buff = mixdown_buffer;

	int16_t *ptr = (int16_t *)buffers[last_free];
	last_free = (last_free + 1) % BUFFER_COUNT;

	for (int i = 0; i < buffer_size * 2; i++) {

		ptr[i] = src_buff[i] >> 16;
	}

	(*queueItf)->Enqueue(queueItf, ptr, 4 * buffer_size);
}

void AudioDriverOpenSL::_buffer_callbacks(SLAndroidSimpleBufferQueueItf queueItf, void *pContext) {

	AudioDriverOpenSL *ad = (AudioDriverOpenSL *)pContext;

	ad->_buffer_callback(queueItf);
}

AudioDriverOpenSL *AudioDriverOpenSL::s_ad = NULL;

const char *AudioDriverOpenSL::get_name() const {

	return "Android";
}

Error AudioDriverOpenSL::init() {

	SLresult
			res;
	SLEngineOption EngineOption[] = {
		(SLuint32)SL_ENGINEOPTION_THREADSAFE,
		(SLuint32)SL_BOOLEAN_TRUE

	};
	res = slCreateEngine(&sl, 1, EngineOption, 0, NULL, NULL);
	if (res != SL_RESULT_SUCCESS) {

		ERR_EXPLAIN("Could not Initialize OpenSL");
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}
	res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
	if (res != SL_RESULT_SUCCESS) {

		ERR_EXPLAIN("Could not Realize OpenSL");
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}

	return OK;
}

void AudioDriverOpenSL::start() {

	mutex = Mutex::create();
	active = false;

	SLint32 numOutputs = 0;
	SLuint32 deviceID = 0;
	SLresult res;

	buffer_size = 1024;

	for (int i = 0; i < BUFFER_COUNT; i++) {

		buffers[i] = memnew_arr(int16_t, buffer_size * 2);
		memset(buffers[i], 0, buffer_size * 4);
	}

	mixdown_buffer = memnew_arr(int32_t, buffer_size * 2);

	/* Callback context for the buffer queue callback function */

	/* Get the SL Engine Interface which is implicit */
	res = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void *)&EngineItf);

	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	/* Initialize arrays required[] and iidArray[] */
	SLboolean required[MAX_NUMBER_INTERFACES];
	SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

	{
		const SLInterfaceID ids[1] = { SL_IID_ENVIRONMENTALREVERB };
		const SLboolean req[1] = { SL_BOOLEAN_FALSE };
		res = (*EngineItf)->CreateOutputMix(EngineItf, &OutputMix, 0, ids, req);
	}

	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	// Realizing the Output Mix object in synchronous mode.
	res = (*OutputMix)->Realize(OutputMix, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, BUFFER_COUNT };

	/* Setup the format of the content in the buffer queue */
	pcm.formatType = SL_DATAFORMAT_PCM;
	pcm.numChannels = 2;
	pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
	pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
#ifdef BIG_ENDIAN_ENABLED
	pcm.endianness = SL_BYTEORDER_BIGENDIAN;
#else
	pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
#endif
	audioSource.pFormat = (void *)&pcm;
	audioSource.pLocator = (void *)&loc_bufq;

	/* Setup the data sink structure */
	locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	locator_outputmix.outputMix = OutputMix;
	audioSink.pLocator = (void *)&locator_outputmix;
	audioSink.pFormat = NULL;

	/* Set arrays required[] and iidArray[] for SEEK interface (PlayItf is implicit) */
	required[0] = SL_BOOLEAN_TRUE;
	iidArray[0] = SL_IID_BUFFERQUEUE;
	/* Create the music player */
	{
		const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND };
		const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

		res = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &audioSource, &audioSink, 1, ids, req);
		ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	}
	/* Realizing the player in synchronous mode. */
	res = (*player)->Realize(player, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	/* Get seek and play interfaces */
	res = (*player)->GetInterface(player, SL_IID_PLAY, (void *)&playItf);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	res = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE,
			(void *)&bufferQueueItf);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	/* Setup to receive buffer queue event callbacks */
	res = (*bufferQueueItf)->RegisterCallback(bufferQueueItf, _buffer_callbacks, this);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	last_free = 0;

	//fill up buffers
	for (int i = 0; i < BUFFER_COUNT; i++) {
		/* Enqueue a few buffers to get the ball rolling */
		res = (*bufferQueueItf)->Enqueue(bufferQueueItf, buffers[i], 4 * buffer_size); /* Size given in */
	}

	res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	active = true;
}

int AudioDriverOpenSL::get_mix_rate() const {

	return 44100;
}

AudioDriverSW::OutputFormat AudioDriverOpenSL::get_output_format() const {

	return OUTPUT_STEREO;
}

void AudioDriverOpenSL::lock() {

	if (active && mutex)
		mutex->lock();
}

void AudioDriverOpenSL::unlock() {

	if (active && mutex)
		mutex->unlock();
}

void AudioDriverOpenSL::finish() {

	(*sl)->Destroy(sl);
}

void AudioDriverOpenSL::set_pause(bool p_pause) {

	SLresult res;
	SLuint32 playState;

	pause = p_pause;

	if (active) {
		res = (*playItf)->GetPlayState(playItf, &playState);

		ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

		if (pause && playState == SL_PLAYSTATE_PLAYING) {
			res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED);

			ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
		} else if (!pause && playState == SL_PLAYSTATE_PAUSED) {
			res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

			ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
		}
	}
}

AudioDriverOpenSL::AudioDriverOpenSL() {
	s_ad = this;
	pause = false;
}
