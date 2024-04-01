/*************************************************************************/
/*  audio_driver_wasapi.cpp                                              */
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
#ifdef WASAPI_ENABLED

#include "audio_driver_wasapi.h"

#include "globals.h"
#include "os/os.h"

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

static bool default_device_changed = false;

class CMMNotificationClient : public IMMNotificationClient {
	LONG _cRef;
	IMMDeviceEnumerator *_pEnumerator;

public:
	CMMNotificationClient() :
			_cRef(1),
			_pEnumerator(NULL) {}
	~CMMNotificationClient() {
		if ((_pEnumerator) != NULL) {
			(_pEnumerator)->Release();
			(_pEnumerator) = NULL;
		}
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		return InterlockedIncrement(&_cRef);
	}

	ULONG STDMETHODCALLTYPE Release() {
		ULONG ulRef = InterlockedDecrement(&_cRef);
		if (0 == ulRef) {
			delete this;
		}
		return ulRef;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) {
		if (IID_IUnknown == riid) {
			AddRef();
			*ppvInterface = (IUnknown *)this;
		} else if (__uuidof(IMMNotificationClient) == riid) {
			AddRef();
			*ppvInterface = (IMMNotificationClient *)this;
		} else {
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) {
		return S_OK;
	};

	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) {
		if (flow == eRender && role == eConsole) {
			default_device_changed = true;
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
		return S_OK;
	}
};

static CMMNotificationClient notif_client;

Error AudioDriverWASAPI::init_device(bool reinit) {

	WAVEFORMATEX *pwfex;
	IMMDeviceEnumerator *enumerator = NULL;
	IMMDevice *device = NULL;

	CoInitialize(NULL);

	HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **)&enumerator);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	if (reinit) {
		// In case we're trying to re-initialize the device prevent throwing this error on the console,
		// otherwise if there is currently no device available this will spam the console.
		if (hr != S_OK) {
			return ERR_CANT_OPEN;
		}
	} else {
		ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);
	}

	hr = enumerator->RegisterEndpointNotificationCallback(&notif_client);
	if (hr != S_OK) {
		ERR_PRINT("WASAPI: RegisterEndpointNotificationCallback error");
	}

	hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&audio_client);
	if (reinit) {
		if (hr != S_OK) {
			return ERR_CANT_OPEN;
		}
	} else {
		ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);
	}

	hr = audio_client->GetMixFormat(&pwfex);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	// Since we're using WASAPI Shared Mode we can't control any of these, we just tag along
	wasapi_channels = pwfex->nChannels;
	mix_rate = pwfex->nSamplesPerSec;
	format_tag = pwfex->wFormatTag;
	bits_per_sample = pwfex->wBitsPerSample;

	switch (wasapi_channels) {
		case 2: // Stereo
			//case 6: // Surround 5.1
			//case 8: // Surround 7.1
			channels = wasapi_channels;
			break;

		default:
			WARN_PRINTS("WASAPI: Unsupported number of channels (" + itos(wasapi_channels) + ")");
			channels = 2;
	}

	if (format_tag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE *)pwfex;

		if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
			format_tag = WAVE_FORMAT_PCM;
		} else if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			format_tag = WAVE_FORMAT_IEEE_FLOAT;
		} else {
			ERR_PRINT("WASAPI: Format not supported");
			ERR_FAIL_V(ERR_CANT_OPEN);
		}
	} else {
		if (format_tag != WAVE_FORMAT_PCM && format_tag != WAVE_FORMAT_IEEE_FLOAT) {
			ERR_PRINT("WASAPI: Format not supported");
			ERR_FAIL_V(ERR_CANT_OPEN);
		}
	}

	hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, pwfex, NULL);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	event = CreateEvent(NULL, FALSE, FALSE, NULL);
	ERR_FAIL_COND_V(event == NULL, ERR_CANT_OPEN);

	hr = audio_client->SetEventHandle(event);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	hr = audio_client->GetService(IID_IAudioRenderClient, (void **)&render_client);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	UINT32 max_frames;
	hr = audio_client->GetBufferSize(&max_frames);
	ERR_FAIL_COND_V(hr != S_OK, ERR_CANT_OPEN);

	// Due to WASAPI Shared Mode we have no control of the buffer size
	buffer_frames = max_frames;

	// Sample rate is independent of channels (ref: https://stackoverflow.com/questions/11048825/audio-sample-frequency-rely-on-channels)
	buffer_size = buffer_frames * channels;
	samples_in.resize(buffer_size);

	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("audio buffer frames: " + itos(buffer_frames) + " calculated latency: " + itos(buffer_frames * 1000 / mix_rate) + "ms");
	}

	return OK;
}

