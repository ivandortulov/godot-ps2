/*************************************************************************/
/*  remote_transform_2d.cpp                                              */
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
#include "remote_transform_2d.h"
#include "scene/scene_string_names.h"

void RemoteTransform2D::_update_cache() {

	cache = 0;
	if (has_node(remote_node)) {
		Node *node = get_node(remote_node);
		if (!node || this == node || node->is_a_parent_of(this) || this->is_a_parent_of(node)) {
			return;
		}

		cache = node->get_instance_ID();
	}
}

void RemoteTransform2D::_update_remote() {

	if (!is_inside_tree())
		return;

	if (!cache)
		return;

	Object *obj = ObjectDB::get_instance(cache);
	if (!obj)
		return;

	Node2D *n = obj->cast_to<Node2D>();
	if (!n)
		return;

	if (!n->is_inside_tree())
		return;

	//todo make faster
	n->set_global_transform(get_global_transform());
}

void RemoteTransform2D::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_READY: {

			_update_cache();

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (!is_inside_tree())
				break;

			if (cache) {

				_update_remote();
			}

		} break;
	}
}

void RemoteTransform2D::set_remote_node(const NodePath &p_remote_node) {

	remote_node = p_remote_node;
	if (is_inside_tree())
		_update_cache();

	update_configuration_warning();
}

NodePath RemoteTransform2D::get_remote_node() const {

	return remote_node;
}

String RemoteTransform2D::get_configuration_warning() const {

	if (!has_node(remote_node) || !get_node(remote_node) || !get_node(remote_node)->cast_to<Node2D>()) {
		return TTR("Path property must point to a valid Node2D node to work.");
	}

	return String();
}

void RemoteTransform2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_remote_node", "path"), &RemoteTransform2D::set_remote_node);
	ObjectTypeDB::bind_method(_MD("get_remote_node"), &RemoteTransform2D::get_remote_node);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "remote_path"), _SCS("set_remote_node"), _SCS("get_remote_node"));
}

RemoteTransform2D::RemoteTransform2D() {

	cache = 0;
}
