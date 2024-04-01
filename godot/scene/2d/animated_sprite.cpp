/*************************************************************************/
/*  animated_sprite.cpp                                                  */
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
#include "animated_sprite.h"
#include "os/os.h"
#include "scene/scene_string_names.h"

////////////////////////////

void SpriteFrames::add_frame(const StringName &p_anim, const Ref<Texture> &p_frame, int p_at_pos) {

	Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND(!E);

	if (p_at_pos >= 0 && p_at_pos < E->get().frames.size())
		E->get().frames.insert(p_at_pos, p_frame);
	else
		E->get().frames.push_back(p_frame);

	emit_changed();
}

int SpriteFrames::get_frame_count(const StringName &p_anim) const {
	const Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND_V(!E, 0);

	return E->get().frames.size();
}

void SpriteFrames::remove_frame(const StringName &p_anim, int p_idx) {

	Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND(!E);

	E->get().frames.remove(p_idx);
	emit_changed();
}
void SpriteFrames::clear(const StringName &p_anim) {

	Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND(!E);

	E->get().frames.clear();
	emit_changed();
}

void SpriteFrames::clear_all() {

	animations.clear();
	add_animation("default");
}

void SpriteFrames::add_animation(const StringName &p_anim) {

	ERR_FAIL_COND(animations.has(p_anim));

	animations[p_anim] = Anim();
}

bool SpriteFrames::has_animation(const StringName &p_anim) const {

	return animations.has(p_anim);
}
void SpriteFrames::remove_animation(const StringName &p_anim) {

	animations.erase(p_anim);
}

void SpriteFrames::rename_animation(const StringName &p_prev, const StringName &p_next) {

	ERR_FAIL_COND(!animations.has(p_prev));
	ERR_FAIL_COND(animations.has(p_next));

	Anim anim = animations[p_prev];
	animations.erase(p_prev);
	animations[p_next] = anim;
}

Vector<String> SpriteFrames::_get_animation_list() const {

	Vector<String> ret;
	List<StringName> al;
	get_animation_list(&al);
	for (List<StringName>::Element *E = al.front(); E; E = E->next()) {

		ret.push_back(E->get());
	}

	return ret;
}

void SpriteFrames::get_animation_list(List<StringName> *r_animations) const {

	for (const Map<StringName, Anim>::Element *E = animations.front(); E; E = E->next()) {
		r_animations->push_back(E->key());
	}
}

void SpriteFrames::set_animation_speed(const StringName &p_anim, float p_fps) {

	ERR_FAIL_COND(p_fps < 0);
	Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND(!E);
	E->get().speed = p_fps;
}
float SpriteFrames::get_animation_speed(const StringName &p_anim) const {

	const Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND_V(!E, 0);
	return E->get().speed;
}

void SpriteFrames::set_animation_loop(const StringName &p_anim, bool p_loop) {
	Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND(!E);
	E->get().loop = p_loop;
}
bool SpriteFrames::get_animation_loop(const StringName &p_anim) const {
	const Map<StringName, Anim>::Element *E = animations.find(p_anim);
	ERR_FAIL_COND_V(!E, false);
	return E->get().loop;
}

void SpriteFrames::_set_frames(const Array &p_frames) {

	clear_all();
	Map<StringName, Anim>::Element *E = animations.find(SceneStringNames::get_singleton()->_default);
	ERR_FAIL_COND(!E);

	E->get().frames.resize(p_frames.size());
	for (int i = 0; i < E->get().frames.size(); i++)
		E->get().frames[i] = p_frames[i];
}
Array SpriteFrames::_get_frames() const {

	return Array();
}