Error AudioDriverWASAPI::finish_device() {

	if (audio_client) {
		if (active) {
			audio_client->Stop();
			audio_client->Release();
			audio_client = NULL;
			active = false;
		}
	}

	if (render_client) {
		render_client->Release();
		render_client = NULL;
	}

	if (audio_client) {
		audio_client->Release();
		audio_client = NULL;
	}

	return OK;
}

Error AudioDriverWASAPI::init() {

	Error err = init_device();
	if (err != OK) {
		ERR_PRINT("WASAPI: init_device error");
		ERR_FAIL_V(err);
	}

	active = false;
	exit_thread = false;
	thread_exited = false;

	mutex = Mutex::create(true);
	thread = Thread::create(thread_func, this);

	return OK;
}

Error AudioDriverWASAPI::reopen() {
	Error err = finish_device();
	if (err != OK) {
		ERR_PRINT("WASAPI: finish_device error");
	} else {
		err = init_device();
		if (err != OK) {
			ERR_PRINT("WASAPI: init_device error");
		} else {
			start();
		}
	}

	return err;
}

int AudioDriverWASAPI::get_mix_rate() const {

	return mix_rate;
}

AudioDriverSW::OutputFormat AudioDriverWASAPI::get_output_format() const {

	return OUTPUT_STEREO;
}

void AudioDriverWASAPI::write_sample(AudioDriverWASAPI *ad, BYTE *buffer, int i, int32_t sample) {
	if (ad->format_tag == WAVE_FORMAT_PCM) {
		switch (ad->bits_per_sample) {
			case 8:
				((int8_t *)buffer)[i] = sample >> 24;
				break;

			case 16:
				((int16_t *)buffer)[i] = sample >> 16;
				break;

			case 24:
				((int8_t *)buffer)[i * 3 + 2] = sample >> 24;
				((int8_t *)buffer)[i * 3 + 1] = sample >> 16;
				((int8_t *)buffer)[i * 3 + 0] = sample >> 8;
				break;

			case 32:
				((int32_t *)buffer)[i] = sample;
				break;
		}
	} else if (ad->format_tag == WAVE_FORMAT_IEEE_FLOAT) {
		((float *)buffer)[i] = (sample >> 16) / 32768.f;
	} else {
		ERR_PRINT("WASAPI: Unknown format tag");
		ad->exit_thread = true;
	}
}

