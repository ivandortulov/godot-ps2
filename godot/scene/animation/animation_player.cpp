/*************************************************************************/
/*  animation_player.cpp                                                 */
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
#include "animation_player.h"

#include "message_queue.h"
#include "scene/scene_string_names.h"

bool AnimationPlayer::_set(const StringName &p_name, const Variant &p_value) {

	String name = p_name;

	if (p_name == SceneStringNames::get_singleton()->playback_speed || p_name == SceneStringNames::get_singleton()->speed) { //bw compatibility
		set_speed(p_value);

	} else if (p_name == SceneStringNames::get_singleton()->playback_active) {
		set_active(p_value);
	} else if (name.begins_with("playback/play")) {

		String which = p_value;

		if (which == "[stop]")
			stop();
		else
			play(which);
	} else if (name.begins_with("anims/")) {

		String which = name.get_slicec('/', 1);

		add_animation(which, p_value);
	} else if (name.begins_with("next/")) {

		String which = name.get_slicec('/', 1);
		animation_set_next(which, p_value);

	} else if (p_name == SceneStringNames::get_singleton()->blend_times) {

		Array array = p_value;
		int len = array.size();
		ERR_FAIL_COND_V(len % 3, false);

		for (int i = 0; i < len / 3; i++) {

			StringName from = array[i * 3 + 0];
			StringName to = array[i * 3 + 1];
			float time = array[i * 3 + 2];

			set_blend_time(from, to, time);
		}

	} else if (p_name == SceneStringNames::get_singleton()->autoplay) {
		autoplay = p_value;

	} else
		return false;

	return true;
}

bool AnimationPlayer::_get(const StringName &p_name, Variant &r_ret) const {

	String name = p_name;

	if (name == "playback/speed") { //bw compatibility

		r_ret = speed_scale;
	} else if (name == "playback/active") {

		r_ret = is_active();
	} else if (name == "playback/play") {

		if (is_active() && is_playing())
			r_ret = playback.assigned;
		else
			r_ret = "[stop]";

	} else if (name.begins_with("anims/")) {

		String which = name.get_slicec('/', 1);

		r_ret = get_animation(which).get_ref_ptr();
	} else if (name.begins_with("next/")) {

		String which = name.get_slicec('/', 1);

		r_ret = animation_get_next(which);

	} else if (name == "blend_times") {

		Vector<BlendKey> keys;
		for (Map<BlendKey, float>::Element *E = blend_times.front(); E; E = E->next()) {

			keys.ordered_insert(E->key());
		}

		Array array;
		for (int i = 0; i < keys.size(); i++) {

			array.push_back(keys[i].from);
			array.push_back(keys[i].to);
			array.push_back(blend_times[keys[i]]);
		}

		r_ret = array;
	} else if (name == "autoplay") {
		r_ret = autoplay;

	} else
		return false;

	return true;
}

