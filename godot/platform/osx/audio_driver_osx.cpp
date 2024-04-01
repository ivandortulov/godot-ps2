/*************************************************************************/
/*  audio_driver_osx.cpp                                                 */
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
#ifdef OSX_ENABLED

#include "audio_driver_osx.h"
#include "globals.h"
#include "os/os.h"

#define kOutputBus 0

static OSStatus outputDeviceAddressCB(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses, void *__nullable inClientData) {
	AudioDriverOSX *driver = (AudioDriverOSX *)inClientData;

	driver->reopen();

	return noErr;
}

Error AudioDriverOSX::initDevice() {
	AudioComponentDescription desc;
	zeromem(&desc, sizeof(desc));
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(NULL, &desc);
	ERR_FAIL_COND_V(comp == NULL, FAILED);

	OSStatus result = AudioComponentInstanceNew(comp, &audio_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	AudioStreamBasicDescription strdesc;

	// TODO: Implement this
	/*zeromem(&strdesc, sizeof(strdesc));
	UInt32 size = sizeof(strdesc);
	result = AudioUnitGetProperty(audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kOutputBus, &strdesc, &size);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	switch (strdesc.mChannelsPerFrame) {
		case 2: // Stereo
		case 6: // Surround 5.1
		case 8: // Surround 7.1
			channels = strdesc.mChannelsPerFrame;
			break;

		default:
			// Unknown number of channels, default to stereo
			channels = 2;
			break;
	}*/

	mix_rate = GLOBAL_DEF("audio/mix_rate", 44100);

	zeromem(&strdesc, sizeof(strdesc));
	strdesc.mFormatID = kAudioFormatLinearPCM;
	strdesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	strdesc.mChannelsPerFrame = channels;
	strdesc.mSampleRate = mix_rate;
	strdesc.mFramesPerPacket = 1;
	strdesc.mBitsPerChannel = 16;
	strdesc.mBytesPerFrame = strdesc.mBitsPerChannel * strdesc.mChannelsPerFrame / 8;
	strdesc.mBytesPerPacket = strdesc.mBytesPerFrame * strdesc.mFramesPerPacket;

	result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, kOutputBus, &strdesc, sizeof(strdesc));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	int latency = GLOBAL_DEF("audio/output_latency", 25);
	unsigned int buffer_size = closest_power_of_2(latency * mix_rate / 1000);

	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("audio buffer size: " + itos(buffer_size) + " calculated latency: " + itos(buffer_size * 1000 / mix_rate));
	}

	samples_in.resize(buffer_size);
	buffer_frames = buffer_size / channels;

	AURenderCallbackStruct callback;
	zeromem(&callback, sizeof(AURenderCallbackStruct));
	callback.inputProc = &AudioDriverOSX::output_callback;
	callback.inputProcRefCon = this;
	result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputBus, &callback, sizeof(callback));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	result = AudioUnitInitialize(audio_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	return OK;
}

Error AudioDriverOSX::finishDevice() {
	OSStatus result;

	if (active) {
		result = AudioOutputUnitStop(audio_unit);
		ERR_FAIL_COND_V(result != noErr, FAILED);

		active = false;
	}

	result = AudioUnitUninitialize(audio_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	return OK;
}

Error AudioDriverOSX::init() {
	OSStatus result;

	mutex = Mutex::create();
	active = false;
	channels = 2;

	outputDeviceAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	outputDeviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
	outputDeviceAddress.mElement = kAudioObjectPropertyElementMaster;

	result = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &outputDeviceAddress, &outputDeviceAddressCB, this);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	return initDevice();
};

Error AudioDriverOSX::reopen() {
	bool restart = false;

	lock();

	if (active) {
		restart = true;
	}

	Error err = finishDevice();
	if (err != OK) {
		ERR_PRINT("finishDevice failed");
		unlock();
		return err;
	}

	err = initDevice();
	if (err != OK) {
		ERR_PRINT("initDevice failed");
		unlock();
		return err;
	}

	if (restart) {
		start();
	}

	unlock();

	return OK;
}

OSStatus AudioDriverOSX::output_callback(void *inRefCon,
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData) {

	AudioDriverOSX *ad = (AudioDriverOSX *)inRefCon;

	if (!ad->active || !ad->try_lock()) {
		for (unsigned int i = 0; i < ioData->mNumberBuffers; i++) {
			AudioBuffer *abuf = &ioData->mBuffers[i];
			zeromem(abuf->mData, abuf->mDataByteSize);
		};
		return 0;
	};

	for (unsigned int i = 0; i < ioData->mNumberBuffers; i++) {

		AudioBuffer *abuf = &ioData->mBuffers[i];
		int frames_left = inNumberFrames;
		int16_t *out = (int16_t *)abuf->mData;

		while (frames_left) {

			int frames = MIN(frames_left, ad->buffer_frames);
			ad->audio_server_process(frames, ad->samples_in.ptr());

			for (int j = 0; j < frames * ad->channels; j++) {

				out[j] = ad->samples_in[j] >> 16;
			}

			frames_left -= frames;
			out += frames * ad->channels;
		};
	};

	ad->unlock();

	return 0;
};

void AudioDriverOSX::start() {
	if (!active) {
		OSStatus result = AudioOutputUnitStart(audio_unit);
		if (result != noErr) {
			ERR_PRINT("AudioOutputUnitStart failed");
		} else {
			active = true;
		}
	}
};

int AudioDriverOSX::get_mix_rate() const {
	return 44100;
};

AudioDriverSW::OutputFormat AudioDriverOSX::get_output_format() const {
	return OUTPUT_STEREO;
};

void AudioDriverOSX::lock() {
	if (mutex)
		mutex->lock();
};

void AudioDriverOSX::unlock() {
	if (mutex)
		mutex->unlock();
};

bool AudioDriverOSX::try_lock() {
	if (mutex)
		return mutex->try_lock() == OK;
	return true;
}

void AudioDriverOSX::finish() {
	finishDevice();

	OSStatus result = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &outputDeviceAddress, &outputDeviceAddressCB, this);
	if (result != noErr) {
		ERR_PRINT("AudioObjectRemovePropertyListener failed");
	}

	AURenderCallbackStruct callback;
	zeromem(&callback, sizeof(AURenderCallbackStruct));
	result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputBus, &callback, sizeof(callback));
	if (result != noErr) {
		ERR_PRINT("AudioUnitSetProperty failed");
	}

	if (mutex) {
		memdelete(mutex);
		mutex = NULL;
	}
};

AudioDriverOSX::AudioDriverOSX() {
	active = false;
	mutex = NULL;

	mix_rate = 44100;
	channels = 2;
	samples_in.clear();
};

AudioDriverOSX::~AudioDriverOSX(){};

#endif