void AudioDriverWASAPI::thread_func(void *p_udata) {

	AudioDriverWASAPI *ad = (AudioDriverWASAPI *)p_udata;

	while (!ad->exit_thread) {
		if (ad->active) {
			ad->lock();

			ad->audio_server_process(ad->buffer_frames, ad->samples_in.ptr());

			ad->unlock();
		} else {
			for (unsigned int i = 0; i < ad->buffer_size; i++) {
				ad->samples_in[i] = 0;
			}
		}

		unsigned int left_frames = ad->buffer_frames;
		unsigned int buffer_idx = 0;
		while (left_frames > 0 && ad->audio_client) {
			WaitForSingleObject(ad->event, 1000);

			UINT32 cur_frames;
			HRESULT hr = ad->audio_client->GetCurrentPadding(&cur_frames);
			if (hr == S_OK) {
				// Check how much frames are available on the WASAPI buffer
				UINT32 avail_frames = ad->buffer_frames - cur_frames;
				UINT32 write_frames = avail_frames > left_frames ? left_frames : avail_frames;

				BYTE *buffer = NULL;
				hr = ad->render_client->GetBuffer(write_frames, &buffer);
				if (hr == S_OK) {
					// We're using WASAPI Shared Mode so we must convert the buffer

					if (ad->channels == ad->wasapi_channels) {
						for (unsigned int i = 0; i < write_frames * ad->channels; i++) {
							ad->write_sample(ad, buffer, i, ad->samples_in[buffer_idx++]);
						}
					} else {
						for (unsigned int i = 0; i < write_frames; i++) {
							for (unsigned int j = 0; j < MIN(ad->channels, ad->wasapi_channels); j++) {
								ad->write_sample(ad, buffer, i * ad->wasapi_channels + j, ad->samples_in[buffer_idx++]);
							}
							if (ad->wasapi_channels > ad->channels) {
								for (unsigned int j = ad->channels; j < ad->wasapi_channels; j++) {
									ad->write_sample(ad, buffer, i * ad->wasapi_channels + j, 0);
								}
							}
						}
					}

					hr = ad->render_client->ReleaseBuffer(write_frames, 0);
					if (hr != S_OK) {
						ERR_PRINT("WASAPI: Release buffer error");
					}

					left_frames -= write_frames;
				} else if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
					// Device is not valid anymore, reopen it

					Error err = ad->finish_device();
					if (err != OK) {
						ERR_PRINT("WASAPI: finish_device error");
					} else {
						// We reopened the device and samples_in may have resized, so invalidate the current left_frames
						left_frames = 0;
					}
				} else {
					ERR_PRINT("WASAPI: Get buffer error");
					ad->exit_thread = true;
				}
			} else if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
				// Device is not valid anymore, reopen it

				Error err = ad->finish_device();
				if (err != OK) {
					ERR_PRINT("WASAPI: finish_device error");
				} else {
					// We reopened the device and samples_in may have resized, so invalidate the current left_frames
					left_frames = 0;
				}
			} else {
				ERR_PRINT("WASAPI: GetCurrentPadding error");
			}
		}

		if (default_device_changed) {
			Error err = ad->finish_device();
			if (err != OK) {
				ERR_PRINT("WASAPI: finish_device error");
			}

			default_device_changed = false;
		}

		if (!ad->audio_client) {
			Error err = ad->init_device(true);
			if (err == OK) {
				ad->start();
			}
		}
	}

	ad->thread_exited = true;
}

void AudioDriverWASAPI::start() {

	if (audio_client) {
		HRESULT hr = audio_client->Start();
		if (hr != S_OK) {
			ERR_PRINT("WASAPI: Start failed");
		} else {
			active = true;
		}
	}
}

void AudioDriverWASAPI::lock() {

	if (mutex)
		mutex->lock();
}

void AudioDriverWASAPI::unlock() {

	if (mutex)
		mutex->unlock();
}

void AudioDriverWASAPI::finish() {

	if (thread) {
		exit_thread = true;
		Thread::wait_to_finish(thread);

		memdelete(thread);
		thread = NULL;
	}

	finish_device();

	if (mutex) {
		memdelete(mutex);
		mutex = NULL;
	}

	samples_in.clear();
}

AudioDriverWASAPI::AudioDriverWASAPI() {

	audio_client = NULL;
	render_client = NULL;
	mutex = NULL;
	thread = NULL;

	format_tag = 0;
	bits_per_sample = 0;

	samples_in.clear();

	buffer_size = 0;
	channels = 0;
	wasapi_channels = 0;
	mix_rate = 44100;
	buffer_frames = 0;

	thread_exited = false;
	exit_thread = false;
	active = false;
}

#endif