void AnimationPlayer::_get_property_list(List<PropertyInfo> *p_list) const {

	List<String> names;

	List<PropertyInfo> anim_names;

	for (Map<StringName, AnimationData>::Element *E = animation_set.front(); E; E = E->next()) {

		anim_names.push_back(PropertyInfo(Variant::OBJECT, "anims/" + String(E->key()), PROPERTY_HINT_RESOURCE_TYPE, "Animation", PROPERTY_USAGE_NOEDITOR));
		if (E->get().next != StringName())
			anim_names.push_back(PropertyInfo(Variant::STRING, "next/" + String(E->key()), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		names.push_back(E->key());
	}

	anim_names.sort();

	for (List<PropertyInfo>::Element *E = anim_names.front(); E; E = E->next()) {
		p_list->push_back(E->get());
	}

	{
		names.sort();
		names.push_front("[stop]");
		String hint;
		for (List<String>::Element *E = names.front(); E; E = E->next()) {

			if (E != names.front())
				hint += ",";
			hint += E->get();
		}

		p_list->push_back(PropertyInfo(Variant::STRING, "playback/play", PROPERTY_HINT_ENUM, hint, PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ANIMATE_AS_TRIGGER));
		p_list->push_back(PropertyInfo(Variant::BOOL, "playback/active", PROPERTY_HINT_NONE, ""));
		p_list->push_back(PropertyInfo(Variant::REAL, "playback/speed", PROPERTY_HINT_RANGE, "-64,64,0.01"));
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "blend_times", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	p_list->push_back(PropertyInfo(Variant::STRING, "autoplay", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
}

void AnimationPlayer::advance(float p_time) {

	_animation_process(p_time);
}

void AnimationPlayer::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE: {

			if (!processing) {
				//make sure that a previous process state was not saved
				//only process if "processing" is set
				set_fixed_process(false);
				set_process(false);
			}
			//_set_process(false);
			clear_caches();
		} break;
		case NOTIFICATION_READY: {

			if (!get_tree()->is_editor_hint() && animation_set.has(autoplay)) {
				play(autoplay);
				set_autoplay(""); //this line is the fix for autoplay issues with animatio
			}
		} break;
		case NOTIFICATION_PROCESS: {
			if (animation_process_mode == ANIMATION_PROCESS_FIXED)
				break;

			if (processing)
				_animation_process(get_process_delta_time());
		} break;
		case NOTIFICATION_FIXED_PROCESS: {

			if (animation_process_mode == ANIMATION_PROCESS_IDLE)
				break;

			if (processing)
				_animation_process(get_fixed_process_delta_time());
		} break;
		case NOTIFICATION_EXIT_TREE: {

			//stop_all();
			clear_caches();
		} break;
	}
}

void AnimationPlayer::_generate_node_caches(AnimationData *p_anim) {

	Node *parent = get_node(root);

	ERR_FAIL_COND(!parent);

	Animation *a = p_anim->animation.operator->();

	p_anim->node_cache.resize(a->get_track_count());

	for (int i = 0; i < a->get_track_count(); i++) {

		p_anim->node_cache[i] = NULL;
		RES resource;
		Node *child = parent->get_node_and_resource(a->track_get_path(i), resource);
		if (!child) {
			ERR_EXPLAIN("On Animation: '" + p_anim->name + "', couldn't resolve track:  '" + String(a->track_get_path(i)) + "'");
		}
		ERR_CONTINUE(!child); // couldn't find the child node
		uint32_t id = resource.is_valid() ? resource->get_instance_ID() : child->get_instance_ID();
		int bone_idx = -1;

		if (a->track_get_path(i).get_property() && child->cast_to<Skeleton>()) {

			bone_idx = child->cast_to<Skeleton>()->find_bone(a->track_get_path(i).get_property());
			if (bone_idx == -1) {

				continue;
			}
		}

		{
			if (!child->is_connected("exit_tree", this, "_node_removed"))
				child->connect("exit_tree", this, "_node_removed", make_binds(child), CONNECT_ONESHOT);
		}

		TrackNodeCacheKey key;
		key.id = id;
		key.bone_idx = bone_idx;

		if (node_cache_map.has(key)) {

			p_anim->node_cache[i] = &node_cache_map[key];
		} else {

			node_cache_map[key] = TrackNodeCache();

			p_anim->node_cache[i] = &node_cache_map[key];
			p_anim->node_cache[i]->path = a->track_get_path(i);
			p_anim->node_cache[i]->node = child;
			p_anim->node_cache[i]->resource = resource;
			p_anim->node_cache[i]->node_2d = child->cast_to<Node2D>();
			if (a->track_get_type(i) == Animation::TYPE_TRANSFORM) {
				// special cases and caches for transform tracks

				// cache spatial
				p_anim->node_cache[i]->spatial = child->cast_to<Spatial>();
				// cache skeleton
				p_anim->node_cache[i]->skeleton = child->cast_to<Skeleton>();
				if (p_anim->node_cache[i]->skeleton) {

					StringName bone_name = a->track_get_path(i).get_property();
					if (bone_name.operator String() != "") {

						p_anim->node_cache[i]->bone_idx = p_anim->node_cache[i]->skeleton->find_bone(bone_name);
						if (p_anim->node_cache[i]->bone_idx < 0) {
							// broken track (nonexistent bone)
							p_anim->node_cache[i]->skeleton = NULL;
							p_anim->node_cache[i]->spatial = NULL;
							printf("bone is %ls\n", String(bone_name).c_str());
							ERR_CONTINUE(p_anim->node_cache[i]->bone_idx < 0);
						} else {
						}
					} else {
						// no property, just use spatialnode
						p_anim->node_cache[i]->skeleton = NULL;
					}
				}
			}
		}

		if (a->track_get_type(i) == Animation::TYPE_VALUE) {

			StringName property = a->track_get_path(i).get_property();
			if (!p_anim->node_cache[i]->property_anim.has(property)) {

				TrackNodeCache::PropertyAnim pa;
				pa.prop = property;
				pa.object = resource.is_valid() ? (Object *)resource.ptr() : (Object *)child;
				pa.special = SP_NONE;
				pa.owner = p_anim->node_cache[i];
				if (false && p_anim->node_cache[i]->node_2d) {

					if (pa.prop == SceneStringNames::get_singleton()->transform_pos)
						pa.special = SP_NODE2D_POS;
					else if (pa.prop == SceneStringNames::get_singleton()->transform_rot)
						pa.special = SP_NODE2D_ROT;
					else if (pa.prop == SceneStringNames::get_singleton()->transform_scale)
						pa.special = SP_NODE2D_SCALE;
				}
				p_anim->node_cache[i]->property_anim[property] = pa;
			}
		}
	}
}

void AnimationPlayer::_animation_process_animation(AnimationData *p_anim, float p_time, float p_delta, float p_interp, bool p_allow_discrete) {

	if (p_anim->node_cache.size() != p_anim->animation->get_track_count()) {
		// animation hasn't been "node-cached"
		_generate_node_caches(p_anim);
	}

	ERR_FAIL_COND(p_anim->node_cache.size() != p_anim->animation->get_track_count());

	Animation *a = p_anim->animation.operator->();
	bool can_call = is_inside_tree() && !get_tree()->is_editor_hint();

	for (int i = 0; i < a->get_track_count(); i++) {

		TrackNodeCache *nc = p_anim->node_cache[i];

		if (!nc) // no node cache for this track, skip it
			continue;

		if (a->track_get_key_count(i) == 0)
			continue; // do nothing if track is empty

		switch (a->track_get_type(i)) {

			case Animation::TYPE_TRANSFORM: {

				if (!nc->spatial)
					continue;

				Vector3 loc;
				Quat rot;
				Vector3 scale;

				Error err = a->transform_track_interpolate(i, p_time, &loc, &rot, &scale);
				//ERR_CONTINUE(err!=OK); //used for testing, should be removed

				if (err != OK)
					continue;

				if (nc->accum_pass != accum_pass) {
					ERR_CONTINUE(cache_update_size >= NODE_CACHE_UPDATE_MAX);
					cache_update[cache_update_size++] = nc;
					nc->accum_pass = accum_pass;
					nc->loc_accum = loc;
					nc->rot_accum = rot;
					nc->scale_accum = scale;

				} else {

					nc->loc_accum = nc->loc_accum.linear_interpolate(loc, p_interp);
					nc->rot_accum = nc->rot_accum.slerp(rot, p_interp);
					nc->scale_accum = nc->scale_accum.linear_interpolate(scale, p_interp);
				}

			} break;
			case Animation::TYPE_VALUE: {

				if (!nc->node)
					continue;

				//StringName property=a->track_get_path(i).get_property();

				Map<StringName, TrackNodeCache::PropertyAnim>::Element *E = nc->property_anim.find(a->track_get_path(i).get_property());
				ERR_CONTINUE(!E); //should it continue, or create a new one?

				TrackNodeCache::PropertyAnim *pa = &E->get();

				if (a->value_track_get_update_mode(i) == Animation::UPDATE_CONTINUOUS || (p_delta == 0 && a->value_track_get_update_mode(i) == Animation::UPDATE_DISCRETE)) { //delta == 0 means seek

					Variant value = a->value_track_interpolate(i, p_time);
					//thanks to trigger mode, this should be solved now..
					//if (p_delta==0 && value.get_type()==Variant::STRING)
					//	continue; // doing this with strings is messy, should find another way
					if (pa->accum_pass != accum_pass) {
						ERR_CONTINUE(cache_update_prop_size >= NODE_CACHE_UPDATE_MAX);
						cache_update_prop[cache_update_prop_size++] = pa;
						pa->value_accum = value;
						pa->accum_pass = accum_pass;
					} else {
						Variant::interpolate(pa->value_accum, value, p_interp, pa->value_accum);
					}

				} else if (p_allow_discrete && p_delta != 0) {

					List<int> indices;
					a->value_track_get_key_indices(i, p_time, p_delta, &indices);

					for (List<int>::Element *F = indices.front(); F; F = F->next()) {

						Variant value = a->track_get_key_value(i, F->get());
						switch (pa->special) {

							case SP_NONE: {
								bool valid;
								pa->object->set(pa->prop, value, &valid); //you are not speshul
#ifdef DEBUG_ENABLED
								if (!valid) {
									ERR_PRINTS("Failed setting track value '" + String(pa->owner->path) + "'. Check if property exists or the type of key is valid");
								}
#endif

							} break;
							case SP_NODE2D_POS: {
#ifdef DEBUG_ENABLED
								if (value.get_type() != Variant::VECTOR2) {
									ERR_PRINTS("Position key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not of type Vector2()");
								}
#endif
								static_cast<Node2D *>(pa->object)->set_pos(value);
							} break;
							case SP_NODE2D_ROT: {
#ifdef DEBUG_ENABLED
								if (value.is_num()) {
									ERR_PRINTS("Rotation key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not numerical");
								}
#endif

								static_cast<Node2D *>(pa->object)->set_rot(Math::deg2rad(value));
							} break;
							case SP_NODE2D_SCALE: {
#ifdef DEBUG_ENABLED
								if (value.get_type() != Variant::VECTOR2) {
									ERR_PRINTS("Scale key at time " + rtos(p_time) + " in Animation Track '" + String(pa->owner->path) + "' not of type Vector2()");
								}
#endif

								static_cast<Node2D *>(pa->object)->set_scale(value);
							} break;
						}
					}
				}

			} break;
			case Animation::TYPE_METHOD: {

				if (!nc->node)
					continue;
				if (p_delta == 0)
					continue;
				if (!p_allow_discrete)
					break;

				List<int> indices;

				a->method_track_get_key_indices(i, p_time, p_delta, &indices);

				for (List<int>::Element *E = indices.front(); E; E = E->next()) {

					StringName method = a->method_track_get_name(i, E->get());
					Vector<Variant> params = a->method_track_get_params(i, E->get());

					int s = params.size();

					ERR_CONTINUE(s > VARIANT_ARG_MAX);
					if (can_call) {
						MessageQueue::get_singleton()->push_call(
								nc->node,
								method,
								s >= 1 ? params[0] : Variant(),
								s >= 2 ? params[1] : Variant(),
								s >= 3 ? params[2] : Variant(),
								s >= 4 ? params[3] : Variant(),
								s >= 5 ? params[4] : Variant());
					}
				}

			} break;
		}
	}
}

void AnimationPlayer::_animation_process_data(PlaybackData &cd, float p_delta, float p_blend) {

	float delta = p_delta * speed_scale * cd.speed_scale;
	bool backwards = delta < 0;
	float next_pos = cd.pos + delta;

	float len = cd.from->animation->get_length();
	bool loop = cd.from->animation->has_loop();

	if (!loop) {

		if (next_pos < 0)
			next_pos = 0;
		else if (next_pos > len)
			next_pos = len;

		// fix delta
		delta = next_pos - cd.pos;

		if (&cd == &playback.current) {

			if (!backwards && cd.pos <= len && next_pos == len /*&& playback.blend.empty()*/) {
				//playback finished
				end_reached = true;
				end_notify = cd.pos < len; // Notify only if not already at the end
			}

			if (backwards && cd.pos >= 0 && next_pos == 0 /*&& playback.blend.empty()*/) {
				//playback finished
				end_reached = true;
				end_notify = cd.pos > 0; // Notify only if not already at the beginning
			}
		}

	} else {

		next_pos = Math::fposmod(next_pos, len);
	}

	cd.pos = next_pos;

	_animation_process_animation(cd.from, cd.pos, delta, p_blend, &cd == &playback.current);
}
void AnimationPlayer::_animation_process2(float p_delta) {

	Playback &c = playback;

	float prev_blend = 1.0;
	accum_pass++;

	int pop_count = 1;
	int pop = 0; // if >0, then amount of elements to pop from the back

	for (List<Blend>::Element *E = c.blend.back(); E; E = E->prev(), pop_count++) {

		Blend &b = E->get();
		_animation_process_data(b.data, p_delta, prev_blend);

		prev_blend = 1.0 - b.blend_left / b.blend_time;

		b.blend_left -= Math::absf(speed_scale * p_delta);

		if (b.blend_left < 0) {

			pop = pop_count;
		}
	}

	while (pop--) {

		c.blend.pop_back();
	}

	_animation_process_data(c.current, p_delta, prev_blend);
}

void AnimationPlayer::_animation_update_transforms() {

	for (int i = 0; i < cache_update_size; i++) {

		TrackNodeCache *nc = cache_update[i];

		ERR_CONTINUE(nc->accum_pass != accum_pass);

		if (nc->spatial) {

			Transform t;
			t.origin = nc->loc_accum;
			t.basis = nc->rot_accum;
			t.basis.scale(nc->scale_accum);

			if (nc->skeleton && nc->bone_idx >= 0) {

				nc->skeleton->set_bone_pose(nc->bone_idx, t);

			} else if (nc->spatial) {

				nc->spatial->set_transform(t);
			}
		}
	}

	cache_update_size = 0;

	for (int i = 0; i < cache_update_prop_size; i++) {

		TrackNodeCache::PropertyAnim *pa = cache_update_prop[i];

		ERR_CONTINUE(pa->accum_pass != accum_pass);

#if 1
		/*		switch(pa->special) {


			case SP_NONE: pa->object->set(pa->prop,pa->value_accum); break; //you are not speshul
			case SP_NODE2D_POS: static_cast<Node2D*>(pa->object)->set_pos(pa->value_accum); break;
			case SP_NODE2D_ROT: static_cast<Node2D*>(pa->object)->set_rot(Math::deg2rad(pa->value_accum)); break;
			case SP_NODE2D_SCALE: static_cast<Node2D*>(pa->object)->set_scale(pa->value_accum); break;
		}*/

		switch (pa->special) {

			case SP_NONE: {
				bool valid;
				pa->object->set(pa->prop, pa->value_accum, &valid); //you are not speshul
#ifdef DEBUG_ENABLED
				if (!valid) {
					ERR_PRINTS("Failed setting key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "', Track '" + String(pa->owner->path) + "'. Check if property exists or the type of key is right for the property");
				}
#endif

			} break;
			case SP_NODE2D_POS: {
#ifdef DEBUG_ENABLED
				if (pa->value_accum.get_type() != Variant::VECTOR2) {
					ERR_PRINTS("Position key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "', Track '" + String(pa->owner->path) + "' not of type Vector2()");
				}
#endif
				static_cast<Node2D *>(pa->object)->set_pos(pa->value_accum);
			} break;
			case SP_NODE2D_ROT: {
#ifdef DEBUG_ENABLED
				if (pa->value_accum.is_num()) {
					ERR_PRINTS("Rotation key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "', Track '" + String(pa->owner->path) + "' not numerical");
				}
#endif

				static_cast<Node2D *>(pa->object)->set_rot(Math::deg2rad(pa->value_accum));
			} break;
			case SP_NODE2D_SCALE: {
#ifdef DEBUG_ENABLED
				if (pa->value_accum.get_type() != Variant::VECTOR2) {
					ERR_PRINTS("Scale key at time " + rtos(playback.current.pos) + " in Animation '" + get_current_animation() + "', Track '" + String(pa->owner->path) + "' not of type Vector2()");
				}
#endif

				static_cast<Node2D *>(pa->object)->set_scale(pa->value_accum);
			} break;
		}
#else

		pa->object->set(pa->prop, pa->value_accum);
#endif
	}

	cache_update_prop_size = 0;
}

void AnimationPlayer::_animation_process(float p_delta) {

	//	bool any_active=false;

	if (playback.current.from) {

		end_reached = false;
		end_notify = false;
		_animation_process2(p_delta);
		_animation_update_transforms();
		if (end_reached) {
			if (queued.size()) {
				String old = playback.assigned;
				play(queued.front()->get());
				String new_name = playback.assigned;
				queued.pop_front();
				if (end_notify)
					emit_signal(SceneStringNames::get_singleton()->animation_changed, old, new_name);
			} else {
				//stop();
				playing = false;
				_set_process(false);
				if (end_notify)
					emit_signal(SceneStringNames::get_singleton()->finished);
			}
			end_reached = false;
		}

	} else {
		_set_process(false);
	}
}

Error AnimationPlayer::add_animation(const StringName &p_name, const Ref<Animation> &p_animation) {

#ifdef DEBUG_ENABLED
	ERR_EXPLAIN("Invalid animation name: " + String(p_name));
	ERR_FAIL_COND_V(String(p_name).find("/") != -1 || String(p_name).find(":") != -1 || String(p_name).find(",") != -1 || String(p_name).find("[") != -1, ERR_INVALID_PARAMETER);
#endif

	ERR_FAIL_COND_V(p_animation.is_null(), ERR_INVALID_PARAMETER);

	//print_line("Add anim: "+String(p_name)+" name: "+p_animation->get_name());

	if (animation_set.has(p_name)) {

		_unref_anim(animation_set[p_name].animation);
		animation_set[p_name].animation = p_animation;
		clear_caches();
	} else {

		AnimationData ad;
		ad.animation = p_animation;
		ad.name = p_name;
		animation_set[p_name] = ad;
	}

	_ref_anim(p_animation);
	_change_notify();
	return OK;
}

void AnimationPlayer::remove_animation(const StringName &p_name) {

	ERR_FAIL_COND(!animation_set.has(p_name));

	stop_all();
	_unref_anim(animation_set[p_name].animation);
	animation_set.erase(p_name);

	clear_caches();
	_change_notify();
}

void AnimationPlayer::_ref_anim(const Ref<Animation> &p_anim) {

	if (used_anims.has(p_anim))
		used_anims[p_anim]++;
	else {
		used_anims[p_anim] = 1;
		Ref<Animation>(p_anim)->connect("changed", this, "_animation_changed");
	}
}

void AnimationPlayer::_unref_anim(const Ref<Animation> &p_anim) {

	ERR_FAIL_COND(!used_anims.has(p_anim));

	int &n = used_anims[p_anim];
	n--;
	if (n == 0) {

		Ref<Animation>(p_anim)->disconnect("changed", this, "_animation_changed");
		used_anims.erase(p_anim);
	}
}

void AnimationPlayer::rename_animation(const StringName &p_name, const StringName &p_new_name) {

	ERR_FAIL_COND(!animation_set.has(p_name));
	ERR_FAIL_COND(String(p_new_name).find("/") != -1 || String(p_new_name).find(":") != -1);
	ERR_FAIL_COND(animation_set.has(p_new_name));

	//print_line("Rename anim: "+String(p_name)+" name: "+String(p_new_name));

	stop_all();
	AnimationData ad = animation_set[p_name];
	ad.name = p_new_name;
	animation_set.erase(p_name);
	animation_set[p_new_name] = ad;

	List<BlendKey> to_erase;
	Map<BlendKey, float> to_insert;
	for (Map<BlendKey, float>::Element *E = blend_times.front(); E; E = E->next()) {

		BlendKey bk = E->key();
		BlendKey new_bk = bk;
		bool erase = false;
		if (bk.from == p_name) {
			new_bk.from = p_new_name;
			erase = true;
		}
		if (bk.to == p_name) {
			new_bk.to = p_new_name;
			erase = true;
		}

		if (erase) {
			to_erase.push_back(bk);
			to_insert[new_bk] = E->get();
		}
	}

	while (to_erase.size()) {

		blend_times.erase(to_erase.front()->get());
		to_erase.pop_front();
	}

	while (to_insert.size()) {
		blend_times[to_insert.front()->key()] = to_insert.front()->get();
		to_insert.erase(to_insert.front());
	}

	if (autoplay == p_name)
		autoplay = p_new_name;

	clear_caches();
	_change_notify();
}

bool AnimationPlayer::has_animation(const StringName &p_name) const {

	return animation_set.has(p_name);
}
Ref<Animation> AnimationPlayer::get_animation(const StringName &p_name) const {

	ERR_FAIL_COND_V(!animation_set.has(p_name), Ref<Animation>());

	const AnimationData &data = animation_set[p_name];

	return data.animation;
}
void AnimationPlayer::get_animation_list(List<StringName> *p_animations) const {

	List<String> anims;

	for (Map<StringName, AnimationData>::Element *E = animation_set.front(); E; E = E->next()) {

		anims.push_back(E->key());
	}

	anims.sort();

	for (List<String>::Element *E = anims.front(); E; E = E->next()) {

		p_animations->push_back(E->get());
	}
}

void AnimationPlayer::set_blend_time(const StringName &p_animation1, const StringName &p_animation2, float p_time) {

	ERR_FAIL_COND(p_time < 0);

	BlendKey bk;
	bk.from = p_animation1;
	bk.to = p_animation2;
	if (p_time == 0)
		blend_times.erase(bk);
	else
		blend_times[bk] = p_time;
}

float AnimationPlayer::get_blend_time(const StringName &p_animation1, const StringName &p_animation2) const {

	BlendKey bk;
	bk.from = p_animation1;
	bk.to = p_animation2;

	if (blend_times.has(bk))
		return blend_times[bk];
	else
		return 0;
}

void AnimationPlayer::queue(const StringName &p_name) {

	if (!is_playing())
		play(p_name);
	else
		queued.push_back(p_name);
}

void AnimationPlayer::clear_queue() {
	queued.clear();
};

void AnimationPlayer::play_backwards(const StringName &p_name, float p_custom_blend) {

	play(p_name, p_custom_blend, -1, true);
}

void AnimationPlayer::play(const StringName &p_name, float p_custom_blend, float p_custom_scale, bool p_from_end) {

	//printf("animation is %ls\n", String(p_name).c_str());
	//ERR_FAIL_COND(!is_inside_scene());
	StringName name = p_name;

	if (String(name) == "")
		name = playback.assigned;

	if (!animation_set.has(name)) {
		ERR_EXPLAIN("Animation not found: " + name);
		ERR_FAIL();
	}

	Playback &c = playback;

	if (c.current.from) {

		float blend_time = 0;
		// find if it can blend
		BlendKey bk;
		bk.from = c.current.from->name;
		bk.to = name;

		if (p_custom_blend >= 0) {
			blend_time = p_custom_blend;
		} else if (blend_times.has(bk)) {

			blend_time = blend_times[bk];
		} else {

			bk.from = "*";
			if (blend_times.has(bk)) {

				blend_time = blend_times[bk];
			} else {

				bk.from = c.current.from->name;
				bk.to = "*";

				if (blend_times.has(bk)) {

					blend_time = blend_times[bk];
				}
			}
		}

		if (p_custom_blend < 0 && blend_time == 0 && default_blend_time)
			blend_time = default_blend_time;
		if (blend_time > 0) {

			Blend b;
			b.data = c.current;
			b.blend_time = b.blend_left = blend_time;
			c.blend.push_back(b);
		}
	}

	c.current.from = &animation_set[name];
	c.current.pos = p_from_end ? c.current.from->animation->get_length() : 0;
	c.current.speed_scale = p_custom_scale;
	c.assigned = p_name;

	if (!end_reached)
		queued.clear();
	_set_process(true); // always process when starting an animation
	playing = true;

	emit_signal(SceneStringNames::get_singleton()->animation_started, c.assigned);

	if (is_inside_tree() && get_tree()->is_editor_hint())
		return; // no next in this case

	StringName next = animation_get_next(p_name);
	if (next != StringName() && animation_set.has(next)) {
		queue(next);
	}
}

bool AnimationPlayer::is_playing() const {

	return playing;
	/*
	if (playback.current.from==NULL)
		return false;

	float len=playback.current.from->animation->get_length();
	float pos = playback.current.pos;
	bool loop=playback.current.from->animation->has_loop();
	if (!loop && pos >= len) {
		return false;
	};

	return true;
    */
}
void AnimationPlayer::set_current_animation(const String &p_anim) {

	if (is_playing()) {
		play(p_anim);
	} else {
		ERR_FAIL_COND(!animation_set.has(p_anim));
		playback.current.pos = 0;
		playback.current.from = &animation_set[p_anim];
		playback.assigned = p_anim;
	}
}

String AnimationPlayer::get_current_animation() const {

	return (playback.assigned);
}

void AnimationPlayer::stop(bool p_reset) {

	Playback &c = playback;
	c.blend.clear();
	if (p_reset) {
		c.current.from = NULL;
	}
	_set_process(false);
	queued.clear();
	playing = false;
}

void AnimationPlayer::stop_all() {

	stop();

	_set_process(false); // always process when starting an animation
}

void AnimationPlayer::set_speed(float p_speed) {

	speed_scale = p_speed;
}
float AnimationPlayer::get_speed() const {

	return speed_scale;
}

void AnimationPlayer::seek(float p_time, bool p_update) {

	if (!playback.current.from) {
		if (playback.assigned)
			set_current_animation(playback.assigned);
		ERR_FAIL_COND(!playback.current.from);
	}

	playback.current.pos = p_time;
	if (p_update) {
		_animation_process(0);
	}
}

void AnimationPlayer::seek_delta(float p_time, float p_delta) {

	if (!playback.current.from) {
		if (playback.assigned)
			set_current_animation(playback.assigned);
		ERR_FAIL_COND(!playback.current.from);
	}

	playback.current.pos = p_time - p_delta;
	if (speed_scale != 0.0)
		p_delta /= speed_scale;
	_animation_process(p_delta);
	//playback.current.pos=p_time;
}

bool AnimationPlayer::is_valid() const {

	return (playback.current.from);
}

float AnimationPlayer::get_current_animation_pos() const {

	ERR_FAIL_COND_V(!playback.current.from, 0);
	return playback.current.pos;
}

float AnimationPlayer::get_current_animation_length() const {

	ERR_FAIL_COND_V(!playback.current.from, 0);
	return playback.current.from->animation->get_length();
}

void AnimationPlayer::_animation_changed() {

	clear_caches();
}

void AnimationPlayer::_node_removed(Node *p_node) {

	clear_caches(); // nodes contained here ar being removed, clear the caches
}

void AnimationPlayer::clear_caches() {

	node_cache_map.clear();

	for (Map<StringName, AnimationData>::Element *E = animation_set.front(); E; E = E->next()) {

		E->get().node_cache.clear();
	}

	cache_update_size = 0;
	cache_update_prop_size = 0;
}

void AnimationPlayer::set_active(bool p_active) {

	if (active == p_active)
		return;

	active = p_active;
	_set_process(processing, true);
}

bool AnimationPlayer::is_active() const {

	return active;
}

StringName AnimationPlayer::find_animation(const Ref<Animation> &p_animation) const {

	for (Map<StringName, AnimationData>::Element *E = animation_set.front(); E; E = E->next()) {

		if (E->get().animation == p_animation)
			return E->key();
	}

	return "";
}

void AnimationPlayer::set_autoplay(const String &p_name) {

	autoplay = p_name;
}

String AnimationPlayer::get_autoplay() const {

	return autoplay;
}

void AnimationPlayer::set_animation_process_mode(AnimationProcessMode p_mode) {

	if (animation_process_mode == p_mode)
		return;

	bool pr = processing;
	if (pr)
		_set_process(false);
	animation_process_mode = p_mode;
	if (pr)
		_set_process(true);
}

AnimationPlayer::AnimationProcessMode AnimationPlayer::get_animation_process_mode() const {

	return animation_process_mode;
}

void AnimationPlayer::_set_process(bool p_process, bool p_force) {

	if (processing == p_process && !p_force)
		return;

	switch (animation_process_mode) {

		case ANIMATION_PROCESS_FIXED: set_fixed_process(p_process && active); break;
		case ANIMATION_PROCESS_IDLE: set_process(p_process && active); break;
	}

	processing = p_process;
}

void AnimationPlayer::animation_set_next(const StringName &p_animation, const StringName &p_next) {

	ERR_FAIL_COND(!animation_set.has(p_animation));
	animation_set[p_animation].next = p_next;
}

StringName AnimationPlayer::animation_get_next(const StringName &p_animation) const {

	if (!animation_set.has(p_animation))
		return StringName();
	return animation_set[p_animation].next;
}

void AnimationPlayer::set_default_blend_time(float p_default) {

	default_blend_time = p_default;
}

float AnimationPlayer::get_default_blend_time() const {

	return default_blend_time;
}

void AnimationPlayer::set_root(const NodePath &p_root) {

	root = p_root;
	clear_caches();
}

NodePath AnimationPlayer::get_root() const {

	return root;
}

void AnimationPlayer::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {

	String pf = p_function;
	if (p_function == "play" || p_function == "remove_animation" || p_function == "has_animation" || p_function == "queue") {
		List<StringName> al;
		get_animation_list(&al);
		for (List<StringName>::Element *E = al.front(); E; E = E->next()) {

			r_options->push_back("\"" + String(E->get()) + "\"");
		}
	}
	Node::get_argument_options(p_function, p_idx, r_options);
}

void AnimationPlayer::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_node_removed"), &AnimationPlayer::_node_removed);
	ObjectTypeDB::bind_method(_MD("_animation_changed"), &AnimationPlayer::_animation_changed);

	ObjectTypeDB::bind_method(_MD("add_animation", "name", "animation:Animation"), &AnimationPlayer::add_animation);
	ObjectTypeDB::bind_method(_MD("remove_animation", "name"), &AnimationPlayer::remove_animation);
	ObjectTypeDB::bind_method(_MD("rename_animation", "name", "newname"), &AnimationPlayer::rename_animation);
	ObjectTypeDB::bind_method(_MD("has_animation", "name"), &AnimationPlayer::has_animation);
	ObjectTypeDB::bind_method(_MD("get_animation:Animation", "name"), &AnimationPlayer::get_animation);
	ObjectTypeDB::bind_method(_MD("get_animation_list"), &AnimationPlayer::_get_animation_list);

	ObjectTypeDB::bind_method(_MD("animation_set_next", "anim_from", "anim_to"), &AnimationPlayer::animation_set_next);
	ObjectTypeDB::bind_method(_MD("animation_get_next", "anim_from"), &AnimationPlayer::animation_get_next);

	ObjectTypeDB::bind_method(_MD("set_blend_time", "anim_from", "anim_to", "sec"), &AnimationPlayer::set_blend_time);
	ObjectTypeDB::bind_method(_MD("get_blend_time", "anim_from", "anim_to"), &AnimationPlayer::get_blend_time);

	ObjectTypeDB::bind_method(_MD("set_default_blend_time", "sec"), &AnimationPlayer::set_default_blend_time);
	ObjectTypeDB::bind_method(_MD("get_default_blend_time"), &AnimationPlayer::get_default_blend_time);

	ObjectTypeDB::bind_method(_MD("play", "name", "custom_blend", "custom_speed", "from_end"), &AnimationPlayer::play, DEFVAL(""), DEFVAL(-1), DEFVAL(1.0), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("play_backwards", "name", "custom_blend"), &AnimationPlayer::play_backwards, DEFVAL(""), DEFVAL(-1));
	ObjectTypeDB::bind_method(_MD("stop", "reset"), &AnimationPlayer::stop, DEFVAL(true));
	ObjectTypeDB::bind_method(_MD("stop_all"), &AnimationPlayer::stop_all);
	ObjectTypeDB::bind_method(_MD("is_playing"), &AnimationPlayer::is_playing);
	ObjectTypeDB::bind_method(_MD("set_current_animation", "anim"), &AnimationPlayer::set_current_animation);
	ObjectTypeDB::bind_method(_MD("get_current_animation"), &AnimationPlayer::get_current_animation);
	ObjectTypeDB::bind_method(_MD("queue", "name"), &AnimationPlayer::queue);
	ObjectTypeDB::bind_method(_MD("clear_queue"), &AnimationPlayer::clear_queue);

	ObjectTypeDB::bind_method(_MD("set_active", "active"), &AnimationPlayer::set_active);
	ObjectTypeDB::bind_method(_MD("is_active"), &AnimationPlayer::is_active);

	ObjectTypeDB::bind_method(_MD("set_speed", "speed"), &AnimationPlayer::set_speed);
	ObjectTypeDB::bind_method(_MD("get_speed"), &AnimationPlayer::get_speed);

	ObjectTypeDB::bind_method(_MD("set_autoplay", "name"), &AnimationPlayer::set_autoplay);
	ObjectTypeDB::bind_method(_MD("get_autoplay"), &AnimationPlayer::get_autoplay);

	ObjectTypeDB::bind_method(_MD("set_root", "path"), &AnimationPlayer::set_root);
	ObjectTypeDB::bind_method(_MD("get_root"), &AnimationPlayer::get_root);

	ObjectTypeDB::bind_method(_MD("seek", "pos_sec", "update"), &AnimationPlayer::seek, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("get_pos"), &AnimationPlayer::get_current_animation_pos);

	ObjectTypeDB::bind_method(_MD("find_animation", "animation:Animation"), &AnimationPlayer::find_animation);

	ObjectTypeDB::bind_method(_MD("clear_caches"), &AnimationPlayer::clear_caches);

	ObjectTypeDB::bind_method(_MD("set_animation_process_mode", "mode"), &AnimationPlayer::set_animation_process_mode);
	ObjectTypeDB::bind_method(_MD("get_animation_process_mode"), &AnimationPlayer::get_animation_process_mode);

	ObjectTypeDB::bind_method(_MD("get_current_animation_pos"), &AnimationPlayer::get_current_animation_pos);
	ObjectTypeDB::bind_method(_MD("get_current_animation_length"), &AnimationPlayer::get_current_animation_length);

	ObjectTypeDB::bind_method(_MD("advance", "delta"), &AnimationPlayer::advance);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback/process_mode", PROPERTY_HINT_ENUM, "Fixed,Idle"), _SCS("set_animation_process_mode"), _SCS("get_animation_process_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "playback/default_blend_time", PROPERTY_HINT_RANGE, "0,4096,0.01"), _SCS("set_default_blend_time"), _SCS("get_default_blend_time"));
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root/root"), _SCS("set_root"), _SCS("get_root"));

	ADD_SIGNAL(MethodInfo("finished"));
	ADD_SIGNAL(MethodInfo("animation_changed", PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
	ADD_SIGNAL(MethodInfo("animation_started", PropertyInfo(Variant::STRING, "name")));

	BIND_CONSTANT(ANIMATION_PROCESS_FIXED);
	BIND_CONSTANT(ANIMATION_PROCESS_IDLE);
}

AnimationPlayer::AnimationPlayer() {

	accum_pass = 1;
	cache_update_size = 0;
	cache_update_prop_size = 0;
	speed_scale = 1;
	end_reached = false;
	animation_process_mode = ANIMATION_PROCESS_IDLE;
	processing = false;
	default_blend_time = 0;
	root = SceneStringNames::get_singleton()->path_pp;
	playing = false;
	active = true;
}

AnimationPlayer::~AnimationPlayer() {
}
