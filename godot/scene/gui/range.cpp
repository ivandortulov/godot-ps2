/*************************************************************************/
/*  range.cpp                                                            */
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
#include "range.h"

void Range::_value_changed_notify() {

	_value_changed(shared->val);
	emit_signal("value_changed", shared->val);
	update();
	_change_notify("range/value");
}

void Range::Shared::emit_value_changed() {

	for (Set<Range *>::Element *E = owners.front(); E; E = E->next()) {
		Range *r = E->get();
		if (!r->is_inside_tree())
			continue;
		r->_value_changed_notify();
	}
}

void Range::_changed_notify(const char *p_what) {

	emit_signal("changed", shared->val);
	update();
	_change_notify(p_what);
}

void Range::Shared::emit_changed(const char *p_what) {

	for (Set<Range *>::Element *E = owners.front(); E; E = E->next()) {
		Range *r = E->get();
		if (!r->is_inside_tree())
			continue;
		r->_changed_notify(p_what);
	}
}

void Range::set_val(double p_val) {

	if (_rounded_values) {
		p_val = Math::round(p_val);
	}

	if (p_val > shared->max - shared->page)
		p_val = shared->max - shared->page;

	if (p_val < shared->min)
		p_val = shared->min;

	if (shared->val == p_val)
		return;

	shared->val = p_val;

	shared->emit_value_changed();
}
void Range::set_min(double p_min) {

	shared->min = p_min;
	set_val(shared->val);

	shared->emit_changed("range/min");
}
void Range::set_max(double p_max) {

	shared->max = p_max;
	set_val(shared->val);

	shared->emit_changed("range/max");
}
void Range::set_step(double p_step) {

	shared->step = p_step;
	shared->emit_changed("range/step");
}
void Range::set_page(double p_page) {

	shared->page = p_page;
	set_val(shared->val);

	shared->emit_changed("range/page");
}

double Range::get_val() const {

	return shared->val;
}
double Range::get_min() const {

	return shared->min;
}
double Range::get_max() const {

	return shared->max;
}
double Range::get_step() const {

	return shared->step;
}
double Range::get_page() const {

	return shared->page;
}

void Range::set_unit_value(double p_value) {

	double v;

	if (shared->exp_unit_value && get_min() > 0) {

		double exp_min = Math::log(get_min()) / Math::log(2);
		double exp_max = Math::log(get_max()) / Math::log(2);
		v = Math::pow(2, exp_min + (exp_max - exp_min) * p_value);
	} else {

		double percent = (get_max() - get_min()) * p_value;
		if (get_step() > 0) {
			double steps = round(percent / get_step());
			v = steps * get_step() + get_min();
		} else {
			v = percent + get_min();
		}
	}
	set_val(v);
}
double Range::get_unit_value() const {

	if (shared->exp_unit_value && get_min() > 0) {

		double exp_min = Math::log(get_min()) / Math::log(2);
		double exp_max = Math::log(get_max()) / Math::log(2);
		double v = Math::log(get_val()) / Math::log(2);

		return (v - exp_min) / (exp_max - exp_min);

	} else {

		return (get_val() - get_min()) / (get_max() - get_min());
	}
}

void Range::_share(Node *p_range) {

	Range *r = p_range->cast_to<Range>();
	ERR_FAIL_COND(!r);
	share(r);
}

void Range::share(Range *p_range) {

	ERR_FAIL_NULL(p_range);

	p_range->_ref_shared(shared);
	p_range->_changed_notify();
	p_range->_value_changed_notify();
}

void Range::unshare() {

	Shared *nshared = memnew(Shared);
	nshared->min = shared->min;
	nshared->max = shared->max;
	nshared->val = shared->val;
	nshared->step = shared->step;
	nshared->page = shared->page;
	_unref_shared();
	_ref_shared(nshared);
}

void Range::_ref_shared(Shared *p_shared) {

	if (shared && p_shared == shared)
		return;

	_unref_shared();
	shared = p_shared;
	shared->owners.insert(this);
}

void Range::_unref_shared() {

	shared->owners.erase(this);
	if (shared->owners.size() == 0) {
		memdelete(shared);
		shared = NULL;
	}
}

void Range::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_val"), &Range::get_val);
	ObjectTypeDB::bind_method(_MD("get_value"), &Range::get_val);
	ObjectTypeDB::bind_method(_MD("get_min"), &Range::get_min);
	ObjectTypeDB::bind_method(_MD("get_max"), &Range::get_max);
	ObjectTypeDB::bind_method(_MD("get_step"), &Range::get_step);
	ObjectTypeDB::bind_method(_MD("get_page"), &Range::get_page);
	ObjectTypeDB::bind_method(_MD("get_unit_value"), &Range::get_unit_value);
	ObjectTypeDB::bind_method(_MD("set_val", "value"), &Range::set_val);
	ObjectTypeDB::bind_method(_MD("set_value", "value"), &Range::set_val);
	ObjectTypeDB::bind_method(_MD("set_min", "minimum"), &Range::set_min);
	ObjectTypeDB::bind_method(_MD("set_max", "maximum"), &Range::set_max);
	ObjectTypeDB::bind_method(_MD("set_step", "step"), &Range::set_step);
	ObjectTypeDB::bind_method(_MD("set_page", "pagesize"), &Range::set_page);
	ObjectTypeDB::bind_method(_MD("set_unit_value", "value"), &Range::set_unit_value);
	ObjectTypeDB::bind_method(_MD("set_rounded_values", "enabled"), &Range::set_rounded_values);
	ObjectTypeDB::bind_method(_MD("is_rounded_values"), &Range::is_rounded_values);
	ObjectTypeDB::bind_method(_MD("set_exp_unit_value", "enabled"), &Range::set_exp_unit_value);
	ObjectTypeDB::bind_method(_MD("is_unit_value_exp"), &Range::is_unit_value_exp);

	ObjectTypeDB::bind_method(_MD("share", "with"), &Range::_share);
	ObjectTypeDB::bind_method(_MD("unshare"), &Range::unshare);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::REAL, "value")));
	ADD_SIGNAL(MethodInfo("changed"));

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range/min"), _SCS("set_min"), _SCS("get_min"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range/max"), _SCS("set_max"), _SCS("get_max"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range/step"), _SCS("set_step"), _SCS("get_step"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range/page"), _SCS("set_page"), _SCS("get_page"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range/value"), _SCS("set_val"), _SCS("get_val"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "range/exp_edit"), _SCS("set_exp_unit_value"), _SCS("is_unit_value_exp"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "range/rounded"), _SCS("set_rounded_values"), _SCS("is_rounded_values"));
}

void Range::set_rounded_values(bool p_enable) {

	_rounded_values = p_enable;
}

bool Range::is_rounded_values() const {

	return _rounded_values;
}

void Range::set_exp_unit_value(bool p_enable) {

	shared->exp_unit_value = p_enable;
}

bool Range::is_unit_value_exp() const {

	return shared->exp_unit_value;
}

Range::Range() {
	shared = memnew(Shared);
	shared->min = 0;
	shared->max = 100;
	shared->val = 0;
	shared->step = 1;
	shared->page = 0;
	shared->owners.insert(this);
	shared->exp_unit_value = false;

	_rounded_values = false;
}

Range::~Range() {

	_unref_shared();
}