Array SpriteFrames::_get_animations() const {

	Array anims;
	for (Map<StringName, Anim>::Element *E = animations.front(); E; E = E->next()) {
		Dictionary d;
		d["name"] = E->key();
		d["speed"] = E->get().speed;
		d["loop"] = E->get().loop;
		Array frames;
		for (int i = 0; i < E->get().frames.size(); i++) {
			frames.push_back(E->get().frames[i]);
		}
		d["frames"] = frames;
		anims.push_back(d);
	}

	return anims;
}
void SpriteFrames::_set_animations(const Array &p_animations) {

	animations.clear();
	for (int i = 0; i < p_animations.size(); i++) {

		Dictionary d = p_animations[i];

		ERR_CONTINUE(!d.has("name"));
		ERR_CONTINUE(!d.has("speed"));
		ERR_CONTINUE(!d.has("loop"));
		ERR_CONTINUE(!d.has("frames"));

		Anim anim;
		anim.speed = d["speed"];
		anim.loop = d["loop"];
		Array frames = d["frames"];
		for (int i = 0; i < frames.size(); i++) {

			RES res = frames[i];
			anim.frames.push_back(res);
		}

		animations[d["name"]] = anim;
	}
}

void SpriteFrames::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_animation", "anim"), &SpriteFrames::add_animation);
	ObjectTypeDB::bind_method(_MD("has_animation", "anim"), &SpriteFrames::has_animation);
	ObjectTypeDB::bind_method(_MD("remove_animation", "anim"), &SpriteFrames::remove_animation);
	ObjectTypeDB::bind_method(_MD("rename_animation", "anim", "newname"), &SpriteFrames::rename_animation);

	ObjectTypeDB::bind_method(_MD("set_animation_speed", "anim", "speed"), &SpriteFrames::set_animation_speed);
	ObjectTypeDB::bind_method(_MD("get_animation_speed", "anim"), &SpriteFrames::get_animation_speed);

	ObjectTypeDB::bind_method(_MD("set_animation_loop", "anim", "loop"), &SpriteFrames::set_animation_loop);
	ObjectTypeDB::bind_method(_MD("get_animation_loop", "anim"), &SpriteFrames::get_animation_loop);

	ObjectTypeDB::bind_method(_MD("add_frame", "anim", "frame", "atpos"), &SpriteFrames::add_frame, DEFVAL(-1));
	ObjectTypeDB::bind_method(_MD("get_frame_count", "anim"), &SpriteFrames::get_frame_count);
	ObjectTypeDB::bind_method(_MD("get_frame", "anim", "idx"), &SpriteFrames::get_frame);
	ObjectTypeDB::bind_method(_MD("set_frame", "anim", "idx", "txt"), &SpriteFrames::set_frame);
	ObjectTypeDB::bind_method(_MD("remove_frame", "anim", "idx"), &SpriteFrames::remove_frame);
	ObjectTypeDB::bind_method(_MD("clear", "anim"), &SpriteFrames::clear);
	ObjectTypeDB::bind_method(_MD("clear_all"), &SpriteFrames::clear_all);

	ObjectTypeDB::bind_method(_MD("_set_frames"), &SpriteFrames::_set_frames);
	ObjectTypeDB::bind_method(_MD("_get_frames"), &SpriteFrames::_get_frames);

	ADD_PROPERTYNZ(PropertyInfo(Variant::ARRAY, "frames", PROPERTY_HINT_NONE, "", 0), _SCS("_set_frames"), _SCS("_get_frames")); //compatibility

	ObjectTypeDB::bind_method(_MD("_set_animations"), &SpriteFrames::_set_animations);
	ObjectTypeDB::bind_method(_MD("_get_animations"), &SpriteFrames::_get_animations);

	ADD_PROPERTYNZ(PropertyInfo(Variant::ARRAY, "animations", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), _SCS("_set_animations"), _SCS("_get_animations")); //compatibility
}

SpriteFrames::SpriteFrames() {

	add_animation(SceneStringNames::get_singleton()->_default);
}

void AnimatedSprite::edit_set_pivot(const Point2 &p_pivot) {

	set_offset(p_pivot);
}

