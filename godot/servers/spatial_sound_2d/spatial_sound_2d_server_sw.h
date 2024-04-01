/*************************************************************************/
/*  spatial_sound_2d_server_sw.h                                         */
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
#ifndef SPATIAL_SOUND_2D_SERVER_SW_H
#define SPATIAL_SOUND_2D_SERVER_SW_H

#include "servers/spatial_sound_2d_server.h"

#include "os/thread_safe.h"

class SpatialSound2DServerSW : public SpatialSound2DServer {

	OBJ_TYPE(SpatialSound2DServerSW, SpatialSound2DServer);

	_THREAD_SAFE_CLASS_

	enum {
		INTERNAL_BUFFER_SIZE = 4096,
		INTERNAL_BUFFER_MAX_CHANNELS = 4,
		VOICE_IS_STREAM = -1

	};

	struct InternalAudioStream : public AudioServer::AudioStream {

		::SpatialSound2DServerSW *owner;
		virtual int get_channel_count() const;
		virtual void set_mix_rate(int p_rate); //notify the stream of the mix rate
		virtual bool mix(int32_t *p_buffer, int p_frames);
		virtual void update();
	};

	InternalAudioStream *internal_audio_stream;
	RID internal_audio_stream_rid;
	int32_t *internal_buffer;
	int internal_buffer_channels;

	bool internal_buffer_mix(int32_t *p_buffer, int p_frames);

	struct Room;

	struct Space {

		RID default_room;
		Set<RID> rooms;
		Set<RID> sources;
		Set<RID> listeners;

		//Octree<Room> octree;
	};

	mutable RID_Owner<Space> space_owner;

	struct Room {
		RID space;
		Matrix32 transform;
		Matrix32 inverse_transform;
		DVector<Point2> bounds;
		RoomReverb reverb;
		float params[ROOM_PARAM_MAX];
		bool override_other_sources;
		//OctreeElementID octree_id;
		int level;

		Room();
	};

	mutable RID_Owner<Room> room_owner;

	struct Source {

		struct Voice {

			RID voice_rid;
			RID sample_rid;
			bool active;
			bool restart;
			int priority;
			float pitch_scale;
			float volume_scale;
			int sample_mix_rate;

			float last_volume;
			float last_filter_gain;
			float last_filter_cutoff;
			Vector2 last_panning;
			int last_mix_rate;
			RoomReverb last_reverb_room;
			float last_reverb_send;

			Voice();
			~Voice();
		};

		struct StreamData {

			Vector2 panning;
			RoomReverb reverb;
			float reverb_send;
			float volume;
			float filter_gain;
			float filter_cutoff;

			struct FilterState {

				float ha[2];
				float hb[2];
			} filter_state[4];

			StreamData() {

				reverb_send = 0;
				reverb = ROOM_REVERB_HALL;
				volume = 1.0;
				filter_gain = 1;
				filter_cutoff = 5000;
			}
		} stream_data;

		RID space;
		Matrix32 transform;
		float params[SOURCE_PARAM_MAX];
		AudioServer::AudioStream *stream;
		Vector<Voice> voices;
		int last_voice;

		Source();
	};

	mutable RID_Owner<Source> source_owner;

	struct Listener {

		RID space;
		Matrix32 transform;
		float params[LISTENER_PARAM_MAX];

		Listener();
	};

	mutable RID_Owner<Listener> listener_owner;

	struct ActiveVoice {

		Source *source;
		int voice;
		bool operator<(const ActiveVoice &p_voice) const { return (voice == p_voice.voice) ? (source < p_voice.source) : (voice < p_voice.voice); }
		ActiveVoice(Source *p_source = NULL, int p_voice = 0) {
			source = p_source;
			voice = p_voice;
		}
	};

	//	Room *cull_rooms[MAX_CULL_ROOMS];

	Set<Source *> streaming_sources;
	Set<ActiveVoice> active_voices;

	void _clean_up_owner(RID_OwnerBase *p_owner, const char *p_area);
	void _update_sources();

public:
	/* SPACE */
	virtual RID space_create();

	/* ROOM */

	virtual RID room_create();
	virtual void room_set_space(RID p_room, RID p_space);
	virtual RID room_get_space(RID p_room) const;

	virtual void room_set_bounds(RID p_room, const DVector<Point2> &p_bounds);
	virtual DVector<Point2> room_get_bounds(RID p_room) const;
	virtual void room_set_transform(RID p_room, const Matrix32 &p_transform);
	virtual Matrix32 room_get_transform(RID p_room) const;

	virtual void room_set_param(RID p_room, RoomParam p_param, float p_value);
	virtual float room_get_param(RID p_room, RoomParam p_param) const;

	virtual void room_set_level(RID p_room, int p_level);
	virtual int room_get_level(RID p_room) const;

	virtual void room_set_reverb(RID p_room, RoomReverb p_reverb);
	virtual RoomReverb room_get_reverb(RID p_room) const;

	//useful for underwater or rooms with very strange conditions
	virtual void room_set_force_params_to_all_sources(RID p_room, bool p_force);
	virtual bool room_is_forcing_params_to_all_sources(RID p_room) const;

	/* SOURCE */

	virtual RID source_create(RID p_space);

	virtual void source_set_polyphony(RID p_source, int p_voice_count);
	virtual int source_get_polyphony(RID p_source) const;

	virtual void source_set_transform(RID p_source, const Matrix32 &p_transform);
	virtual Matrix32 source_get_transform(RID p_source) const;

	virtual void source_set_param(RID p_source, SourceParam p_param, float p_value);
	virtual float source_get_param(RID p_source, SourceParam p_param) const;

	virtual void source_set_audio_stream(RID p_source, AudioServer::AudioStream *p_stream); //null to unset
	virtual SourceVoiceID source_play_sample(RID p_source, RID p_sample, int p_mix_rate, int p_voice = SOURCE_NEXT_VOICE, int p_priority = 0);
	/* VOICES */
	virtual void source_voice_set_pitch_scale(RID p_source, SourceVoiceID p_voice, float p_pitch_scale);
	virtual void source_voice_set_volume_scale_db(RID p_source, SourceVoiceID p_voice, float p_volume);

	virtual bool source_is_voice_active(RID p_source, SourceVoiceID p_voice) const;
	virtual void source_stop_voice(RID p_source, SourceVoiceID p_voice);

	/* LISTENER */

	virtual RID listener_create();
	virtual void listener_set_space(RID p_listener, RID p_space);

	virtual void listener_set_transform(RID p_listener, const Matrix32 &p_transform);
	virtual Matrix32 listener_get_transform(RID p_listener) const;

	virtual void listener_set_param(RID p_listener, ListenerParam p_param, float p_value);
	virtual float listener_get_param(RID p_listener, ListenerParam p_param) const;

	/* MISC */

	virtual void free(RID p_id);

	virtual void init();
	virtual void update(float p_delta);
	virtual void finish();

	SpatialSound2DServerSW();
};

#endif // SPATIAL_SOUND_2D_SERVER_SW_H
