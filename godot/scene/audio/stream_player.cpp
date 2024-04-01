/*************************************************************************/
/*  stream_player.cpp                                                    */
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
#include "stream_player.h"

int StreamPlayer::InternalStream::get_channel_count() const {

	return player->sp_get_channel_count();
}
void StreamPlayer::InternalStream::set_mix_rate(int p_rate) {

	return player->sp_set_mix_rate(p_rate);
}
bool StreamPlayer::InternalStream::mix(int32_t *p_buffer, int p_frames) {

	return player->sp_mix(p_buffer, p_frames);
}
void StreamPlayer::InternalStream::update() {

	player->sp_update();
}

int StreamPlayer::sp_get_channel_count() const {

	return playback->get_channels();
}

void StreamPlayer::sp_set_mix_rate(int p_rate) {

	server_mix_rate = p_rate;
}

bool StreamPlayer::sp_mix(int32_t *p_buffer, int p_frames) {

	if (resampler.is_ready() && !paused) {
		return resampler.mix(p_buffer, p_frames);
	}

	return false;
}

void StreamPlayer::sp_update() {

	//_THREAD_SAFE_METHOD_
	if (!paused && resampler.is_ready() && playback.is_valid()) {

		if (!playback->is_playing()) {
			//stream depleted data, but there's still audio in the ringbuffer
			//check that all this audio has been flushed before stopping the stream
			int to_mix = resampler.get_total() - resampler.get_todo();
			if (to_mix == 0) {
				if (!stop_request) {
					stop_request = true;
					call_deferred("_do_stop");
				}
				return;
			}

			return;
		}

		int todo = resampler.get_todo();
		int wrote = playback->mix(resampler.get_write_buffer(), todo);
		resampler.write(wrote);
	}
}

void StreamPlayer::_do_stop() {
	stop();
	emit_signal("finished");
}

void StreamPlayer::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE: {

			//set_idle_process(false); //don't annoy
			if (stream.is_valid() && !get_tree()->is_editor_hint()) {
				if (resume_pos >= 0) {
					play(resume_pos);
					resume_pos = -1;
				} else if (autoplay) {
					play();
					autoplay = false; //this line fix autoplay issues
				}
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {

			if (is_playing()) {
				resume_pos = get_pos();
			}
			stop(); //wathever it may be doing, stop
		} break;
	}
}

void StreamPlayer::set_stream(const Ref<AudioStream> &p_stream) {

	stop();

	stream = p_stream;

	if (!stream.is_null()) {
		playback = stream->instance_playback();
		playback->set_loop(loops);
		playback->set_loop_restart_time(loop_point);
		AudioServer::get_singleton()->lock();
		resampler.setup(playback->get_channels(), playback->get_mix_rate(), server_mix_rate, buffering_ms, playback->get_minimum_buffer_size());
		AudioServer::get_singleton()->unlock();
	} else {
		AudioServer::get_singleton()->lock();
		resampler.clear();
		playback.unref();
		AudioServer::get_singleton()->unlock();
	}
}

Ref<AudioStream> StreamPlayer::get_stream() const {

	return stream;
}

void StreamPlayer::play(float p_from_offset) {

	ERR_FAIL_COND(!is_inside_tree());
	if (playback.is_null())
		return;
	//if (is_playing())
	stop();

	//_THREAD_SAFE_METHOD_
	playback->play(p_from_offset);
	//feed the ringbuffer as long as no update callback is going on
	sp_update();
	AudioServer::get_singleton()->stream_set_active(stream_rid, true);
	AudioServer::get_singleton()->stream_set_volume_scale(stream_rid, volume);
	//	if (stream->get_update_mode()!=AudioStream::UPDATE_NONE)
	//		set_idle_process(true);
}

void StreamPlayer::stop() {

	if (!is_inside_tree())
		return;
	if (playback.is_null())
		return;

	//_THREAD_SAFE_METHOD_
	AudioServer::get_singleton()->stream_set_active(stream_rid, false);
	stop_request = false;
	playback->stop();
	resampler.flush();

	//set_idle_process(false);
}

bool StreamPlayer::is_playing() const {

	if (playback.is_null())
		return false;

	return playback->is_playing() || resampler.has_data();
}

void StreamPlayer::set_loop(bool p_enable) {

	loops = p_enable;
	if (playback.is_null())
		return;
	playback->set_loop(loops);
}
bool StreamPlayer::has_loop() const {

	return loops;
}

void StreamPlayer::set_volume(float p_vol) {

	volume = p_vol;
	if (stream_rid.is_valid())
		AudioServer::get_singleton()->stream_set_volume_scale(stream_rid, volume);
}

float StreamPlayer::get_volume() const {

	return volume;
}

void StreamPlayer::set_loop_restart_time(float p_secs) {

	loop_point = p_secs;
	if (playback.is_valid())
		playback->set_loop_restart_time(p_secs);
}

float StreamPlayer::get_loop_restart_time() const {

	return loop_point;
}

void StreamPlayer::set_volume_db(float p_db) {

	if (p_db < -79)
		set_volume(0);
	else
		set_volume(Math::db2linear(p_db));
}

float StreamPlayer::get_volume_db() const {

	if (volume == 0)
		return -80;
	else
		return Math::linear2db(volume);
}

String StreamPlayer::get_stream_name() const {

	if (stream.is_null())
		return "<No Stream>";
	return stream->get_name();
}

int StreamPlayer::get_loop_count() const {

	if (playback.is_null())
		return 0;
	return playback->get_loop_count();
}

