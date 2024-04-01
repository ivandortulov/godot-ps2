/*************************************************************************/
/*  spatial_sound_server.h                                               */
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
#ifndef SPATIAL_SOUND_SERVER_H
#define SPATIAL_SOUND_SERVER_H

#include "bsp_tree.h"
#include "object.h"
#include "servers/audio_server.h"

class SpatialSoundServer : public Object {
	OBJ_TYPE(SpatialSoundServer, Object);

	static SpatialSoundServer *singleton;

public:
	enum {
		SOURCE_INVALID_VOICE = -1,
		SOURCE_NEXT_VOICE = -2,
	};

	typedef int SourceVoiceID;

	/* SPACE */
	virtual RID space_create() = 0;

	/* ROOM */

	virtual RID room_create() = 0;
	virtual void room_set_space(RID p_room, RID p_space) = 0;
	virtual RID room_get_space(RID p_room) const = 0;

	virtual void room_set_bounds(RID p_room, const BSP_Tree &p_bounds) = 0;
	virtual BSP_Tree room_get_bounds(RID p_room) const = 0;
	virtual void room_set_transform(RID p_room, const Transform &p_transform) = 0;
	virtual Transform room_get_transform(RID p_room) const = 0;

	enum RoomParam {
		ROOM_PARAM_SPEED_OF_SOUND_SCALE,
		ROOM_PARAM_DOPPLER_FACTOR,
		ROOM_PARAM_PITCH_SCALE,
		ROOM_PARAM_VOLUME_SCALE_DB,
		ROOM_PARAM_REVERB_SEND,
		ROOM_PARAM_CHORUS_SEND,
		ROOM_PARAM_ATTENUATION_SCALE,
		ROOM_PARAM_ATTENUATION_HF_CUTOFF,
		ROOM_PARAM_ATTENUATION_HF_FLOOR_DB,
		ROOM_PARAM_ATTENUATION_HF_RATIO_EXP,
		ROOM_PARAM_ATTENUATION_REVERB_SCALE,
		ROOM_PARAM_MAX
	};

	virtual void room_set_param(RID p_room, RoomParam p_param, float p_value) = 0;
	virtual float room_get_param(RID p_room, RoomParam p_param) const = 0;

	enum RoomReverb {
		ROOM_REVERB_SMALL,
		ROOM_REVERB_MEDIUM,
		ROOM_REVERB_LARGE,
		ROOM_REVERB_HALL
	};

	virtual void room_set_reverb(RID p_room, RoomReverb p_reverb) = 0;
	virtual RoomReverb room_get_reverb(RID p_room) const = 0;

	virtual void room_set_level(RID p_room, int p_level) = 0;
	virtual int room_get_level(RID p_room) const = 0;

	//useful for underwater or rooms with very strange conditions
	virtual void room_set_force_params_to_all_sources(RID p_room, bool p_force) = 0;
	virtual bool room_is_forcing_params_to_all_sources(RID p_room) const = 0;

	/* SOURCE */

	virtual RID source_create(RID p_space) = 0;

	virtual void source_set_transform(RID p_source, const Transform &p_transform) = 0;
	virtual Transform source_get_transform(RID p_source) const = 0;

	virtual void source_set_polyphony(RID p_source, int p_voice_count) = 0;
	virtual int source_get_polyphony(RID p_source) const = 0;

	enum SourceParam {

		SOURCE_PARAM_VOLUME_DB,
		SOURCE_PARAM_PITCH_SCALE,
		SOURCE_PARAM_ATTENUATION_MIN_DISTANCE,
		SOURCE_PARAM_ATTENUATION_MAX_DISTANCE,
		SOURCE_PARAM_ATTENUATION_DISTANCE_EXP,
		SOURCE_PARAM_EMISSION_CONE_DEGREES,
		SOURCE_PARAM_EMISSION_CONE_ATTENUATION_DB,
		SOURCE_PARAM_MAX
	};

	virtual void source_set_param(RID p_source, SourceParam p_param, float p_value) = 0;
	virtual float source_get_param(RID p_source, SourceParam p_param) const = 0;

	virtual void source_set_audio_stream(RID p_source, AudioServer::AudioStream *p_stream) = 0; //null to unset
	virtual SourceVoiceID source_play_sample(RID p_source, RID p_sample, int p_mix_rate, int p_voice = SOURCE_NEXT_VOICE, int p_priority = 0) = 0;
	//voices
	virtual void source_voice_set_pitch_scale(RID p_source, SourceVoiceID p_voice, float p_pitch_scale) = 0;
	virtual void source_voice_set_volume_scale_db(RID p_source, SourceVoiceID p_voice, float p_volume_db) = 0;

	virtual bool source_is_voice_active(RID p_source, SourceVoiceID p_voice) const = 0;
	virtual void source_stop_voice(RID p_source, SourceVoiceID p_voice) = 0;

	/* LISTENER */

	enum ListenerParam {

		LISTENER_PARAM_VOLUME_SCALE_DB,
		LISTENER_PARAM_PITCH_SCALE,
		LISTENER_PARAM_ATTENUATION_SCALE,
		LISTENER_PARAM_RECEPTION_CONE_DEGREES,
		LISTENER_PARAM_RECEPTION_CONE_ATTENUATION_DB,
		LISTENER_PARAM_MAX
	};

	virtual RID listener_create() = 0;
	virtual void listener_set_space(RID p_listener, RID p_space) = 0;

	virtual void listener_set_transform(RID p_listener, const Transform &p_transform) = 0;
	virtual Transform listener_get_transform(RID p_listener) const = 0;

	virtual void listener_set_param(RID p_listener, ListenerParam p_param, float p_value) = 0;
	virtual float listener_get_param(RID p_listener, ListenerParam p_param) const = 0;

	/* MISC */

	virtual void free(RID p_id) = 0;

	virtual void init() = 0;
	virtual void update(float p_delta) = 0;
	virtual void finish() = 0;

	static SpatialSoundServer *get_singleton();

	SpatialSoundServer();
};

#endif // SPATIAL_SOUND_SERVER_H