Point2 AnimatedSprite::edit_get_pivot() const {

	return get_offset();
}
bool AnimatedSprite::edit_has_pivot() const {

	return true;
}

void AnimatedSprite::_validate_property(PropertyInfo &property) const {

	if (!frames.is_valid())
		return;
	if (property.name == "animation") {

		property.hint = PROPERTY_HINT_ENUM;
		List<StringName> names;
		frames->get_animation_list(&names);
		names.sort_custom<StringName::AlphCompare>();

		bool current_found = false;

		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
			if (E->prev()) {
				property.hint_string += ",";
			}

			property.hint_string += String(E->get());
			if (animation == E->get()) {
				current_found = true;
			}
		}

		if (!current_found) {
			if (property.hint_string == String()) {
				property.hint_string = String(animation);
			} else {
				property.hint_string = String(animation) + "," + property.hint_string;
			}
		}
	}

	if (property.name == "frame") {

		property.hint = PROPERTY_HINT_SPRITE_FRAME;

		if (frames->has_animation(animation)) {
			property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
		} else {
			property.hint_string = "0,0,0";
		}
	}
}

void AnimatedSprite::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_PROCESS: {

			if (frames.is_null())
				return;
			if (!frames->has_animation(animation))
				return;
			if (frame < 0)
				return;

			float speed = frames->get_animation_speed(animation);
			if (speed == 0)
				return; //do nothing

			float remaining = get_process_delta_time();

			while (remaining) {

				if (timeout <= 0) {

					timeout = 1.0 / speed;

					int fc = frames->get_frame_count(animation);
					if (frame >= fc - 1) {
						if (frames->get_animation_loop(animation)) {
							frame = 0;
						} else {
							frame = fc - 1;
						}
					} else {
						frame++;
						if (frame == fc - 1) {
							emit_signal(SceneStringNames::get_singleton()->finished);
						}
					}

					update();
					_change_notify("frame");
					emit_signal(SceneStringNames::get_singleton()->frame_changed);
				}

				float to_process = MIN(timeout, remaining);
				remaining -= to_process;
				timeout -= to_process;
			}
		} break;

		case NOTIFICATION_DRAW: {

			if (frames.is_null()) {
				print_line("no draw no faemos");
				return;
			}

			if (frame < 0) {
				print_line("no draw frame <0");
				return;
			}

			if (!frames->has_animation(animation)) {
				print_line("no draw no anim: " + String(animation));
				return;
			}

			Ref<Texture> texture = frames->get_frame(animation, frame);
			if (texture.is_null()) {
				print_line("no draw texture is null");
				return;
			}

			//print_line("DECIDED TO DRAW");

			RID ci = get_canvas_item();

			/*
			texture->draw(ci,Point2());
			break;
			*/

			Size2i s;
			s = texture->get_size();
			Point2 ofs = offset;
			if (centered)
				ofs -= s / 2;

			if (OS::get_singleton()->get_use_pixel_snap()) {
				ofs = ofs.floor();
			}
			Rect2 dst_rect(ofs, s);

			if (hflip)
				dst_rect.size.x = -dst_rect.size.x;
			if (vflip)
				dst_rect.size.y = -dst_rect.size.y;

			//texture->draw_rect(ci,dst_rect,false,modulate);
			texture->draw_rect_region(ci, dst_rect, Rect2(Vector2(), texture->get_size()), modulate);
			//			VisualServer::get_singleton()->canvas_item_add_texture_rect_region(ci,dst_rect,texture->get_rid(),src_rect,modulate);

		} break;
	}
}

void AnimatedSprite::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {

	if (frames.is_valid())
		frames->disconnect("changed", this, "_res_changed");
	frames = p_frames;
	if (frames.is_valid())
		frames->connect("changed", this, "_res_changed");

	if (!frames.is_valid()) {
		frame = 0;
	} else {
		set_frame(frame);
	}

	_change_notify();
	_reset_timeout();
	update();
	update_configuration_warning();
}