float StreamPlayer::get_pos() const {

	if (playback.is_null())
		return 0;
	return playback->get_pos();
}

float StreamPlayer::get_length() const {

	if (playback.is_null())
		return 0;
	return playback->get_length();
}
void StreamPlayer::seek_pos(float p_time) {

	if (playback.is_null())
		return;
	//works better...
	stop();
	playback->play(p_time);
}

void StreamPlayer::set_autoplay(bool p_enable) {

	autoplay = p_enable;
}

bool StreamPlayer::has_autoplay() const {

	return autoplay;
}

void StreamPlayer::set_paused(bool p_paused) {

	paused = p_paused;
	//if (stream.is_valid())
	//	stream->set_paused(p_paused);
}

bool StreamPlayer::is_paused() const {

	return paused;
}

void StreamPlayer::_set_play(bool p_play) {

	_play = p_play;
	if (is_inside_tree()) {
		if (_play)
			play();
		else
			stop();
	}
}

bool StreamPlayer::_get_play() const {

	return _play;
}

void StreamPlayer::set_buffering_msec(int p_msec) {

	buffering_ms = p_msec;
}

int StreamPlayer::get_buffering_msec() const {

	return buffering_ms;
}

void StreamPlayer::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_stream", "stream:AudioStream"), &StreamPlayer::set_stream);
	ObjectTypeDB::bind_method(_MD("get_stream:AudioStream"), &StreamPlayer::get_stream);

	ObjectTypeDB::bind_method(_MD("play", "offset"), &StreamPlayer::play, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("stop"), &StreamPlayer::stop);

	ObjectTypeDB::bind_method(_MD("is_playing"), &StreamPlayer::is_playing);

	ObjectTypeDB::bind_method(_MD("set_paused", "paused"), &StreamPlayer::set_paused);
	ObjectTypeDB::bind_method(_MD("is_paused"), &StreamPlayer::is_paused);

	ObjectTypeDB::bind_method(_MD("set_loop", "enabled"), &StreamPlayer::set_loop);
	ObjectTypeDB::bind_method(_MD("has_loop"), &StreamPlayer::has_loop);

	ObjectTypeDB::bind_method(_MD("set_volume", "volume"), &StreamPlayer::set_volume);
	ObjectTypeDB::bind_method(_MD("get_volume"), &StreamPlayer::get_volume);

	ObjectTypeDB::bind_method(_MD("set_volume_db", "db"), &StreamPlayer::set_volume_db);
	ObjectTypeDB::bind_method(_MD("get_volume_db"), &StreamPlayer::get_volume_db);

	ObjectTypeDB::bind_method(_MD("set_buffering_msec", "msec"), &StreamPlayer::set_buffering_msec);
	ObjectTypeDB::bind_method(_MD("get_buffering_msec"), &StreamPlayer::get_buffering_msec);

	ObjectTypeDB::bind_method(_MD("set_loop_restart_time", "secs"), &StreamPlayer::set_loop_restart_time);
	ObjectTypeDB::bind_method(_MD("get_loop_restart_time"), &StreamPlayer::get_loop_restart_time);

	ObjectTypeDB::bind_method(_MD("get_stream_name"), &StreamPlayer::get_stream_name);
	ObjectTypeDB::bind_method(_MD("get_loop_count"), &StreamPlayer::get_loop_count);

	ObjectTypeDB::bind_method(_MD("get_pos"), &StreamPlayer::get_pos);
	ObjectTypeDB::bind_method(_MD("seek_pos", "time"), &StreamPlayer::seek_pos);

	ObjectTypeDB::bind_method(_MD("set_autoplay", "enabled"), &StreamPlayer::set_autoplay);
	ObjectTypeDB::bind_method(_MD("has_autoplay"), &StreamPlayer::has_autoplay);

	ObjectTypeDB::bind_method(_MD("get_length"), &StreamPlayer::get_length);

	ObjectTypeDB::bind_method(_MD("_set_play", "play"), &StreamPlayer::_set_play);
	ObjectTypeDB::bind_method(_MD("_get_play"), &StreamPlayer::_get_play);
	ObjectTypeDB::bind_method(_MD("_do_stop"), &StreamPlayer::_do_stop);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream/stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), _SCS("set_stream"), _SCS("get_stream"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream/play"), _SCS("_set_play"), _SCS("_get_play"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream/loop"), _SCS("set_loop"), _SCS("has_loop"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "stream/volume_db", PROPERTY_HINT_RANGE, "-80,24,0.01"), _SCS("set_volume_db"), _SCS("get_volume_db"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream/autoplay"), _SCS("set_autoplay"), _SCS("has_autoplay"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream/paused"), _SCS("set_paused"), _SCS("is_paused"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "stream/loop_restart_time"), _SCS("set_loop_restart_time"), _SCS("get_loop_restart_time"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream/buffering_ms"), _SCS("set_buffering_msec"), _SCS("get_buffering_msec"));

	ADD_SIGNAL(MethodInfo("finished"));
}

StreamPlayer::StreamPlayer() {

	volume = 1;
	loops = false;
	paused = false;
	autoplay = false;
	_play = false;
	server_mix_rate = 1;
	internal_stream.player = this;
	stream_rid = AudioServer::get_singleton()->audio_stream_create(&internal_stream);
	buffering_ms = 500;
	loop_point = 0;
	stop_request = false;
	resume_pos = -1;
}

StreamPlayer::~StreamPlayer() {
	AudioServer::get_singleton()->free(stream_rid);
	resampler.clear();
}
