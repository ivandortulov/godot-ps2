/*************************************************************************/
/*  range.h                                                              */
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
#ifndef RANGE_H
#define RANGE_H

#include "scene/gui/control.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class Range : public Control {

	OBJ_TYPE(Range, Control);

	struct Shared {
		double val, min, max;
		double step, page;
		bool exp_unit_value;
		Set<Range *> owners;
		void emit_value_changed();
		void emit_changed(const char *p_what = "");
	};

	Shared *shared;

	void _ref_shared(Shared *p_shared);
	void _unref_shared();

	void _share(Node *p_range);

	void _value_changed_notify();
	void _changed_notify(const char *p_what = "");

protected:
	virtual void _value_changed(double) {}

	static void _bind_methods();

	bool _rounded_values;

public:
	void set_val(double p_val);
	void set_min(double p_min);
	void set_max(double p_max);
	void set_step(double p_step);
	void set_page(double p_page);
	void set_unit_value(double p_value);

	double get_val() const;
	double get_min() const;
	double get_max() const;
	double get_step() const;
	double get_page() const;
	double get_unit_value() const;

	void set_rounded_values(bool p_enable);
	bool is_rounded_values() const;

	void set_exp_unit_value(bool p_enable);
	bool is_unit_value_exp() const;

	void share(Range *p_range);
	void unshare();

	Range();
	~Range();
};

#endif