Ref<SpriteFrames> AnimatedSprite::get_sprite_frames() const {

	return frames;
}

void AnimatedSprite::set_frame(int p_frame) {

	if (!frames.is_valid()) {
		return;
	}

	if (frames->has_animation(animation)) {
		int limit = frames->get_frame_count(animation);
		if (p_frame >= limit)
			p_frame = limit - 1;
	}

	if (p_frame < 0)
		p_frame = 0;

	if (frame == p_frame)
		return;

	frame = p_frame;
	_reset_timeout();
	update();
	_change_notify("frame");
	emit_signal(SceneStringNames::get_singleton()->frame_changed);
}
int AnimatedSprite::get_frame() const {

	return frame;
}

void AnimatedSprite::set_centered(bool p_center) {

	centered = p_center;
	update();
	item_rect_changed();
}

bool AnimatedSprite::is_centered() const {

	return centered;
}

void AnimatedSprite::set_offset(const Point2 &p_offset) {

	offset = p_offset;
	update();
	item_rect_changed();
	_change_notify("offset");
}
Point2 AnimatedSprite::get_offset() const {

	return offset;
}

void AnimatedSprite::set_flip_h(bool p_flip) {

	hflip = p_flip;
	update();
}
bool AnimatedSprite::is_flipped_h() const {

	return hflip;
}

void AnimatedSprite::set_flip_v(bool p_flip) {

	vflip = p_flip;
	update();
}
bool AnimatedSprite::is_flipped_v() const {

	return vflip;
}

void AnimatedSprite::set_modulate(const Color &p_color) {

	modulate = p_color;
	update();
}

Color AnimatedSprite::get_modulate() const {

	return modulate;
}

Rect2 AnimatedSprite::get_item_rect() const {

	if (!frames.is_valid() || !frames->has_animation(animation) || frame < 0 || frame >= frames->get_frame_count(animation)) {
		return Node2D::get_item_rect();
	}

	Ref<Texture> t;
	if (animation)
		t = frames->get_frame(animation, frame);
	if (t.is_null())
		return Node2D::get_item_rect();
	Size2i s = t->get_size();

	Point2 ofs = offset;
	if (centered)
		ofs -= s / 2;

	if (s == Size2(0, 0))
		s = Size2(1, 1);

	return Rect2(ofs, s);
}

void AnimatedSprite::_res_changed() {

	set_frame(frame);
	_change_notify("frame");
	_change_notify("animation");
	update();
}

void AnimatedSprite::_set_playing(bool p_playing) {

	if (playing == p_playing)
		return;
	playing = p_playing;
	_reset_timeout();
	set_process(playing);
}

bool AnimatedSprite::_is_playing() const {

	return playing;
}

void AnimatedSprite::play(const StringName &p_animation) {

	if (p_animation)
		set_animation(p_animation);
	_set_playing(true);
}

void AnimatedSprite::stop() {

	_set_playing(false);
}

bool AnimatedSprite::is_playing() const {

	return is_processing();
}

void AnimatedSprite::_reset_timeout() {

	if (!playing)
		return;

	if (frames.is_valid() && frames->has_animation(animation)) {
		float speed = frames->get_animation_speed(animation);
		if (speed > 0) {
			timeout = 1.0 / speed;
		} else {
			timeout = 0;
		}
	} else {
		timeout = 0;
	}
}

void AnimatedSprite::set_animation(const StringName &p_animation) {

	if (animation == p_animation)
		return;

	animation = p_animation;
	_reset_timeout();
	set_frame(0);
	_change_notify();
	update();
}
StringName AnimatedSprite::get_animation() const {

	return animation;
}

String AnimatedSprite::get_configuration_warning() const {

	if (frames.is_null()) {
		return TTR("A SpriteFrames resource must be created or set in the 'Frames' property in order for AnimatedSprite to display frames.");
	}

	return String();
}

void AnimatedSprite::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_sprite_frames", "sprite_frames:SpriteFrames"), &AnimatedSprite::set_sprite_frames);
	ObjectTypeDB::bind_method(_MD("get_sprite_frames:SpriteFrames"), &AnimatedSprite::get_sprite_frames);

	ObjectTypeDB::bind_method(_MD("set_animation", "animation"), &AnimatedSprite::set_animation);
	ObjectTypeDB::bind_method(_MD("get_animation"), &AnimatedSprite::get_animation);

	ObjectTypeDB::bind_method(_MD("_set_playing", "playing"), &AnimatedSprite::_set_playing);
	ObjectTypeDB::bind_method(_MD("_is_playing"), &AnimatedSprite::_is_playing);

	ObjectTypeDB::bind_method(_MD("play", "anim"), &AnimatedSprite::play, DEFVAL(StringName()));
	ObjectTypeDB::bind_method(_MD("stop"), &AnimatedSprite::stop);
	ObjectTypeDB::bind_method(_MD("is_playing"), &AnimatedSprite::is_playing);

	ObjectTypeDB::bind_method(_MD("set_centered", "centered"), &AnimatedSprite::set_centered);
	ObjectTypeDB::bind_method(_MD("is_centered"), &AnimatedSprite::is_centered);

	ObjectTypeDB::bind_method(_MD("set_offset", "offset"), &AnimatedSprite::set_offset);
	ObjectTypeDB::bind_method(_MD("get_offset"), &AnimatedSprite::get_offset);

	ObjectTypeDB::bind_method(_MD("set_flip_h", "flip_h"), &AnimatedSprite::set_flip_h);
	ObjectTypeDB::bind_method(_MD("is_flipped_h"), &AnimatedSprite::is_flipped_h);

	ObjectTypeDB::bind_method(_MD("set_flip_v", "flip_v"), &AnimatedSprite::set_flip_v);
	ObjectTypeDB::bind_method(_MD("is_flipped_v"), &AnimatedSprite::is_flipped_v);

	ObjectTypeDB::bind_method(_MD("set_frame", "frame"), &AnimatedSprite::set_frame);
	ObjectTypeDB::bind_method(_MD("get_frame"), &AnimatedSprite::get_frame);

	ObjectTypeDB::bind_method(_MD("set_modulate", "modulate"), &AnimatedSprite::set_modulate);
	ObjectTypeDB::bind_method(_MD("get_modulate"), &AnimatedSprite::get_modulate);

	ObjectTypeDB::bind_method(_MD("_res_changed"), &AnimatedSprite::_res_changed);

	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("finished"));

	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "frames", PROPERTY_HINT_RESOURCE_TYPE, "SpriteFrames"), _SCS("set_sprite_frames"), _SCS("get_sprite_frames"));
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation"), _SCS("set_animation"), _SCS("get_animation"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::INT, "frame", PROPERTY_HINT_SPRITE_FRAME), _SCS("set_frame"), _SCS("get_frame"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "playing"), _SCS("_set_playing"), _SCS("_is_playing"));
	ADD_PROPERTYNO(PropertyInfo(Variant::BOOL, "centered"), _SCS("set_centered"), _SCS("is_centered"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "offset"), _SCS("set_offset"), _SCS("get_offset"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "flip_h"), _SCS("set_flip_h"), _SCS("is_flipped_h"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "flip_v"), _SCS("set_flip_v"), _SCS("is_flipped_v"));
	ADD_PROPERTYNO(PropertyInfo(Variant::COLOR, "modulate"), _SCS("set_modulate"), _SCS("get_modulate"));
}

AnimatedSprite::AnimatedSprite() {

	centered = true;
	hflip = false;
	vflip = false;

	frame = 0;
	playing = false;
	animation = "default";
	modulate = Color(1, 1, 1, 1);
	timeout = 0;
}
