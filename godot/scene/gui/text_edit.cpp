/*************************************************************************/
/*  text_edit.cpp                                                        */
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

#include "text_edit.h"
#include "os/input.h"
#include "os/keyboard.h"
#include "os/os.h"

#include "globals.h"
#include "message_queue.h"

#define TAB_PIXELS

static bool _is_text_char(CharType c) {

	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool _is_symbol(CharType c) {

	return c != '_' && ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~') || c == '\t' || c == ' ');
}

static bool _is_char(CharType c) {

	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool _is_number(CharType c) {
	return (c >= '0' && c <= '9');
}

static bool _is_hex_symbol(CharType c) {
	return ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static bool _is_pair_right_symbol(CharType c) {
	return c == '"' ||
		   c == '\'' ||
		   c == ')' ||
		   c == ']' ||
		   c == '}';
}

static bool _is_pair_left_symbol(CharType c) {
	return c == '"' ||
		   c == '\'' ||
		   c == '(' ||
		   c == '[' ||
		   c == '{';
}

static bool _is_pair_symbol(CharType c) {
	return _is_pair_left_symbol(c) || _is_pair_right_symbol(c);
}

static CharType _get_right_pair_symbol(CharType c) {
	if (c == '"')
		return '"';
	if (c == '\'')
		return '\'';
	if (c == '(')
		return ')';
	if (c == '[')
		return ']';
	if (c == '{')
		return '}';
	return 0;
}

void TextEdit::Text::set_font(const Ref<Font> &p_font) {

	font = p_font;
}

void TextEdit::Text::set_tab_size(int p_tab_size) {

	tab_size = p_tab_size;
}

void TextEdit::Text::_update_line_cache(int p_line) const {

	int w = 0;
	int tab_w = font->get_char_size(' ').width * tab_size;

	int len = text[p_line].data.length();
	const CharType *str = text[p_line].data.c_str();

	//update width

	for (int i = 0; i < len; i++) {
		if (str[i] == '\t') {

			int left = w % tab_w;
			if (left == 0)
				w += tab_w;
			else
				w += tab_w - w % tab_w; // is right...

		} else {

			w += font->get_char_size(str[i], str[i + 1]).width;
		}
	}

	text[p_line].width_cache = w;

	//update regions

	text[p_line].region_info.clear();

	for (int i = 0; i < len; i++) {

		if (!_is_symbol(str[i]))
			continue;
		if (str[i] == '\\') {
			i++; //skip quoted anything
			continue;
		}

		int left = len - i;

		for (int j = 0; j < color_regions->size(); j++) {

			const ColorRegion &cr = color_regions->operator[](j);

			/* BEGIN */

			int lr = cr.begin_key.length();
			if (lr == 0 || lr > left)
				continue;

			const CharType *kc = cr.begin_key.c_str();

			bool match = true;

			for (int k = 0; k < lr; k++) {
				if (kc[k] != str[i + k]) {
					match = false;
					break;
				}
			}

			if (match) {

				ColorRegionInfo cri;
				cri.end = false;
				cri.region = j;
				text[p_line].region_info[i] = cri;
				i += lr - 1;
				break;
			}

			/* END */

			lr = cr.end_key.length();
			if (lr == 0 || lr > left)
				continue;

			kc = cr.end_key.c_str();

			match = true;

			for (int k = 0; k < lr; k++) {
				if (kc[k] != str[i + k]) {
					match = false;
					break;
				}
			}

			if (match) {

				ColorRegionInfo cri;
				cri.end = true;
				cri.region = j;
				text[p_line].region_info[i] = cri;
				i += lr - 1;
				break;
			}
		}
	}
}

const Map<int, TextEdit::Text::ColorRegionInfo> &TextEdit::Text::get_color_region_info(int p_line) {

	Map<int, ColorRegionInfo> *cri = NULL;
	ERR_FAIL_INDEX_V(p_line, text.size(), *cri); //enjoy your crash

	if (text[p_line].width_cache == -1) {
		_update_line_cache(p_line);
	}

	return text[p_line].region_info;
}

int TextEdit::Text::get_line_width(int p_line) const {

	ERR_FAIL_INDEX_V(p_line, text.size(), -1);

	if (text[p_line].width_cache == -1) {
		_update_line_cache(p_line);
	}

	return text[p_line].width_cache;
}

void TextEdit::Text::clear_caches() {

	for (int i = 0; i < text.size(); i++)
		text[i].width_cache = -1;
}

void TextEdit::Text::clear() {

	text.clear();
	insert(0, "");
}

int TextEdit::Text::get_max_width() const {
	//quite some work.. but should be fast enough.

	int max = 0;

	for (int i = 0; i < text.size(); i++)
		max = MAX(max, get_line_width(i));
	return max;
}

void TextEdit::Text::set(int p_line, const String &p_text) {

	ERR_FAIL_INDEX(p_line, text.size());

	text[p_line].width_cache = -1;
	text[p_line].data = p_text;
}

void TextEdit::Text::insert(int p_at, const String &p_text) {

	Line line;
	line.marked = false;
	line.breakpoint = false;
	line.width_cache = -1;
	line.data = p_text;
	text.insert(p_at, line);
}
void TextEdit::Text::remove(int p_at) {

	text.remove(p_at);
}

void TextEdit::_update_scrollbars() {

	Size2 size = get_size();
	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	v_scroll->set_begin(Point2(size.width - vmin.width, cache.style_normal->get_margin(MARGIN_TOP)));
	v_scroll->set_end(Point2(size.width, size.height - cache.style_normal->get_margin(MARGIN_TOP) - cache.style_normal->get_margin(MARGIN_BOTTOM)));

	h_scroll->set_begin(Point2(0, size.height - hmin.height));
	h_scroll->set_end(Point2(size.width - vmin.width, size.height));

	int hscroll_rows = ((hmin.height - 1) / get_row_height()) + 1;
	int visible_rows = get_visible_rows();
	int total_rows = text.size();
	if (scroll_past_end_of_file_enabled) {
		total_rows += get_visible_rows() - 1;
	}

	int vscroll_pixels = v_scroll->get_combined_minimum_size().width;
	int visible_width = size.width - cache.style_normal->get_minimum_size().width;
	int total_width = text.get_max_width() + vmin.x;

	if (line_numbers)
		total_width += cache.line_number_w;

	if (draw_breakpoint_gutter) {
		total_width += cache.breakpoint_gutter_width;
	}

	bool use_hscroll = true;
	bool use_vscroll = true;

	if (total_rows <= visible_rows && total_width <= visible_width) {
		//thanks yessopie for this clever bit of logic
		use_hscroll = false;
		use_vscroll = false;

	} else {

		if (total_rows > visible_rows && total_width <= visible_width - vscroll_pixels) {
			//thanks yessopie for this clever bit of logic
			use_hscroll = false;
		}

		if (total_rows <= visible_rows - hscroll_rows && total_width > visible_width) {
			//thanks yessopie for this clever bit of logic
			use_vscroll = false;
		}
	}

	updating_scrolls = true;

	if (use_vscroll) {

		v_scroll->show();
		v_scroll->set_max(total_rows);
		v_scroll->set_page(visible_rows);

		if (fabs(v_scroll->get_val() - (double)cursor.line_ofs) >= 1) {
			v_scroll->set_val(cursor.line_ofs);
		}

	} else {
		cursor.line_ofs = 0;
		v_scroll->hide();
	}

	if (use_hscroll) {

		h_scroll->show();
		h_scroll->set_max(total_width);
		h_scroll->set_page(visible_width);
		if (fabs(h_scroll->get_val() - (double)cursor.x_ofs) >= 1) {
			h_scroll->set_val(cursor.x_ofs);
		}

	} else {

		h_scroll->hide();
	}

	updating_scrolls = false;
}

void TextEdit::_click_selection_held() {

	if (Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT) && selection.selecting_mode != Selection::MODE_NONE) {

		Point2 mp = Input::get_singleton()->get_mouse_pos() - get_global_pos();

		int row, col;
		_get_mouse_pos(Point2i(mp.x, mp.y), row, col);

		select(selection.selecting_line, selection.selecting_column, row, col);

		cursor_set_line(row);
		cursor_set_column(col);
		update();

		click_select_held->start();

	} else {

		click_select_held->stop();
	}
}

void TextEdit::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {

			_update_caches();
			if (cursor_changed_dirty)
				MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
			if (text_changed_dirty)
				MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");

		} break;
		case NOTIFICATION_RESIZED: {

			cache.size = get_size();
			adjust_viewport_to_cursor();

		} break;
		case NOTIFICATION_THEME_CHANGED: {

			_update_caches();
		} break;
		case MainLoop::NOTIFICATION_WM_FOCUS_IN: {
			window_has_focus = true;
			draw_caret = true;
			update();
		} break;
		case MainLoop::NOTIFICATION_WM_FOCUS_OUT: {
			window_has_focus = false;
			draw_caret = false;
			update();
		} break;
		case NOTIFICATION_DRAW: {

			if ((!has_focus() && !menu->has_focus()) || !window_has_focus) {
				draw_caret = false;
			}

			if (draw_breakpoint_gutter) {
				breakpoint_gutter_width = (get_row_height() * 55) / 100;
				cache.breakpoint_gutter_width = breakpoint_gutter_width;
			} else {
				cache.breakpoint_gutter_width = 0;
			}

			int line_number_char_count = 0;

			{
				int lc = text.size();
				cache.line_number_w = 0;
				while (lc) {
					cache.line_number_w += 1;
					lc /= 10;
				};

				if (line_numbers) {

					line_number_char_count = cache.line_number_w;
					cache.line_number_w = (cache.line_number_w + 1) * cache.font->get_char_size('0').width;
				} else {
					cache.line_number_w = 0;
				}
			}
			_update_scrollbars();

			RID ci = get_canvas_item();
			int xmargin_beg = cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width;
			int xmargin_end = cache.size.width - cache.style_normal->get_margin(MARGIN_RIGHT);
			//let's do it easy for now:
			cache.style_normal->draw(ci, Rect2(Point2(), cache.size));
			if (has_focus())
				cache.style_focus->draw(ci, Rect2(Point2(), cache.size));

			int ascent = cache.font->get_ascent();

			int visible_rows = get_visible_rows();

			int tab_w = cache.font->get_char_size(' ').width * tab_size;

			Color color = cache.font_color;
			int in_region = -1;

			if (syntax_coloring) {

				if (custom_bg_color.a > 0.01) {

					Point2i ofs = Point2i(cache.style_normal->get_offset()) / 2.0;
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(ofs, get_size() - cache.style_normal->get_minimum_size() + ofs), custom_bg_color);
				}
				//compute actual region to start (may be inside say, a comment).
				//slow in very large documments :( but ok for source!

				for (int i = 0; i < cursor.line_ofs; i++) {

					const Map<int, Text::ColorRegionInfo> &cri_map = text.get_color_region_info(i);

					if (in_region >= 0 && color_regions[in_region].line_only) {
						in_region = -1; //reset regions that end at end of line
					}

					for (const Map<int, Text::ColorRegionInfo>::Element *E = cri_map.front(); E; E = E->next()) {

						const Text::ColorRegionInfo &cri = E->get();

						if (in_region == -1) {

							if (!cri.end) {

								in_region = cri.region;
							}
						} else if (in_region == cri.region && !color_regions[cri.region].line_only) { //ignore otherwise

							if (cri.end || color_regions[cri.region].eq) {

								in_region = -1;
							}
						}
					}
				}
			}

			int brace_open_match_line = -1;
			int brace_open_match_column = -1;
			bool brace_open_matching = false;
			bool brace_open_mismatch = false;
			int brace_close_match_line = -1;
			int brace_close_match_column = -1;
			bool brace_close_matching = false;
			bool brace_close_mismatch = false;

			if (brace_matching_enabled) {

				if (cursor.column < text[cursor.line].length()) {
					//check for open
					CharType c = text[cursor.line][cursor.column];
					CharType closec = 0;

					if (c == '[') {
						closec = ']';
					} else if (c == '{') {
						closec = '}';
					} else if (c == '(') {
						closec = ')';
					}

					if (closec != 0) {

						int stack = 1;

						for (int i = cursor.line; i < text.size(); i++) {

							int from = i == cursor.line ? cursor.column + 1 : 0;
							for (int j = from; j < text[i].length(); j++) {

								CharType cc = text[i][j];
								//ignore any brackets inside a string
								if (cc == '"' || cc == '\'') {
									CharType quotation = cc;
									do {
										j++;
										if (!(j < text[i].length())) {
											break;
										}
										cc = text[i][j];
										//skip over escaped quotation marks inside strings
										if (cc == '\\') {
											bool escaped = true;
											while (j + 1 < text[i].length() && text[i][j + 1] == '\\') {
												escaped = !escaped;
												j++;
											}
											if (escaped) {
												j++;
												continue;
											}
										}
									} while (cc != quotation);
								} else if (cc == c)
									stack++;
								else if (cc == closec)
									stack--;

								if (stack == 0) {
									brace_open_match_line = i;
									brace_open_match_column = j;
									brace_open_matching = true;

									break;
								}
							}
							if (brace_open_match_line != -1)
								break;
						}

						if (!brace_open_matching)
							brace_open_mismatch = true;
					}
				}

				if (cursor.column > 0) {
					CharType c = text[cursor.line][cursor.column - 1];
					CharType closec = 0;

					if (c == ']') {
						closec = '[';
					} else if (c == '}') {
						closec = '{';
					} else if (c == ')') {
						closec = '(';
					}

					if (closec != 0) {

						int stack = 1;

						for (int i = cursor.line; i >= 0; i--) {

							int from = i == cursor.line ? cursor.column - 2 : text[i].length() - 1;
							for (int j = from; j >= 0; j--) {

								CharType cc = text[i][j];
								//ignore any brackets inside a string
								if (cc == '"' || cc == '\'') {
									CharType quotation = cc;
									do {
										j--;
										if (!(j >= 0)) {
											break;
										}
										cc = text[i][j];
										//skip over escaped quotation marks inside strings
										if (cc == quotation) {
											bool escaped = false;
											while (j - 1 >= 0 && text[i][j - 1] == '\\') {
												escaped = !escaped;
												j--;
											}
											if (escaped) {
												j--;
												cc = '\\';
												continue;
											}
										}
									} while (cc != quotation);
								} else if (cc == c)
									stack++;
								else if (cc == closec)
									stack--;

								if (stack == 0) {
									brace_close_match_line = i;
									brace_close_match_column = j;
									brace_close_matching = true;

									break;
								}
							}
							if (brace_close_match_line != -1)
								break;
						}

						if (!brace_close_matching)
							brace_close_mismatch = true;
					}
				}
			}

			int deregion = 0; //force it to clear inrgion
			Point2 cursor_pos;

			// get the highlighted words
			String highlighted_text = get_selection_text();

			String line_num_padding = line_numbers_zero_padded ? "0" : " ";

			for (int i = 0; i < visible_rows; i++) {

				int line = i + cursor.line_ofs;

				if (line < 0 || line >= (int)text.size())
					continue;

				const String &str = text[line];

				int char_margin = xmargin_beg - cursor.x_ofs;
				int char_ofs = 0;
				int ofs_y = i * get_row_height() + cache.line_spacing / 2;
				bool prev_is_char = false;
				bool prev_is_number = false;
				bool in_keyword = false;
				bool in_word = false;
				bool in_function_name = false;
				bool in_member_variable = false;
				bool is_hex_notation = false;
				Color keyword_color;

				// check if line contains highlighted word
				int highlighted_text_col = -1;
				int search_text_col = -1;

				if (!search_text.empty())
					search_text_col = _get_column_pos_of_word(search_text, str, search_flags, 0);

				if (highlighted_text.length() != 0 && highlighted_text != search_text)
					highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, 0);

				const Map<int, Text::ColorRegionInfo> &cri_map = text.get_color_region_info(line);

				if (text.is_marked(line)) {

					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg, ofs_y, xmargin_end - xmargin_beg, get_row_height()), cache.mark_color);
				}

				if (text.is_breakpoint(line)) {

					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(xmargin_beg, ofs_y, xmargin_end - xmargin_beg, get_row_height()), cache.breakpoint_color);
				}

				if (line == cursor.line) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(0, ofs_y, xmargin_end, get_row_height()), cache.current_line_color);
				}

				// draw breakpoint marker
				if (text.is_breakpoint(line)) {
					if (draw_breakpoint_gutter) {
						int vertical_gap = (get_row_height() * 40) / 100;
						int horizontal_gap = (cache.breakpoint_gutter_width * 30) / 100;
						int marker_height = get_row_height() - (vertical_gap * 2);
						int marker_width = cache.breakpoint_gutter_width - (horizontal_gap * 2);
						// no transparency on marker
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cache.style_normal->get_margin(MARGIN_LEFT) + horizontal_gap - 2, ofs_y + vertical_gap, marker_width, marker_height), Color(cache.breakpoint_color.r, cache.breakpoint_color.g, cache.breakpoint_color.b));
					}
				}

				if (cache.line_number_w) {
					String fc = String::num(line + 1);
					while (fc.length() < line_number_char_count) {
						fc = line_num_padding + fc;
					}

					cache.font->draw(ci, Point2(cache.style_normal->get_margin(MARGIN_LEFT) + cache.breakpoint_gutter_width, ofs_y + cache.font->get_ascent()), fc, cache.line_number_color);
				}
				for (int j = 0; j < str.length(); j++) {

					//look for keyword

					if (deregion > 0) {
						deregion--;
						if (deregion == 0)
							in_region = -1;
					}
					if (syntax_coloring && deregion == 0) {

						color = cache.font_color; //reset
						//find keyword
						bool is_char = _is_text_char(str[j]);
						bool is_symbol = _is_symbol(str[j]);
						bool is_number = _is_number(str[j]);

						if (j == 0 && in_region >= 0 && color_regions[in_region].line_only) {
							in_region = -1; //reset regions that end at end of line
						}

						// allow ABCDEF in hex notation
						if (is_hex_notation && (_is_hex_symbol(str[j]) || is_number)) {
							is_number = true;
						} else {
							is_hex_notation = false;
						}

						// check for dot or 'x' for hex notation in floating point number
						if ((str[j] == '.' || str[j] == 'x') && !in_word && prev_is_number && !is_number) {
							is_number = true;
							is_symbol = false;

							if (str[j] == 'x' && str[j - 1] == '0') {
								is_hex_notation = true;
							}
						}

						if (!in_word && _is_char(str[j])) {
							in_word = true;
						}

						if ((in_keyword || in_word) && !is_hex_notation) {
							is_number = false;
						}

						if (is_symbol && str[j] != '.' && in_word) {
							in_word = false;
						}

						if (is_symbol && cri_map.has(j)) {

							const Text::ColorRegionInfo &cri = cri_map[j];

							if (in_region == -1) {

								if (!cri.end) {

									in_region = cri.region;
								}
							} else if (in_region == cri.region && !color_regions[cri.region].line_only) { //ignore otherwise

								if (cri.end || color_regions[cri.region].eq) {

									deregion = color_regions[cri.region].eq ? color_regions[cri.region].begin_key.length() : color_regions[cri.region].end_key.length();
								}
							}
						}

						if (!is_char)
							in_keyword = false;

						if (in_region == -1 && !in_keyword && is_char && !prev_is_char) {

							int to = j;
							while (to < str.length() && _is_text_char(str[to]))
								to++;

							uint32_t hash = String::hash(&str[j], to - j);
							StrRange range(&str[j], to - j);

							const Color *col = keywords.custom_getptr(range, hash);

							if (col) {

								in_keyword = true;
								keyword_color = *col;
							}
						}

						if (!in_function_name && in_word && !in_keyword) {

							int k = j;
							while (k < str.length() && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
								k++;
							}

							// check for space between name and bracket
							while (k < str.length() && (str[k] == '\t' || str[k] == ' ')) {
								k++;
							}

							if (str[k] == '(') {
								in_function_name = true;
							}
						}

						if (!in_function_name && !in_member_variable && !in_keyword && !is_number && in_word) {
							int k = j;
							while (k > 0 && !_is_symbol(str[k]) && str[k] != '\t' && str[k] != ' ') {
								k--;
							}

							if (str[k] == '.') {
								in_member_variable = true;
							}
						}

						if (is_symbol) {
							in_function_name = false;
							in_member_variable = false;
						}

						if (in_region >= 0)
							color = color_regions[in_region].color;
						else if (in_keyword)
							color = keyword_color;
						else if (in_member_variable)
							color = cache.member_variable_color;
						else if (in_function_name)
							color = cache.function_color;
						else if (is_symbol)
							color = symbol_color;
						else if (is_number)
							color = cache.number_color;

						prev_is_char = is_char;
						prev_is_number = is_number;
					}
					int char_w;

					//handle tabulator

					if (str[j] == '\t') {
						int left = char_ofs % tab_w;
						if (left == 0)
							char_w = tab_w;
						else
							char_w = tab_w - char_ofs % tab_w; // is right...

					} else {
						char_w = cache.font->get_char_size(str[j], str[j + 1]).width;
					}

					if ((char_ofs + char_margin) < xmargin_beg) {
						char_ofs += char_w;
						continue;
					}

					if ((char_ofs + char_margin + char_w) >= xmargin_end) {
						if (syntax_coloring)
							continue;
						else
							break;
					}

					bool in_search_result = false;

					if (search_text_col != -1) {
						// if we are at the end check for new search result on same line
						if (j >= search_text_col + search_text.length())
							search_text_col = _get_column_pos_of_word(search_text, str, search_flags, j);

						in_search_result = j >= search_text_col && j < search_text_col + search_text.length();

						if (in_search_result) {
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, get_row_height())), cache.search_result_color);
						}
					}

					bool in_selection = (selection.active && line >= selection.from_line && line <= selection.to_line && (line > selection.from_line || j >= selection.from_column) && (line < selection.to_line || j < selection.to_column));

					if (in_selection) {
						//inside selection!
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, get_row_height())), cache.selection_color);
					}

					if (in_search_result) {
						Color border_color = (line == search_result_line && j >= search_result_col && j < search_result_col + search_text.length()) ? cache.font_color : cache.search_result_border_color;

						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, 1)), border_color);
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y + get_row_height() - 1), Size2i(char_w, 1)), border_color);

						if (j == search_text_col)
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(1, get_row_height())), border_color);
						if (j == search_text_col + search_text.length() - 1)
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin + char_w - 1, ofs_y), Size2i(1, get_row_height())), border_color);
					}

					if (highlight_all_occurrences) {
						if (highlighted_text_col != -1) {

							// if we are at the end check for new word on same line
							if (j > highlighted_text_col + highlighted_text.length()) {
								highlighted_text_col = _get_column_pos_of_word(highlighted_text, str, SEARCH_MATCH_CASE | SEARCH_WHOLE_WORDS, j);
							}

							bool in_highlighted_word = (j >= highlighted_text_col && j < highlighted_text_col + highlighted_text.length());

							/* if this is the original highlighted text we don't want to highlight it again */
							if (cursor.line == line && (cursor.column >= highlighted_text_col && cursor.column <= highlighted_text_col + highlighted_text.length())) {
								in_highlighted_word = false;
							}

							if (in_highlighted_word) {
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2i(char_ofs + char_margin, ofs_y), Size2i(char_w, get_row_height())), cache.word_highlighted_color);
							}
						}
					}

					if (brace_matching_enabled) {
						if ((brace_open_match_line == line && brace_open_match_column == j) ||
								(cursor.column == j && cursor.line == line && (brace_open_matching || brace_open_mismatch))) {

							if (brace_open_mismatch)
								color = cache.brace_mismatch_color;
							cache.font->draw_char(ci, Point2i(char_ofs + char_margin, ofs_y + ascent), '_', str[j + 1], in_selection ? cache.font_selected_color : color);
						}

						if (
								(brace_close_match_line == line && brace_close_match_column == j) ||
								(cursor.column == j + 1 && cursor.line == line && (brace_close_matching || brace_close_mismatch))) {

							if (brace_close_mismatch)
								color = cache.brace_mismatch_color;
							cache.font->draw_char(ci, Point2i(char_ofs + char_margin, ofs_y + ascent), '_', str[j + 1], in_selection ? cache.font_selected_color : color);
						}
					}

					if (cursor.column == j && cursor.line == line) {

						cursor_pos = Point2i(char_ofs + char_margin, ofs_y);

						if (insert_mode) {
							cursor_pos.y += (get_row_height() - 3);
						}

						int caret_w = (str[j] == '\t') ? cache.font->get_char_size(' ').width : char_w;
						if (draw_caret) {
							if (insert_mode) {
								int caret_h = (block_caret) ? 4 : 1;
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, caret_h)), cache.caret_color);
							} else {
								caret_w = (block_caret) ? caret_w : 1;
								VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, get_row_height())), cache.caret_color);
							}
						}
					}

					if (cursor.column == j && cursor.line == line && block_caret && draw_caret && !insert_mode) {
						color = cache.caret_background_color;
					}

					if (str[j] >= 32)
						cache.font->draw_char(ci, Point2i(char_ofs + char_margin, ofs_y + ascent), str[j], str[j + 1], in_selection ? cache.font_selected_color : color);

					else if (draw_tabs && str[j] == '\t') {
						int yofs = (get_row_height() - cache.tab_icon->get_height()) / 2;
						cache.tab_icon->draw(ci, Point2(char_ofs + char_margin, ofs_y + yofs), in_selection ? cache.font_selected_color : color);
					}

					char_ofs += char_w;
				}

				if (cursor.column == str.length() && cursor.line == line && (char_ofs + char_margin) >= xmargin_beg) {

					cursor_pos = Point2i(char_ofs + char_margin, ofs_y);

					if (insert_mode) {
						cursor_pos.y += (get_row_height() - 3);
					}

					if (draw_caret) {
						if (insert_mode) {
							int char_w = cache.font->get_char_size(' ').width;
							int caret_h = (block_caret) ? 4 : 1;
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(char_w, caret_h)), cache.caret_color);
						} else {
							int char_w = cache.font->get_char_size(' ').width;
							int caret_w = (block_caret) ? char_w : 1;
							VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor_pos, Size2i(caret_w, get_row_height())), cache.caret_color);
						}
					}
				}
			}

			if (line_length_guideline) {
				int x = xmargin_beg + cache.font->get_char_size('0').width * line_length_guideline_col - cursor.x_ofs;
				if (x > xmargin_beg && x < xmargin_end) {
					VisualServer::get_singleton()->canvas_item_add_line(ci, Point2(x, 0), Point2(x, cache.size.height), cache.line_length_guideline_color);
				}
			}

			bool completion_below = false;
			if (completion_active) {
				// code completion box
				Ref<StyleBox> csb = get_stylebox("completion");
				int maxlines = get_constant("completion_lines");
				int cmax_width = get_constant("completion_max_width") * cache.font->get_char_size('x').x;
				int scrollw = get_constant("completion_scroll_width");
				Color scrollc = get_color("completion_scroll_color");

				int lines = MIN(completion_options.size(), maxlines);
				int w = 0;
				int h = lines * get_row_height();
				int nofs = cache.font->get_string_size(completion_base).width;

				if (completion_options.size() < 50) {
					for (int i = 0; i < completion_options.size(); i++) {
						int w2 = MIN(cache.font->get_string_size(completion_options[i]).x, cmax_width);
						if (w2 > w)
							w = w2;
					}
				} else {
					w = cmax_width;
				}

				int th = h + csb->get_minimum_size().y;

				if (cursor_pos.y + get_row_height() + th > get_size().height) {
					completion_rect.pos.y = cursor_pos.y - th;
				} else {
					completion_rect.pos.y = cursor_pos.y + get_row_height() + csb->get_offset().y;
					completion_below = true;
				}

				if (cursor_pos.x - nofs + w + scrollw > get_size().width) {
					completion_rect.pos.x = get_size().width - w - scrollw;
				} else {
					completion_rect.pos.x = cursor_pos.x - nofs;
				}

				completion_rect.size.width = w + 2;
				completion_rect.size.height = h;
				if (completion_options.size() <= maxlines)
					scrollw = 0;

				draw_style_box(csb, Rect2(completion_rect.pos - csb->get_offset(), completion_rect.size + csb->get_minimum_size() + Size2(scrollw, 0)));

				if (cache.completion_background_color.a > 0.01) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(completion_rect.pos, completion_rect.size + Size2(scrollw, 0)), cache.completion_background_color);
				}
				int line_from = CLAMP(completion_index - lines / 2, 0, completion_options.size() - lines);
				VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(Point2(completion_rect.pos.x, completion_rect.pos.y + (completion_index - line_from) * get_row_height()), Size2(completion_rect.size.width, get_row_height())), cache.completion_selected_color);
				draw_rect(Rect2(completion_rect.pos, Size2(nofs, completion_rect.size.height)), cache.completion_existing_color);

				for (int i = 0; i < lines; i++) {

					int l = line_from + i;
					ERR_CONTINUE(l < 0 || l >= completion_options.size());
					Color text_color = cache.completion_font_color;
					for (int j = 0; j < color_regions.size(); j++) {
						if (completion_options[l].begins_with(color_regions[j].begin_key)) {
							text_color = color_regions[j].color;
						}
					}
					draw_string(cache.font, Point2(completion_rect.pos.x, completion_rect.pos.y + i * get_row_height() + cache.font->get_ascent()), completion_options[l], text_color, completion_rect.size.width);
				}

				if (scrollw) {
					//draw a small scroll rectangle to show a position in the options
					float r = maxlines / (float)completion_options.size();
					float o = line_from / (float)completion_options.size();
					draw_rect(Rect2(completion_rect.pos.x + completion_rect.size.width, completion_rect.pos.y + o * completion_rect.size.y, scrollw, completion_rect.size.y * r), scrollc);
				}

				completion_line_ofs = line_from;
			}

			// check to see if the hint should be drawn
			bool show_hint = false;
			if (completion_hint != "") {
				if (completion_active) {
					if (completion_below && !callhint_below) {
						show_hint = true;
					} else if (!completion_below && callhint_below) {
						show_hint = true;
					}
				} else {
					show_hint = true;
				}
			}

			if (show_hint) {

				Ref<StyleBox> sb = get_stylebox("panel", "TooltipPanel");
				Ref<Font> font = cache.font;
				Color font_color = get_color("font_color", "TooltipLabel");

				int max_w = 0;
				int sc = completion_hint.get_slice_count("\n");
				int offset = 0;
				int spacing = 0;
				for (int i = 0; i < sc; i++) {

					String l = completion_hint.get_slice("\n", i);
					int len = font->get_string_size(l).x;
					max_w = MAX(len, max_w);
					if (i == 0) {
						offset = font->get_string_size(l.substr(0, l.find(String::chr(0xFFFF)))).x;
					} else {
						spacing += cache.line_spacing;
					}
				}

				Size2 size = Size2(max_w, sc * font->get_height() + spacing);
				Size2 minsize = size + sb->get_minimum_size();

				if (completion_hint_offset == -0xFFFF) {
					completion_hint_offset = cursor_pos.x - offset;
				}

				Point2 hint_ofs = Vector2(completion_hint_offset, cursor_pos.y) + callhint_offset;

				if (callhint_below) {
					hint_ofs.y += get_row_height() + sb->get_offset().y;
				} else {
					hint_ofs.y -= minsize.y + sb->get_offset().y;
				}

				draw_style_box(sb, Rect2(hint_ofs, minsize));

				spacing = 0;
				for (int i = 0; i < sc; i++) {
					int begin = 0;
					int end = 0;
					String l = completion_hint.get_slice("\n", i);

					if (l.find(String::chr(0xFFFF)) != -1) {
						begin = font->get_string_size(l.substr(0, l.find(String::chr(0xFFFF)))).x;
						end = font->get_string_size(l.substr(0, l.rfind(String::chr(0xFFFF)))).x;
					}

					draw_string(font, hint_ofs + sb->get_offset() + Vector2(0, font->get_ascent() + font->get_height() * i + spacing), l.replace(String::chr(0xFFFF), ""), font_color);
					if (end > 0) {
						Vector2 b = hint_ofs + sb->get_offset() + Vector2(begin, font->get_height() + font->get_height() * i + spacing - 1);
						draw_line(b, b + Vector2(end - begin, 0), font_color);
					}
					spacing += cache.line_spacing;
				}
			}

		} break;
		case NOTIFICATION_FOCUS_ENTER: {

			if (!caret_blink_enabled) {
				draw_caret = true;
			}
			if (OS::get_singleton()->has_virtual_keyboard())
				OS::get_singleton()->show_virtual_keyboard(get_text(), get_global_rect());
			if (raised_from_completion) {
				VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 1);
			}

		} break;
		case NOTIFICATION_FOCUS_EXIT: {

			if (OS::get_singleton()->has_virtual_keyboard())
				OS::get_singleton()->hide_virtual_keyboard();
			if (raised_from_completion) {
				VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 0);
			}
		} break;
	}
}

void TextEdit::_consume_pair_symbol(CharType ch) {

	int cursor_position_to_move = cursor_get_column() + 1;

	CharType ch_single[2] = { ch, 0 };
	CharType ch_single_pair[2] = { _get_right_pair_symbol(ch), 0 };
	CharType ch_pair[3] = { ch, _get_right_pair_symbol(ch), 0 };

	if (is_selection_active()) {

		int new_column, new_line;

		begin_complex_operation();
		_insert_text(get_selection_from_line(), get_selection_from_column(),
				ch_single,
				&new_line, &new_column);

		int to_col_offset = 0;
		if (get_selection_from_line() == get_selection_to_line())
			to_col_offset = 1;

		_insert_text(get_selection_to_line(),
				get_selection_to_column() + to_col_offset,
				ch_single_pair,
				&new_line, &new_column);
		end_complex_operation();

		cursor_set_line(get_selection_to_line());
		cursor_set_column(get_selection_to_column() + to_col_offset);

		deselect();
		update();
		return;
	}

	if ((ch == '\'' || ch == '"') &&
			cursor_get_column() > 0 &&
			_is_text_char(text[cursor.line][cursor_get_column() - 1])) {
		insert_text_at_cursor(ch_single);
		cursor_set_column(cursor_position_to_move);
		return;
	}

	if (cursor_get_column() < text[cursor.line].length()) {
		if (_is_text_char(text[cursor.line][cursor_get_column()])) {
			insert_text_at_cursor(ch_single);
			cursor_set_column(cursor_position_to_move);
			return;
		}
		if (_is_pair_right_symbol(ch) &&
				text[cursor.line][cursor_get_column()] == ch) {
			cursor_set_column(cursor_position_to_move);
			return;
		}
	}

	insert_text_at_cursor(ch_pair);
	cursor_set_column(cursor_position_to_move);
	return;
}

void TextEdit::_consume_backspace_for_pair_symbol(int prev_line, int prev_column) {

	bool remove_right_symbol = false;

	if (cursor.column < text[cursor.line].length() && cursor.column > 0) {

		CharType left_char = text[cursor.line][cursor.column - 1];
		CharType right_char = text[cursor.line][cursor.column];

		if (right_char == _get_right_pair_symbol(left_char)) {
			remove_right_symbol = true;
		}
	}
	if (remove_right_symbol) {
		_remove_text(prev_line, prev_column, cursor.line, cursor.column + 1);
	} else {
		_remove_text(prev_line, prev_column, cursor.line, cursor.column);
	}
}

void TextEdit::backspace_at_cursor() {
	if (readonly)
		return;

	if (cursor.column == 0 && cursor.line == 0)
		return;

	int prev_line = cursor.column ? cursor.line : cursor.line - 1;
	int prev_column = cursor.column ? (cursor.column - 1) : (text[cursor.line - 1].length());
	if (auto_brace_completion_enabled &&
			cursor.column > 0 &&
			_is_pair_left_symbol(text[cursor.line][cursor.column - 1])) {
		_consume_backspace_for_pair_symbol(prev_line, prev_column);
	} else {
		_remove_text(prev_line, prev_column, cursor.line, cursor.column);
	}

	cursor_set_line(prev_line);
	cursor_set_column(prev_column);
}

void TextEdit::indent_selection_right() {

	if (!is_selection_active()) {
		return;
	}
	begin_complex_operation();
	int start_line = get_selection_from_line();
	int end_line = get_selection_to_line();

	// ignore if the cursor is not past the first column
	if (get_selection_to_column() == 0) {
		end_line--;
	}

	for (int i = start_line; i <= end_line; i++) {
		String line_text = get_line(i);
		line_text = '\t' + line_text;
		set_line(i, line_text);
	}

	// fix selection being off by one on the last line
	selection.to_column++;
	end_complex_operation();
	update();
}

void TextEdit::indent_selection_left() {

	if (!is_selection_active()) {
		return;
	}
	begin_complex_operation();
	int start_line = get_selection_from_line();
	int end_line = get_selection_to_line();

	// ignore if the cursor is not past the first column
	if (get_selection_to_column() == 0) {
		end_line--;
	}
	String last_line_text = get_line(end_line);

	for (int i = start_line; i <= end_line; i++) {
		String line_text = get_line(i);

		if (line_text.begins_with("\t")) {
			line_text = line_text.substr(1, line_text.length());
			set_line(i, line_text);
		} else if (line_text.begins_with("    ")) {
			line_text = line_text.substr(4, line_text.length());
			set_line(i, line_text);
		}
	}

	// fix selection being off by one on the last line
	if (last_line_text != get_line(end_line) && selection.to_column > 0) {
		selection.to_column--;
	}
	end_complex_operation();
	update();
}

void TextEdit::_get_mouse_pos(const Point2i &p_mouse, int &r_row, int &r_col) const {

	float rows = p_mouse.y;
	rows -= cache.style_normal->get_margin(MARGIN_TOP);
	rows /= get_row_height();
	int row = cursor.line_ofs + rows;

	if (row < 0)
		row = 0;

	int col = 0;

	if (row >= text.size()) {

		row = text.size() - 1;
		col = text[row].size();
	} else {

		col = p_mouse.x - (cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width);
		col += cursor.x_ofs;
		col = get_char_pos_for(col, get_line(row));
	}

	r_row = row;
	r_col = col;
}

void TextEdit::_input_event(const InputEvent &p_input_event) {

	switch (p_input_event.type) {

		case InputEvent::MOUSE_BUTTON: {

			const InputEventMouseButton &mb = p_input_event.mouse_button;

			if (completion_active && completion_rect.has_point(Point2(mb.x, mb.y))) {

				if (!mb.pressed)
					return;

				if (mb.button_index == BUTTON_WHEEL_UP) {
					if (completion_index > 0) {
						completion_index--;
						completion_current = completion_options[completion_index];
						update();
					}
				}
				if (mb.button_index == BUTTON_WHEEL_DOWN) {

					if (completion_index < completion_options.size() - 1) {
						completion_index++;
						completion_current = completion_options[completion_index];
						update();
					}
				}

				if (mb.button_index == BUTTON_LEFT) {

					completion_index = CLAMP(completion_line_ofs + (mb.y - completion_rect.pos.y) / get_row_height(), 0, completion_options.size() - 1);

					completion_current = completion_options[completion_index];
					update();
					if (mb.doubleclick)
						_confirm_completion();
				}
				return;
			} else {
				_cancel_completion();
				_cancel_code_hint();
			}

			if (mb.pressed) {

				if (mb.button_index == BUTTON_WHEEL_UP && !mb.mod.command) {
					v_scroll->set_val(v_scroll->get_val() - (3 * mb.factor));
				}
				if (mb.button_index == BUTTON_WHEEL_DOWN && !mb.mod.command) {
					v_scroll->set_val(v_scroll->get_val() + (3 * mb.factor));
				}
				if (mb.button_index == BUTTON_WHEEL_LEFT) {
					h_scroll->set_val(h_scroll->get_val() - (100 * mb.factor));
				}
				if (mb.button_index == BUTTON_WHEEL_RIGHT) {
					h_scroll->set_val(h_scroll->get_val() + (100 * mb.factor));
				}
				if (mb.button_index == BUTTON_LEFT) {

					_reset_caret_blink_timer();

					int row, col;
					_get_mouse_pos(Point2i(mb.x, mb.y), row, col);

					// toggle breakpoint on gutter click
					if (draw_breakpoint_gutter) {
						int gutter = cache.style_normal->get_margin(MARGIN_LEFT);
						if (mb.x > gutter && mb.x <= gutter + cache.breakpoint_gutter_width + 3) {
							set_line_as_breakpoint(row, !is_line_set_as_breakpoint(row));
							emit_signal("breakpoint_toggled", row);
							return;
						}
					}

					int prev_col = cursor.column;
					int prev_line = cursor.line;

					cursor_set_line(row);
					cursor_set_column(col);

					if (mb.mod.shift && (cursor.column != prev_col || cursor.line != prev_line)) {

						if (!selection.active) {
							selection.active = true;
							selection.selecting_mode = Selection::MODE_POINTER;
							selection.from_column = prev_col;
							selection.from_line = prev_line;
							selection.to_column = cursor.column;
							selection.to_line = cursor.line;

							if (selection.from_line > selection.to_line || (selection.from_line == selection.to_line && selection.from_column > selection.to_column)) {
								SWAP(selection.from_column, selection.to_column);
								SWAP(selection.from_line, selection.to_line);
								selection.shiftclick_left = false;
							} else {
								selection.shiftclick_left = true;
							}
							selection.selecting_line = prev_line;
							selection.selecting_column = prev_col;
							update();
						} else {

							if (cursor.line < selection.selecting_line || (cursor.line == selection.selecting_line && cursor.column < selection.selecting_column)) {

								if (selection.shiftclick_left) {
									SWAP(selection.from_column, selection.to_column);
									SWAP(selection.from_line, selection.to_line);
									selection.shiftclick_left = !selection.shiftclick_left;
								}
								selection.from_column = cursor.column;
								selection.from_line = cursor.line;

							} else if (cursor.line > selection.selecting_line || (cursor.line == selection.selecting_line && cursor.column > selection.selecting_column)) {

								if (!selection.shiftclick_left) {
									SWAP(selection.from_column, selection.to_column);
									SWAP(selection.from_line, selection.to_line);
									selection.shiftclick_left = !selection.shiftclick_left;
								}
								selection.to_column = cursor.column;
								selection.to_line = cursor.line;

							} else {
								selection.active = false;
							}

							update();
						}

					} else {

						//if sel active and dblick last time < something

						//else
						selection.active = false;
						selection.selecting_mode = Selection::MODE_POINTER;
						selection.selecting_line = row;
						selection.selecting_column = col;
					}

					if (!mb.doubleclick && (OS::get_singleton()->get_ticks_msec() - last_dblclk) < 600 && cursor.line == prev_line) {
						//tripleclick select line
						select(cursor.line, 0, cursor.line, text[cursor.line].length());
						selection.selecting_column = 0;
						last_dblclk = 0;

					} else if (mb.doubleclick && text[cursor.line].length()) {

						//doubleclick select world
						String s = text[cursor.line];
						int beg = CLAMP(cursor.column, 0, s.length());
						int end = beg;

						if (s[beg] > 32 || beg == s.length()) {

							bool symbol = beg < s.length() && _is_symbol(s[beg]); //not sure if right but most editors behave like this

							while (beg > 0 && s[beg - 1] > 32 && (symbol == _is_symbol(s[beg - 1]))) {
								beg--;
							}
							while (end < s.length() && s[end + 1] > 32 && (symbol == _is_symbol(s[end + 1]))) {
								end++;
							}

							if (end < s.length())
								end += 1;

							select(cursor.line, beg, cursor.line, end);

							selection.selecting_column = beg;
						}

						last_dblclk = OS::get_singleton()->get_ticks_msec();
					}

					update();
				}

				if (mb.button_index == BUTTON_RIGHT) {

					menu->set_pos(get_global_transform().xform(get_local_mouse_pos()));
					menu->set_size(Vector2(1, 1));
					menu->popup();
					grab_focus();
				}
			} else {

				if (mb.button_index == BUTTON_LEFT)
					click_select_held->stop();

				// notify to show soft keyboard
				notification(NOTIFICATION_FOCUS_ENTER);
			}

		} break;
		case InputEvent::MOUSE_MOTION: {

			const InputEventMouseMotion &mm = p_input_event.mouse_motion;

			if (mm.button_mask & BUTTON_MASK_LEFT) {

				if (selection.selecting_mode != Selection::MODE_NONE) {

					_reset_caret_blink_timer();

					int row, col;
					_get_mouse_pos(Point2i(mm.x, mm.y), row, col);

					select(selection.selecting_line, selection.selecting_column, row, col);

					cursor_set_line(row);
					cursor_set_column(col);
					update();

					click_select_held->start();
				}
			}

		} break;

		case InputEvent::KEY: {

			InputEventKey k = p_input_event.key;

			if (!k.pressed)
				return;

			if (completion_active) {
				if (readonly)
					break;

				bool valid = true;
				if (k.mod.command || k.mod.meta)
					valid = false;

				if (valid) {

					if (!k.mod.alt) {
						if (k.scancode == KEY_UP) {

							if (completion_index > 0) {
								completion_index--;
								completion_current = completion_options[completion_index];
								update();
							}
							accept_event();
							return;
						}

						if (k.scancode == KEY_DOWN) {

							if (completion_index < completion_options.size() - 1) {
								completion_index++;
								completion_current = completion_options[completion_index];
								update();
							}
							accept_event();
							return;
						}

						if (k.scancode == KEY_PAGEUP) {

							completion_index -= get_constant("completion_lines");
							if (completion_index < 0)
								completion_index = 0;
							completion_current = completion_options[completion_index];
							update();
							accept_event();
							return;
						}

						if (k.scancode == KEY_PAGEDOWN) {

							completion_index += get_constant("completion_lines");
							if (completion_index >= completion_options.size())
								completion_index = completion_options.size() - 1;
							completion_current = completion_options[completion_index];
							update();
							accept_event();
							return;
						}

						if (k.scancode == KEY_HOME && completion_index > 0) {

							completion_index = 0;
							completion_current = completion_options[completion_index];
							update();
							accept_event();
							return;
						}

						if (k.scancode == KEY_END && completion_index < completion_options.size() - 1) {

							completion_index = completion_options.size() - 1;
							completion_current = completion_options[completion_index];
							update();
							accept_event();
							return;
						}

						if (k.scancode == KEY_DOWN) {

							if (completion_index < completion_options.size() - 1) {
								completion_index++;
								completion_current = completion_options[completion_index];
								update();
							}
							accept_event();
							return;
						}

						if (k.scancode == KEY_ENTER || k.scancode == KEY_RETURN || k.scancode == KEY_TAB) {

							_confirm_completion();
							accept_event();
							return;
						}

						if (k.scancode == KEY_BACKSPACE) {

							_reset_caret_blink_timer();

							backspace_at_cursor();
							_update_completion_candidates();
							accept_event();
							return;
						}

						if (k.scancode == KEY_SHIFT) {
							accept_event();
							return;
						}
					}

					if (k.unicode > 32) {

						_reset_caret_blink_timer();

						const CharType chr[2] = { (CharType)k.unicode, 0 };
						if (auto_brace_completion_enabled && _is_pair_symbol(chr[0])) {
							_consume_pair_symbol(chr[0]);
						} else {

							// remove the old character if in insert mode
							if (insert_mode) {
								begin_complex_operation();

								// make sure we don't try and remove empty space
								if (cursor.column < get_line(cursor.line).length()) {
									_remove_text(cursor.line, cursor.column, cursor.line, cursor.column + 1);
								}
							}

							_insert_text_at_cursor(chr);

							if (insert_mode) {
								end_complex_operation();
							}
						}
						_update_completion_candidates();
						accept_event();

						return;
					}
				}

				_cancel_completion();
			}

			/* TEST CONTROL FIRST!! */

			// some remaps for duplicate functions..
			if (k.mod.command && !k.mod.shift && !k.mod.alt && !k.mod.meta && k.scancode == KEY_INSERT) {

				k.scancode = KEY_C;
			}
			if (!k.mod.command && k.mod.shift && !k.mod.alt && !k.mod.meta && k.scancode == KEY_INSERT) {

				k.scancode = KEY_V;
				k.mod.command = true;
				k.mod.shift = false;
			}

			if (!k.mod.command) {
				_reset_caret_blink_timer();
			}
			// save here for insert mode, just in case it is cleared in the following section
			bool had_selection = selection.active;

			// stuff to do when selection is active..
			if (selection.active) {

				if (readonly)
					break;

				bool clear = false;
				bool unselect = false;
				bool dobreak = false;

				switch (k.scancode) {

					case KEY_TAB: {
						if (k.mod.shift) {
							indent_selection_left();
						} else {
							indent_selection_right();
						}
						dobreak = true;
						accept_event();
					} break;
					case KEY_X:
					case KEY_C:
						//special keys often used with control, wait...
						clear = (!k.mod.command || k.mod.shift || k.mod.alt);
						break;
					case KEY_DELETE:
						if (!k.mod.shift) {
							accept_event();
							clear = true;
							dobreak = true;
						} else if (k.mod.command || k.mod.alt) {
							dobreak = true;
						}
						break;
					case KEY_BACKSPACE:
						accept_event();
						clear = true;
						dobreak = true;
						break;
					case KEY_LEFT:
					case KEY_RIGHT:
					case KEY_UP:
					case KEY_DOWN:
					case KEY_PAGEUP:
					case KEY_PAGEDOWN:
					case KEY_HOME:
					case KEY_END:
						// ignore arrows if any modifiers are held (shift = selecting, others may be used for editor hotkeys)
						if (k.mod.command || k.mod.shift || k.mod.alt)
							break;
						unselect = true;
						break;

					default:
						if (k.unicode >= 32 && !k.mod.command && !k.mod.alt && !k.mod.meta)
							clear = true;
						if (auto_brace_completion_enabled && _is_pair_left_symbol(k.unicode))
							clear = false;
				}

				if (unselect) {
					selection.active = false;
					selection.selecting_mode = Selection::MODE_NONE;
					update();
				}
				if (clear) {

					if (!dobreak) {
						begin_complex_operation();
					}
					selection.active = false;
					update();
					_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
					cursor_set_line(selection.from_line);
					cursor_set_column(selection.from_column);
					update();
				}
				if (dobreak)
					break;
			}

			selection.selecting_text = false;

			bool scancode_handled = true;

			// special scancode test...

			switch (k.scancode) {

				case KEY_ENTER:
				case KEY_RETURN: {

					if (readonly)
						break;

					String ins = "\n";

					//keep indentation
					for (int i = 0; i < text[cursor.line].length(); i++) {
						if (text[cursor.line][i] == '\t')
							ins += "\t";
						else
							break;
					}
					if (auto_indent) {
						// indent once again if previous line will end with ':'
						// (i.e. colon precedes current cursor position)
						if (cursor.column > 0 && text[cursor.line][cursor.column - 1] == ':') {
							ins += "\t";
						}
					}

					bool first_line = false;
					if (k.mod.command) {
						if (k.mod.shift) {
							if (cursor.line > 0) {
								cursor_set_line(cursor.line - 1);
								cursor_set_column(text[cursor.line].length());
							} else {
								cursor_set_column(0);
								first_line = true;
							}
						} else {
							cursor_set_column(text[cursor.line].length());
						}
					}

					_insert_text_at_cursor(ins);
					_push_current_op();

					if (first_line) {
						cursor_set_line(0);
					}

				} break;
				case KEY_ESCAPE: {
					if (completion_hint != "") {
						completion_hint = "";
						update();
					} else {
						scancode_handled = false;
					}
				} break;
				case KEY_TAB: {
					if (k.mod.command) break; // avoid tab when command

					if (readonly)
						break;

					if (selection.active) {

					} else {
						if (k.mod.shift) {

							int cc = cursor.column;
							if (cc > 0 && cc <= text[cursor.line].length() && text[cursor.line][cursor.column - 1] == '\t') {
								//simple unindent

								backspace_at_cursor();
							}
						} else {
							//simple indent
							_insert_text_at_cursor("\t");
						}
					}

				} break;
				case KEY_BACKSPACE: {
					if (readonly)
						break;

#ifdef APPLE_STYLE_KEYS
					if (k.mod.alt) {
#else
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					} else if (k.mod.command) {
#endif
						int line = cursor.line;
						int column = cursor.column;

						bool prev_char = false;
						bool only_whitespace = true;

						while (only_whitespace && line > 0) {

							while (column > 0) {
								CharType c = text[line][column - 1];

								if (c != '\t' && c != ' ') {
									only_whitespace = false;
									break;
								}

								column--;
							}

							if (only_whitespace) {
								line--;
								column = text[line].length();
							}
						}

						while (column > 0) {
							bool ischar = _is_text_char(text[line][column - 1]);

							if (prev_char && !ischar)
								break;

							prev_char = ischar;
							column--;
						}

						_remove_text(line, column, cursor.line, cursor.column);

						cursor_set_line(line);
						cursor_set_column(column);

					} else {
						backspace_at_cursor();
					}

				} break;
				case KEY_KP_4: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_left
				}
				case KEY_LEFT: {

					if (k.mod.shift)
						_pre_shift_selection();
#ifdef APPLE_STYLE_KEYS
					else
#else
					else if (!k.mod.alt)
#endif
						deselect();

#ifdef APPLE_STYLE_KEYS
					if (k.mod.command) {
						cursor_set_column(0);
					} else if (k.mod.alt) {

#else
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					} else if (k.mod.command) {
#endif
						bool prev_char = false;
						int cc = cursor.column;

						if (cc == 0 && cursor.line > 0) {
							cursor_set_line(cursor.line - 1);
							cursor_set_column(text[cursor.line].length());
							break;
						}

						while (cc > 0) {

							bool ischar = _is_text_char(text[cursor.line][cc - 1]);

							if (prev_char && !ischar)
								break;

							prev_char = ischar;
							cc--;
						}

						cursor_set_column(cc);

					} else if (cursor.column == 0) {

						if (cursor.line > 0) {
							cursor_set_line(cursor.line - 1);
							cursor_set_column(text[cursor.line].length());
						}
					} else {
						cursor_set_column(cursor_get_column() - 1);
					}

					if (k.mod.shift)
						_post_shift_selection();

				} break;
				case KEY_KP_6: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_right
				}
				case KEY_RIGHT: {

					if (k.mod.shift)
						_pre_shift_selection();
#ifdef APPLE_STYLE_KEYS
					else
#else
					else if (!k.mod.alt)
#endif
						deselect();

#ifdef APPLE_STYLE_KEYS
					if (k.mod.command) {
						cursor_set_column(text[cursor.line].length());
					} else if (k.mod.alt) {
#else
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					} else if (k.mod.command) {
#endif
						bool prev_char = false;
						int cc = cursor.column;

						if (cc == text[cursor.line].length() && cursor.line < text.size() - 1) {
							cursor_set_line(cursor.line + 1);
							cursor_set_column(0);
							break;
						}

						while (cc < text[cursor.line].length()) {

							bool ischar = _is_text_char(text[cursor.line][cc]);

							if (prev_char && !ischar)
								break;
							prev_char = ischar;
							cc++;
						}

						cursor_set_column(cc);

					} else if (cursor.column == text[cursor.line].length()) {

						if (cursor.line < text.size() - 1) {
							cursor_set_line(cursor.line + 1);
							cursor_set_column(0);
						}
					} else {
						cursor_set_column(cursor_get_column() + 1);
					}

					if (k.mod.shift)
						_post_shift_selection();

				} break;
				case KEY_KP_8: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_up
				}
				case KEY_UP: {

					if (k.mod.shift)
						_pre_shift_selection();
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					}
#ifndef APPLE_STYLE_KEYS
					if (k.mod.command) {
						_scroll_lines_up();
						break;
					}
#else
					if (k.mod.command && k.mod.alt) {
						_scroll_lines_up();
						break;
					}

					if (k.mod.command)
						cursor_set_line(0);
					else
#endif
					cursor_set_line(cursor_get_line() - 1);

					if (k.mod.shift)
						_post_shift_selection();
					_cancel_code_hint();

				} break;
				case KEY_KP_2: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_down
				}
				case KEY_DOWN: {

					if (k.mod.shift)
						_pre_shift_selection();
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					}
#ifndef APPLE_STYLE_KEYS
					if (k.mod.command) {
						_scroll_lines_down();
						break;
					}
#else
					if (k.mod.command && k.mod.alt) {
						_scroll_lines_down();
						break;
					}

					if (k.mod.command)
						cursor_set_line(text.size() - 1);
					else
#endif
					cursor_set_line(cursor_get_line() + 1);

					if (k.mod.shift)
						_post_shift_selection();
					_cancel_code_hint();

				} break;

				case KEY_DELETE: {

					if (readonly)
						break;

					if (k.mod.shift && !k.mod.command && !k.mod.alt) {
						cut();
						break;
					}

					int curline_len = text[cursor.line].length();

					if (cursor.line == text.size() - 1 && cursor.column == curline_len)
						break; //nothing to do

					int next_line = cursor.column < curline_len ? cursor.line : cursor.line + 1;
					int next_column;

#ifdef APPLE_STYLE_KEYS
					if (k.mod.alt) {
#else
					if (k.mod.alt) {
						scancode_handled = false;
						break;
					} else if (k.mod.command) {
#endif
						int last_line = text.size() - 1;

						int line = cursor.line;
						int column = cursor.column;

						bool prev_char = false;
						bool only_whitespace = true;

						while (only_whitespace && line < last_line) {

							while (column < text[line].length()) {
								CharType c = text[line][column];

								if (c != '\t' && c != ' ') {
									only_whitespace = false;
									break;
								}

								column++;
							}

							if (only_whitespace) {
								line++;
								column = 0;
							}
						}

						while (column < text[line].length()) {

							bool ischar = _is_text_char(text[line][column]);

							if (prev_char && !ischar)
								break;
							prev_char = ischar;
							column++;
						}

						next_line = line;
						next_column = column;
					} else {
						next_column = cursor.column < curline_len ? (cursor.column + 1) : 0;
					}

					_remove_text(cursor.line, cursor.column, next_line, next_column);
					update();

				} break;
				case KEY_KP_7: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_home
				}
#ifdef APPLE_STYLE_KEYS
				case KEY_HOME: {

					if (k.mod.shift)
						_pre_shift_selection();

					cursor_set_line(0);

					if (k.mod.shift)
						_post_shift_selection();
					else if (k.mod.command || k.mod.control)
						deselect();

				} break;
#else
				case KEY_HOME: {

					if (k.mod.shift)
						_pre_shift_selection();

					if (k.mod.command) {
						cursor_set_line(0);
						cursor_set_column(0);
					} else {
						// compute whitespace symbols seq length
						int current_line_whitespace_len = 0;
						while (current_line_whitespace_len < text[cursor.line].length()) {
							CharType c = text[cursor.line][current_line_whitespace_len];
							if (c != '\t' && c != ' ')
								break;
							current_line_whitespace_len++;
						}

						if (cursor_get_column() == current_line_whitespace_len)
							cursor_set_column(0);
						else
							cursor_set_column(current_line_whitespace_len);
					}

					if (k.mod.shift)
						_post_shift_selection();
					else if (k.mod.command || k.mod.control)
						deselect();
					_cancel_completion();
					completion_hint = "";

				} break;
#endif
				case KEY_KP_1: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_end
				}
#ifdef APPLE_STYLE_KEYS
				case KEY_END: {

					if (k.mod.shift)
						_pre_shift_selection();

					cursor_set_line(text.size() - 1);

					if (k.mod.shift)
						_post_shift_selection();
					else if (k.mod.command || k.mod.control)
						deselect();

				} break;
#else
				case KEY_END: {

					if (k.mod.shift)
						_pre_shift_selection();

					if (k.mod.command)
						cursor_set_line(text.size() - 1);
					cursor_set_column(text[cursor.line].length());

					if (k.mod.shift)
						_post_shift_selection();
					else if (k.mod.command || k.mod.control)
						deselect();

					_cancel_completion();
					completion_hint = "";

				} break;
#endif
				case KEY_KP_9: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_pageup
				}
				case KEY_PAGEUP: {

					if (k.mod.shift)
						_pre_shift_selection();

					cursor_set_line(cursor_get_line() - get_visible_rows());

					if (k.mod.shift)
						_post_shift_selection();

					_cancel_completion();
					completion_hint = "";

				} break;
				case KEY_KP_3: {
					if (k.unicode != 0) {
						scancode_handled = false;
						break;
					}
					// numlock disabled. fallthrough to key_pageup
				}
				case KEY_PAGEDOWN: {

					if (k.mod.shift)
						_pre_shift_selection();

					cursor_set_line(cursor_get_line() + get_visible_rows());

					if (k.mod.shift)
						_post_shift_selection();

					_cancel_completion();
					completion_hint = "";

				} break;
				case KEY_A: {

					if (!k.mod.command || k.mod.shift || k.mod.alt) {
						scancode_handled = false;
						break;
					}

					select_all();

				} break;
				case KEY_X: {
					if (readonly) {
						break;
					}
					if (!k.mod.command || k.mod.shift || k.mod.alt) {
						scancode_handled = false;
						break;
					}

					cut();

				} break;
				case KEY_C: {

					if (!k.mod.command || k.mod.shift || k.mod.alt) {
						scancode_handled = false;
						break;
					}

					copy();

				} break;
				case KEY_Z: {

					if (!k.mod.command) {
						scancode_handled = false;
						break;
					}

					if (k.mod.shift)
						redo();
					else
						undo();
				} break;
				case KEY_V: {
					if (readonly) {
						break;
					}
					if (!k.mod.command || k.mod.shift || k.mod.alt) {
						scancode_handled = false;
						break;
					}

					paste();

				} break;
				case KEY_SPACE: {
#ifdef OSX_ENABLED
					if (completion_enabled && k.mod.meta) { //cmd-space is spotlight shortcut in OSX
#else
					if (completion_enabled && k.mod.command) {
#endif

						query_code_comple();
						scancode_handled = true;
					} else {
						scancode_handled = false;
					}

				} break;

				case KEY_U: {
					if (!k.mod.command || k.mod.shift) {
						scancode_handled = false;
						break;
					} else {
						if (selection.active) {
							int ini = selection.from_line;
							int end = selection.to_line;
							for (int i = ini; i <= end; i++) {
								if (text[i][0] == '#')
									_remove_text(i, 0, i, 1);
							}
						} else {
							if (text[cursor.line][0] == '#')
								_remove_text(cursor.line, 0, cursor.line, 1);
						}
						update();
					}
					break;
				}

				default: {

					scancode_handled = false;
				} break;
			}

			if (scancode_handled)
				accept_event();
			/*
	    if (!scancode_handled && !k.mod.command && !k.mod.alt) {

		if (k.unicode>=32) {

		    if (readonly)
			break;

		    accept_event();
		} else {

		    break;
		}
	    }
*/
			if (k.scancode == KEY_INSERT) {
				set_insert_mode(!insert_mode);
				accept_event();
				return;
			}

			if (!scancode_handled && !k.mod.command) { //for german kbds

				if (k.unicode >= 32) {

					if (readonly)
						break;

					// remove the old character if in insert mode and no selection
					if (insert_mode && !had_selection) {
						begin_complex_operation();

						// make sure we don't try and remove empty space
						if (cursor.column < get_line(cursor.line).length()) {
							_remove_text(cursor.line, cursor.column, cursor.line, cursor.column + 1);
						}
					}

					const CharType chr[2] = { (CharType)k.unicode, 0 };

					if (completion_hint != "" && k.unicode == ')') {
						completion_hint = "";
					}
					if (auto_brace_completion_enabled && _is_pair_symbol(chr[0])) {
						_consume_pair_symbol(chr[0]);
					} else {
						_insert_text_at_cursor(chr);
					}

					if (insert_mode && !had_selection) {
						end_complex_operation();
					}

					if (selection.active != had_selection) {
						end_complex_operation();
					}
					accept_event();
				} else {

					break;
				}
			}

			return;
		} break;
	}
}

void TextEdit::_pre_shift_selection() {

	if (!selection.active || selection.selecting_mode == Selection::MODE_NONE) {

		selection.selecting_line = cursor.line;
		selection.selecting_column = cursor.column;
		selection.active = true;
	}

	selection.selecting_mode = Selection::MODE_SHIFT;
}

void TextEdit::_post_shift_selection() {

	if (selection.active && selection.selecting_mode == Selection::MODE_SHIFT) {

		select(selection.selecting_line, selection.selecting_column, cursor.line, cursor.column);
		update();
	}

	selection.selecting_text = true;
}

void TextEdit::_scroll_lines_up() {
	// adjust the vertical scroll
	if (get_v_scroll() > 0) {
		set_v_scroll(get_v_scroll() - 1);
	}

	// adjust the cursor
	if (cursor_get_line() >= (get_visible_rows() + get_v_scroll()) && !selection.active) {
		cursor_set_line((get_visible_rows() + get_v_scroll()) - 1, false);
	}
}

void TextEdit::_scroll_lines_down() {
	// calculate the maximum vertical scroll position
	int max_v_scroll = get_line_count() - 1;
	if (!scroll_past_end_of_file_enabled) {
		max_v_scroll -= get_visible_rows() - 1;
	}

	// adjust the vertical scroll
	if (get_v_scroll() < max_v_scroll) {
		set_v_scroll(get_v_scroll() + 1);
	}

	// adjust the cursor
	if ((cursor_get_line()) <= get_v_scroll() - 1 && !selection.active) {
		cursor_set_line(get_v_scroll(), false);
	}
}

/**** TEXT EDIT CORE API ****/

void TextEdit::_base_insert_text(int p_line, int p_char, const String &p_text, int &r_end_line, int &r_end_column) {

	//save for undo...
	ERR_FAIL_INDEX(p_line, text.size());
	ERR_FAIL_COND(p_char < 0);

	/* STEP 1 add spaces if the char is greater than the end of the line */
	while (p_char > text[p_line].length()) {

		text.set(p_line, text[p_line] + String::chr(' '));
	}

	/* STEP 2 separate dest string in pre and post text */

	String preinsert_text = text[p_line].substr(0, p_char);
	String postinsert_text = text[p_line].substr(p_char, text[p_line].size());

	/* STEP 3 remove \r from source text and separate in substrings */

	//buh bye \r and split
	Vector<String> substrings = p_text.replace("\r", "").split("\n");

	for (int i = 0; i < substrings.size(); i++) {
		//insert the substrings

		if (i == 0) {

			text.set(p_line, preinsert_text + substrings[i]);
		} else {

			text.insert(p_line + i, substrings[i]);
		}

		if (i == substrings.size() - 1) {

			text.set(p_line + i, text[p_line + i] + postinsert_text);
		}
	}

	r_end_line = p_line + substrings.size() - 1;
	r_end_column = text[r_end_line].length() - postinsert_text.length();

	if (!text_changed_dirty && !setting_text) {
		if (is_inside_tree())
			MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");
		text_changed_dirty = true;
	}
}

String TextEdit::_base_get_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) const {

	ERR_FAIL_INDEX_V(p_from_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, String());
	ERR_FAIL_INDEX_V(p_to_line, text.size(), String());
	ERR_FAIL_INDEX_V(p_to_column, text[p_to_line].length() + 1, String());
	ERR_FAIL_COND_V(p_to_line < p_from_line, String()); // from > to
	ERR_FAIL_COND_V(p_to_line == p_from_line && p_to_column < p_from_column, String()); // from > to

	String ret;

	for (int i = p_from_line; i <= p_to_line; i++) {

		int begin = (i == p_from_line) ? p_from_column : 0;
		int end = (i == p_to_line) ? p_to_column : text[i].length();

		if (i > p_from_line)
			ret += "\n";
		ret += text[i].substr(begin, end - begin);
	}

	return ret;
}

void TextEdit::_base_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

	ERR_FAIL_INDEX(p_from_line, text.size());
	ERR_FAIL_INDEX(p_from_column, text[p_from_line].length() + 1);
	ERR_FAIL_INDEX(p_to_line, text.size());
	ERR_FAIL_INDEX(p_to_column, text[p_to_line].length() + 1);
	ERR_FAIL_COND(p_to_line < p_from_line); // from > to
	ERR_FAIL_COND(p_to_line == p_from_line && p_to_column < p_from_column); // from > to

	String pre_text = text[p_from_line].substr(0, p_from_column);
	String post_text = text[p_to_line].substr(p_to_column, text[p_to_line].length());

	for (int i = p_from_line; i < p_to_line; i++) {

		text.remove(p_from_line + 1);
	}

	text.set(p_from_line, pre_text + post_text);

	if (!text_changed_dirty && !setting_text) {
		if (is_inside_tree())
			MessageQueue::get_singleton()->push_call(this, "_text_changed_emit");
		text_changed_dirty = true;
	}
}

void TextEdit::_insert_text(int p_line, int p_char, const String &p_text, int *r_end_line, int *r_end_column) {

	if (!setting_text)
		idle_detect->start();

	if (undo_enabled) {
		_clear_redo();
	}

	int retline, retchar;
	_base_insert_text(p_line, p_char, p_text, retline, retchar);
	if (r_end_line)
		*r_end_line = retline;
	if (r_end_column)
		*r_end_column = retchar;

	if (!undo_enabled)
		return;

	/* UNDO!! */
	TextOperation op;
	op.type = TextOperation::TYPE_INSERT;
	op.from_line = p_line;
	op.from_column = p_char;
	op.to_line = retline;
	op.to_column = retchar;
	op.text = p_text;
	op.version = ++version;
	op.chain_forward = false;
	op.chain_backward = false;

	//see if it should just be set as current op
	if (current_op.type != op.type) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;

		return; //set as current op, return
	}
	//see if it can be merged
	if (current_op.to_line != p_line || current_op.to_column != p_char) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;
		return; //set as current op, return
	}
	//merge current op

	current_op.text += p_text;
	current_op.to_column = retchar;
	current_op.to_line = retline;
	current_op.version = op.version;
}

void TextEdit::_remove_text(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

	if (!setting_text)
		idle_detect->start();

	String text;
	if (undo_enabled) {
		_clear_redo();
		text = _base_get_text(p_from_line, p_from_column, p_to_line, p_to_column);
	}

	_base_remove_text(p_from_line, p_from_column, p_to_line, p_to_column);

	if (!undo_enabled)
		return;

	/* UNDO!! */
	TextOperation op;
	op.type = TextOperation::TYPE_REMOVE;
	op.from_line = p_from_line;
	op.from_column = p_from_column;
	op.to_line = p_to_line;
	op.to_column = p_to_column;
	op.text = text;
	op.version = ++version;
	op.chain_forward = false;
	op.chain_backward = false;

	//see if it should just be set as current op
	if (current_op.type != op.type) {
		op.prev_version = get_version();
		_push_current_op();
		current_op = op;
		return; //set as current op, return
	}
	//see if it can be merged
	if (current_op.from_line == p_to_line && current_op.from_column == p_to_column) {
		//basckace or similar
		current_op.text = text + current_op.text;
		current_op.from_line = p_from_line;
		current_op.from_column = p_from_column;
		return; //update current op
	}

	op.prev_version = get_version();
	_push_current_op();
	current_op = op;
}

void TextEdit::_insert_text_at_cursor(const String &p_text) {

	int new_column, new_line;
	_insert_text(cursor.line, cursor.column, p_text, &new_line, &new_column);
	cursor_set_line(new_line);
	cursor_set_column(new_column);

	update();
}

int TextEdit::get_char_count() {

	int totalsize = 0;

	for (int i = 0; i < text.size(); i++) {

		if (i > 0)
			totalsize++; // incliude \n
		totalsize += text[i].length();
	}

	return totalsize; // omit last \n
}

Size2 TextEdit::get_minimum_size() const {

	return cache.style_normal->get_minimum_size();
}
int TextEdit::get_visible_rows() const {

	int total = cache.size.height;
	total -= cache.style_normal->get_minimum_size().height;
	total /= get_row_height();
	return total;
}
void TextEdit::adjust_viewport_to_cursor() {

	if (cursor.line_ofs > cursor.line)
		cursor.line_ofs = cursor.line;

	int visible_width = cache.size.width - cache.style_normal->get_minimum_size().width - cache.line_number_w - cache.breakpoint_gutter_width;
	if (v_scroll->is_visible())
		visible_width -= v_scroll->get_combined_minimum_size().width;
	visible_width -= 20; // give it a little more space

	//printf("rowofs %i, visrows %i, cursor.line %i\n",cursor.line_ofs,get_visible_rows(),cursor.line);

	int visible_rows = get_visible_rows();
	if (h_scroll->is_visible())
		visible_rows -= ((h_scroll->get_combined_minimum_size().height - 1) / get_row_height());

	if (cursor.line >= (cursor.line_ofs + visible_rows))
		cursor.line_ofs = cursor.line - visible_rows + 1;
	if (cursor.line < cursor.line_ofs)
		cursor.line_ofs = cursor.line;

	int cursor_x = get_column_x_offset(cursor.column, text[cursor.line]);

	if (cursor_x > (cursor.x_ofs + visible_width))
		cursor.x_ofs = cursor_x - visible_width + 1;

	if (cursor_x < cursor.x_ofs)
		cursor.x_ofs = cursor_x;

	update();
	/*
    get_range()->set_max(text.size());

    get_range()->set_page(get_visible_rows());

    get_range()->set((int)cursor.line_ofs);
*/
}

void TextEdit::center_viewport_to_cursor() {

	if (cursor.line_ofs > cursor.line)
		cursor.line_ofs = cursor.line;

	int visible_width = cache.size.width - cache.style_normal->get_minimum_size().width - cache.line_number_w - cache.breakpoint_gutter_width;
	if (v_scroll->is_visible())
		visible_width -= v_scroll->get_combined_minimum_size().width;
	visible_width -= 20; // give it a little more space

	int visible_rows = get_visible_rows();
	if (h_scroll->is_visible())
		visible_rows -= ((h_scroll->get_combined_minimum_size().height - 1) / get_row_height());

	int max_ofs = text.size() - (scroll_past_end_of_file_enabled ? 1 : visible_rows);
	cursor.line_ofs = CLAMP(cursor.line - (visible_rows / 2), 0, max_ofs);

	int cursor_x = get_column_x_offset(cursor.column, text[cursor.line]);

	if (cursor_x > (cursor.x_ofs + visible_width))
		cursor.x_ofs = cursor_x - visible_width + 1;

	if (cursor_x < cursor.x_ofs)
		cursor.x_ofs = cursor_x;

	update();
}

void TextEdit::cursor_set_column(int p_col, bool p_adjust_viewport) {

	if (p_col < 0)
		p_col = 0;

	cursor.column = p_col;
	if (cursor.column > get_line(cursor.line).length())
		cursor.column = get_line(cursor.line).length();

	cursor.last_fit_x = get_column_x_offset(cursor.column, get_line(cursor.line));

	if (p_adjust_viewport)
		adjust_viewport_to_cursor();

	if (!cursor_changed_dirty) {
		if (is_inside_tree())
			MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
		cursor_changed_dirty = true;
	}
}

void TextEdit::cursor_set_line(int p_row, bool p_adjust_viewport) {

	if (setting_row)
		return;

	setting_row = true;
	if (p_row < 0)
		p_row = 0;

	if (p_row >= (int)text.size())
		p_row = (int)text.size() - 1;

	cursor.line = p_row;
	cursor.column = get_char_pos_for(cursor.last_fit_x, get_line(cursor.line));

	if (p_adjust_viewport)
		adjust_viewport_to_cursor();

	setting_row = false;

	if (!cursor_changed_dirty) {
		if (is_inside_tree())
			MessageQueue::get_singleton()->push_call(this, "_cursor_changed_emit");
		cursor_changed_dirty = true;
	}
}

int TextEdit::cursor_get_column() const {

	return cursor.column;
}

int TextEdit::cursor_get_line() const {

	return cursor.line;
}

bool TextEdit::cursor_get_blink_enabled() const {
	return caret_blink_enabled;
}

void TextEdit::cursor_set_blink_enabled(const bool p_enabled) {
	caret_blink_enabled = p_enabled;

	if (p_enabled) {
		caret_blink_timer->start();
	} else {
		caret_blink_timer->stop();
	}
	draw_caret = true;
}

float TextEdit::cursor_get_blink_speed() const {
	return caret_blink_timer->get_wait_time();
}

void TextEdit::cursor_set_blink_speed(const float p_speed) {
	ERR_FAIL_COND(p_speed <= 0);
	caret_blink_timer->set_wait_time(p_speed);
}

void TextEdit::cursor_set_block_mode(const bool p_enable) {
	block_caret = p_enable;
	update();
}

bool TextEdit::cursor_is_block_mode() const {
	return block_caret;
}

void TextEdit::_scroll_moved(double p_to_val) {

	if (updating_scrolls)
		return;

	if (h_scroll->is_visible())
		cursor.x_ofs = h_scroll->get_val();
	if (v_scroll->is_visible())
		cursor.line_ofs = v_scroll->get_val();
	update();
}

int TextEdit::get_row_height() const {

	return cache.font->get_height() + cache.line_spacing;
}

int TextEdit::get_char_pos_for(int p_px, String p_str) const {

	int px = 0;
	int c = 0;

	int tab_w = cache.font->get_char_size(' ').width * tab_size;

	while (c < p_str.length()) {

		int w = 0;

		if (p_str[c] == '\t') {

			int left = px % tab_w;
			if (left == 0)
				w = tab_w;
			else
				w = tab_w - px % tab_w; // is right...

		} else {

			w = cache.font->get_char_size(p_str[c], p_str[c + 1]).width;
		}

		if (p_px < (px + w / 2))
			break;
		px += w;
		c++;
	}

	return c;
}

int TextEdit::get_column_x_offset(int p_char, String p_str) {

	int px = 0;

	int tab_w = cache.font->get_char_size(' ').width * tab_size;

	for (int i = 0; i < p_char; i++) {

		if (i >= p_str.length())
			break;

		if (p_str[i] == '\t') {

			int left = px % tab_w;
			if (left == 0)
				px += tab_w;
			else
				px += tab_w - px % tab_w; // is right...

		} else {
			px += cache.font->get_char_size(p_str[i], p_str[i + 1]).width;
		}
	}

	return px;
}

void TextEdit::insert_text_at_cursor(const String &p_text) {

	if (selection.active) {

		cursor_set_line(selection.from_line);
		cursor_set_column(selection.from_column);

		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
	}

	_insert_text_at_cursor(p_text);
	update();
}

Control::CursorShape TextEdit::get_cursor_shape(const Point2 &p_pos) const {
	int gutter = cache.style_normal->get_margin(MARGIN_LEFT) + cache.line_number_w + cache.breakpoint_gutter_width;
	if ((completion_active && completion_rect.has_point(p_pos)) || p_pos.x < gutter) {
		return CURSOR_ARROW;
	}
	return CURSOR_IBEAM;
}

void TextEdit::set_text(String p_text) {

	setting_text = true;
	clear();
	_insert_text_at_cursor(p_text);
	clear_undo_history();
	cursor.column = 0;
	cursor.line = 0;
	cursor.x_ofs = 0;
	cursor.line_ofs = 0;
	cursor.last_fit_x = 0;
	cursor_set_line(0);
	cursor_set_column(0);
	update();
	setting_text = false;

	//get_range()->set(0);
};

String TextEdit::get_text() {
	String longthing;
	int len = text.size();
	for (int i = 0; i < len; i++) {

		longthing += text[i];
		if (i != len - 1)
			longthing += "\n";
	}

	return longthing;
};

String TextEdit::get_text_for_completion() {

	String longthing;
	int len = text.size();
	for (int i = 0; i < len; i++) {

		if (i == cursor.line) {
			longthing += text[i].substr(0, cursor.column);
			longthing += String::chr(0xFFFF); //not unicode, represents the cursor
			longthing += text[i].substr(cursor.column, text[i].size());
		} else {

			longthing += text[i];
		}

		if (i != len - 1)
			longthing += "\n";
	}

	return longthing;
};

String TextEdit::get_line(int line) const {

	if (line < 0 || line >= text.size())
		return "";

	return text[line];
};

void TextEdit::_clear() {

	if (undo_enabled) {
		String undo_text = get_text();

		if (undo_text.length() > 0) {
			_clear_redo();

			/* UNDO!! */
			TextOperation op;
			op.type = TextOperation::TYPE_CLEAR;
			op.from_line = 0;
			op.from_column = 0;
			op.to_line = MAX(0, get_line_count() - 1);
			op.to_column = get_line(op.to_line).length();
			op.text = undo_text;
			op.version = ++version;
			op.chain_forward = false;
			op.chain_backward = false;

			op.prev_version = get_version();
			_push_current_op();
			current_op = op;
		}
	} else {
		clear_undo_history();
	}

	text.clear();
	cursor.column = 0;
	cursor.line = 0;
	cursor.x_ofs = 0;
	cursor.line_ofs = 0;
	cursor.last_fit_x = 0;
}

void TextEdit::clear() {

	setting_text = true;
	_clear();
	setting_text = false;
};

void TextEdit::set_readonly(bool p_readonly) {

	readonly = p_readonly;
}

void TextEdit::set_wrap(bool p_wrap) {

	wrap = p_wrap;
}

void TextEdit::set_max_chars(int p_max_chars) {

	max_chars = p_max_chars;
}

void TextEdit::_reset_caret_blink_timer() {
	if (caret_blink_enabled) {
		caret_blink_timer->stop();
		caret_blink_timer->start();
		draw_caret = true;
		update();
	}
}

void TextEdit::_toggle_draw_caret() {
	draw_caret = !draw_caret;
	if (is_visible() && has_focus() && window_has_focus) {
		update();
	}
}

void TextEdit::_update_caches() {

	cache.style_normal = get_stylebox("normal");
	cache.style_focus = get_stylebox("focus");
	cache.completion_background_color = get_color("completion_background_color");
	cache.completion_selected_color = get_color("completion_selected_color");
	cache.completion_existing_color = get_color("completion_existing_color");
	cache.completion_font_color = get_color("completion_font_color");
	cache.font = get_font("font");
	cache.caret_color = get_color("caret_color");
	cache.caret_background_color = get_color("caret_background_color");
	cache.line_number_color = get_color("line_number_color");
	cache.font_color = get_color("font_color");
	cache.font_selected_color = get_color("font_selected_color");
	cache.keyword_color = get_color("keyword_color");
	cache.function_color = get_color("function_color");
	cache.member_variable_color = get_color("member_variable_color");
	cache.number_color = get_color("number_color");
	cache.selection_color = get_color("selection_color");
	cache.mark_color = get_color("mark_color");
	cache.current_line_color = get_color("current_line_color");
	cache.line_length_guideline_color = get_color("line_length_guideline_color");
	cache.breakpoint_color = get_color("breakpoint_color");
	cache.brace_mismatch_color = get_color("brace_mismatch_color");
	cache.word_highlighted_color = get_color("word_highlighted_color");
	cache.search_result_color = get_color("search_result_color");
	cache.search_result_border_color = get_color("search_result_border_color");
	cache.line_spacing = get_constant("line_spacing");
	cache.row_height = cache.font->get_height() + cache.line_spacing;
	cache.tab_icon = get_icon("tab");
	text.set_font(cache.font);
}

void TextEdit::clear_colors() {

	keywords.clear();
	color_regions.clear();
	;
	text.clear_caches();
	custom_bg_color = Color(0, 0, 0, 0);
}

void TextEdit::set_custom_bg_color(const Color &p_color) {

	custom_bg_color = p_color;
	update();
}

void TextEdit::add_keyword_color(const String &p_keyword, const Color &p_color) {

	keywords[p_keyword] = p_color;
	update();
}

void TextEdit::add_color_region(const String &p_begin_key, const String &p_end_key, const Color &p_color, bool p_line_only) {

	color_regions.push_back(ColorRegion(p_begin_key, p_end_key, p_color, p_line_only));
	text.clear_caches();
	update();
}

void TextEdit::set_symbol_color(const Color &p_color) {

	symbol_color = p_color;
	update();
}

void TextEdit::set_syntax_coloring(bool p_enabled) {

	syntax_coloring = p_enabled;
	update();
}

bool TextEdit::is_syntax_coloring_enabled() const {

	return syntax_coloring;
}

void TextEdit::set_auto_indent(bool p_auto_indent) {
	auto_indent = p_auto_indent;
}

void TextEdit::cut() {

	if (!selection.active) {

		String clipboard = text[cursor.line];
		OS::get_singleton()->set_clipboard(clipboard);
		cursor_set_line(cursor.line);
		cursor_set_column(0);
		_remove_text(cursor.line, 0, cursor.line, text[cursor.line].length());

		backspace_at_cursor();
		update();
		cursor_set_line(cursor.line + 1);
		cut_copy_line = true;

	} else {

		String clipboard = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		OS::get_singleton()->set_clipboard(clipboard);

		cursor_set_line(selection.from_line);
		cursor_set_column(selection.from_column);

		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
		update();
		cut_copy_line = false;
	}
}

void TextEdit::convert_case(int p_case) {
	if (selection.active) {
		String text = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		selection.active = false;
		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		cursor_set_line(selection.from_line);
		cursor_set_column(selection.from_column);
		if (p_case == UPPERCASE) {
			_insert_text_at_cursor(text.to_upper());
		} else if (p_case == LOWERCASE) {
			_insert_text_at_cursor(text.to_lower());
		}
	}
}

void TextEdit::copy() {

	if (!selection.active) {
		String clipboard = _base_get_text(cursor.line, 0, cursor.line, text[cursor.line].length());
		OS::get_singleton()->set_clipboard(clipboard);
		cut_copy_line = true;
	} else {
		String clipboard = _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		OS::get_singleton()->set_clipboard(clipboard);
		cut_copy_line = false;
	}
}

void TextEdit::paste() {

	String clipboard = OS::get_singleton()->get_clipboard();

	if (selection.active) {

		selection.active = false;
		selection.selecting_mode = Selection::MODE_NONE;
		_remove_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
		cursor_set_line(selection.from_line);
		cursor_set_column(selection.from_column);

	} else if (cut_copy_line) {

		cursor_set_column(0);
		String ins = "\n";
		clipboard += ins;
	}

	_insert_text_at_cursor(clipboard);
	update();
}

void TextEdit::select_all() {

	if (text.size() == 1 && text[0].length() == 0)
		return;
	selection.active = true;
	selection.from_line = 0;
	selection.from_column = 0;
	selection.selecting_line = 0;
	selection.selecting_column = 0;
	selection.to_line = text.size() - 1;
	selection.to_column = text[selection.to_line].length();
	selection.selecting_mode = Selection::MODE_SHIFT;
	selection.shiftclick_left = true;
	cursor_set_line(selection.to_line, false);
	cursor_set_column(selection.to_column, false);
	update();
}

void TextEdit::deselect() {

	selection.active = false;
	update();
}

void TextEdit::select(int p_from_line, int p_from_column, int p_to_line, int p_to_column) {

	if (p_from_line >= text.size())
		p_from_line = text.size() - 1;
	if (p_from_column >= text[p_from_line].length())
		p_from_column = text[p_from_line].length();

	if (p_to_line >= text.size())
		p_to_line = text.size() - 1;
	if (p_to_column >= text[p_to_line].length())
		p_to_column = text[p_to_line].length();

	selection.from_line = p_from_line;
	selection.from_column = p_from_column;
	selection.to_line = p_to_line;
	selection.to_column = p_to_column;

	selection.active = true;

	if (selection.from_line == selection.to_line) {

		if (selection.from_column == selection.to_column) {

			selection.active = false;

		} else if (selection.from_column > selection.to_column) {

			selection.shiftclick_left = false;
			SWAP(selection.from_column, selection.to_column);
		} else {

			selection.shiftclick_left = true;
		}
	} else if (selection.from_line > selection.to_line) {

		selection.shiftclick_left = false;
		SWAP(selection.from_line, selection.to_line);
		SWAP(selection.from_column, selection.to_column);
	} else {

		selection.shiftclick_left = true;
	}

	update();
}

bool TextEdit::is_selection_active() const {

	return selection.active;
}
int TextEdit::get_selection_from_line() const {

	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.from_line;
}
int TextEdit::get_selection_from_column() const {

	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.from_column;
}
int TextEdit::get_selection_to_line() const {

	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.to_line;
}
int TextEdit::get_selection_to_column() const {

	ERR_FAIL_COND_V(!selection.active, -1);
	return selection.to_column;
}

String TextEdit::get_selection_text() const {

	if (!selection.active)
		return "";

	return _base_get_text(selection.from_line, selection.from_column, selection.to_line, selection.to_column);
}

String TextEdit::get_word_under_cursor() const {

	int prev_cc = cursor.column;
	while (prev_cc > 0) {
		bool is_char = _is_text_char(text[cursor.line][prev_cc - 1]);
		if (!is_char)
			break;
		--prev_cc;
	}

	int next_cc = cursor.column;
	while (next_cc < text[cursor.line].length()) {
		bool is_char = _is_text_char(text[cursor.line][next_cc]);
		if (!is_char)
			break;
		++next_cc;
	}
	if (prev_cc == cursor.column || next_cc == cursor.column)
		return "";
	return text[cursor.line].substr(prev_cc, next_cc - prev_cc);
}

void TextEdit::set_search_text(const String &p_search_text) {
	search_text = p_search_text;
}

void TextEdit::set_search_flags(uint32_t p_flags) {
	search_flags = p_flags;
}

void TextEdit::set_current_search_result(int line, int col) {
	search_result_line = line;
	search_result_col = col;
	update();
}

void TextEdit::set_highlight_all_occurrences(const bool p_enabled) {
	highlight_all_occurrences = p_enabled;
	update();
}

bool TextEdit::is_highlight_all_occurrences_enabled() const {
	return highlight_all_occurrences;
}

int TextEdit::_get_column_pos_of_word(const String &p_key, const String &p_search, uint32_t p_search_flags, int p_from_column) {
	int col = -1;

	if (p_key.length() > 0 && p_search.length() > 0) {
		if (p_from_column < 0 || p_from_column > p_search.length()) {
			p_from_column = 0;
		}

		while (col == -1 && p_from_column <= p_search.length()) {
			if (p_search_flags & SEARCH_MATCH_CASE) {
				col = p_search.find(p_key, p_from_column);
			} else {
				col = p_search.findn(p_key, p_from_column);
			}

			// whole words only
			if (col != -1 && p_search_flags & SEARCH_WHOLE_WORDS) {
				p_from_column = col;

				if (col > 0 && _is_text_char(p_search[col - 1])) {
					col = -1;
				} else if ((col + p_key.length()) < p_search.length() && _is_text_char(p_search[col + p_key.length()])) {
					col = -1;
				}
			}

			p_from_column += 1;
		}
	}
	return col;
}

DVector<int> TextEdit::_search_bind(const String &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column) const {

	int col, line;
	if (search(p_key, p_search_flags, p_from_line, p_from_column, col, line)) {
		DVector<int> result;
		result.resize(2);
		result.set(0, line);
		result.set(1, col);
		return result;

	} else {

		return DVector<int>();
	}
}

bool TextEdit::search(const String &p_key, uint32_t p_search_flags, int p_from_line, int p_from_column, int &r_line, int &r_column) const {

	if (p_key.length() == 0)
		return false;
	ERR_FAIL_INDEX_V(p_from_line, text.size(), false);
	ERR_FAIL_INDEX_V(p_from_column, text[p_from_line].length() + 1, false);

	//search through the whole documment, but start by current line

	int line = -1;
	int pos = -1;

	line = p_from_line;

	for (int i = 0; i < text.size() + 1; i++) {
		//backwards is broken...
		//int idx=(p_search_flags&SEARCH_BACKWARDS)?(text.size()-i):i; //do backwards seearch

		if (line < 0) {
			line = text.size() - 1;
		}
		if (line == text.size()) {
			line = 0;
		}

		String text_line = text[line];
		int from_column = 0;
		if (line == p_from_line) {

			if (i == text.size()) {
				//wrapped

				if (p_search_flags & SEARCH_BACKWARDS) {
					from_column = text_line.length();
				} else {
					from_column = 0;
				}

			} else {

				from_column = p_from_column;
			}

		} else {
			if (p_search_flags & SEARCH_BACKWARDS)
				from_column = text_line.length() - 1;
			else
				from_column = 0;
		}

		pos = -1;

		int pos_from = 0;
		int last_pos = -1;
		while ((last_pos = (p_search_flags & SEARCH_MATCH_CASE) ? text_line.find(p_key, pos_from) : text_line.findn(p_key, pos_from)) != -1) {

			if (p_search_flags & SEARCH_BACKWARDS) {

				if (last_pos > from_column)
					break;
				pos = last_pos;

			} else {

				if (last_pos >= from_column) {
					pos = last_pos;
					break;
				}
			}

			pos_from = last_pos + p_key.length();
		}

		if (pos != -1 && (p_search_flags & SEARCH_WHOLE_WORDS)) {
			//validate for whole words
			if (pos > 0 && _is_text_char(text_line[pos - 1]))
				pos = -1;
			else if (_is_text_char(text_line[pos + p_key.length()]))
				pos = -1;
		}

		if (pos != -1)
			break;

		if (p_search_flags & SEARCH_BACKWARDS)
			line--;
		else
			line++;
	}

	if (pos == -1) {
		r_line = -1;
		r_column = -1;
		return false;
	}

	r_line = line;
	r_column = pos;

	return true;
}

void TextEdit::_cursor_changed_emit() {

	emit_signal("cursor_changed");
	cursor_changed_dirty = false;
}

void TextEdit::_text_changed_emit() {

	emit_signal("text_changed");
	text_changed_dirty = false;
}

void TextEdit::set_line_as_marked(int p_line, bool p_marked) {

	ERR_FAIL_INDEX(p_line, text.size());
	text.set_marked(p_line, p_marked);
	update();
}

bool TextEdit::is_line_set_as_breakpoint(int p_line) const {

	ERR_FAIL_INDEX_V(p_line, text.size(), false);
	return text.is_breakpoint(p_line);
}

void TextEdit::set_line_as_breakpoint(int p_line, bool p_breakpoint) {

	ERR_FAIL_INDEX(p_line, text.size());
	text.set_breakpoint(p_line, p_breakpoint);
	update();
}

void TextEdit::get_breakpoints(List<int> *p_breakpoints) const {

	for (int i = 0; i < text.size(); i++) {
		if (text.is_breakpoint(i))
			p_breakpoints->push_back(i);
	}
}

int TextEdit::get_line_count() const {

	return text.size();
}

void TextEdit::_do_text_op(const TextOperation &p_op, bool p_reverse) {

	ERR_FAIL_COND(p_op.type == TextOperation::TYPE_NONE);

	bool insert = p_op.type == TextOperation::TYPE_INSERT;
	if (p_reverse)
		insert = !insert;

	if (insert) {

		int check_line;
		int check_column;
		_base_insert_text(p_op.from_line, p_op.from_column, p_op.text, check_line, check_column);
		ERR_FAIL_COND(check_line != p_op.to_line); // BUG
		ERR_FAIL_COND(check_column != p_op.to_column); // BUG
	} else {

		_base_remove_text(p_op.from_line, p_op.from_column, p_op.to_line, p_op.to_column);
	}
}

void TextEdit::_clear_redo() {

	if (undo_stack_pos == NULL)
		return; //nothing to clear

	_push_current_op();

	while (undo_stack_pos) {
		List<TextOperation>::Element *elem = undo_stack_pos;
		undo_stack_pos = undo_stack_pos->next();
		undo_stack.erase(elem);
	}
}

void TextEdit::undo() {

	_push_current_op();

	if (undo_stack_pos == NULL) {

		if (!undo_stack.size())
			return; //nothing to undo

		undo_stack_pos = undo_stack.back();

	} else if (undo_stack_pos == undo_stack.front())
		return; // at the bottom of the undo stack
	else
		undo_stack_pos = undo_stack_pos->prev();

	TextOperation op = undo_stack_pos->get();
	_do_text_op(op, true);
	current_op.version = op.prev_version;
	if (undo_stack_pos->get().chain_backward) {
		while (true) {
			ERR_BREAK(!undo_stack_pos->prev());
			undo_stack_pos = undo_stack_pos->prev();
			op = undo_stack_pos->get();
			_do_text_op(op, true);
			current_op.version = op.prev_version;
			if (undo_stack_pos->get().chain_forward) {
				break;
			}
		}
	}

	if (undo_stack_pos->get().type == TextOperation::TYPE_REMOVE || undo_stack_pos->get().type == TextOperation::TYPE_CLEAR) {
		cursor_set_line(undo_stack_pos->get().to_line);
		cursor_set_column(undo_stack_pos->get().to_column);
		_cancel_code_hint();
	} else {
		cursor_set_line(undo_stack_pos->get().from_line);
		cursor_set_column(undo_stack_pos->get().from_column);
	}
	update();
}

void TextEdit::redo() {

	_push_current_op();

	if (undo_stack_pos == NULL)
		return; //nothing to do.

	TextOperation op = undo_stack_pos->get();
	_do_text_op(op, false);
	current_op.version = op.version;
	if (undo_stack_pos->get().chain_forward) {

		while (true) {
			ERR_BREAK(!undo_stack_pos->next());
			undo_stack_pos = undo_stack_pos->next();
			op = undo_stack_pos->get();
			_do_text_op(op, false);
			current_op.version = op.version;
			if (undo_stack_pos->get().chain_backward)
				break;
		}
	}
	cursor_set_line(undo_stack_pos->get().to_line);
	cursor_set_column(undo_stack_pos->get().to_column);
	undo_stack_pos = undo_stack_pos->next();
	update();
}

void TextEdit::clear_undo_history() {

	saved_version = 0;
	current_op.type = TextOperation::TYPE_NONE;
	undo_stack_pos = NULL;
	undo_stack.clear();
}

void TextEdit::begin_complex_operation() {
	_push_current_op();
	next_operation_is_complex = true;
}

void TextEdit::end_complex_operation() {

	_push_current_op();
	ERR_FAIL_COND(undo_stack.size() == 0);

	if (undo_stack.back()->get().chain_forward) {
		undo_stack.back()->get().chain_forward = false;
		return;
	}

	undo_stack.back()->get().chain_backward = true;
}

void TextEdit::_push_current_op() {

	if (current_op.type == TextOperation::TYPE_NONE)
		return; // do nothing

	if (next_operation_is_complex) {
		current_op.chain_forward = true;
		next_operation_is_complex = false;
	}

	undo_stack.push_back(current_op);
	current_op.type = TextOperation::TYPE_NONE;
	current_op.text = "";
	current_op.chain_forward = false;
}

void TextEdit::set_tab_size(const int p_size) {
	ERR_FAIL_COND(p_size <= 0);
	tab_size = p_size;
	text.set_tab_size(p_size);
	update();
}

void TextEdit::set_draw_tabs(bool p_draw) {

	draw_tabs = p_draw;
}

bool TextEdit::is_drawing_tabs() const {

	return draw_tabs;
}

void TextEdit::set_insert_mode(bool p_enabled) {
	insert_mode = p_enabled;
	update();
}

bool TextEdit::is_insert_mode() const {
	return insert_mode;
}

uint32_t TextEdit::get_version() const {
	return current_op.version;
}
uint32_t TextEdit::get_saved_version() const {

	return saved_version;
}
void TextEdit::tag_saved_version() {

	saved_version = get_version();
}

int TextEdit::get_v_scroll() const {

	return v_scroll->get_val();
}
void TextEdit::set_v_scroll(int p_scroll) {

	if (!scroll_past_end_of_file_enabled) {
		if (p_scroll + get_visible_rows() > get_line_count()) {
			p_scroll = get_line_count() - get_visible_rows();
		}
	}
	v_scroll->set_val(p_scroll);
	cursor.line_ofs = p_scroll;
}

int TextEdit::get_h_scroll() const {

	return h_scroll->get_val();
}
void TextEdit::set_h_scroll(int p_scroll) {
	h_scroll->set_val(p_scroll);
}

void TextEdit::set_completion(bool p_enabled, const Vector<String> &p_prefixes) {

	completion_prefixes.clear();
	completion_enabled = p_enabled;
	for (int i = 0; i < p_prefixes.size(); i++)
		completion_prefixes.insert(p_prefixes[i]);
}

void TextEdit::_confirm_completion() {

	begin_complex_operation();

	_remove_text(cursor.line, cursor.column - completion_base.length(), cursor.line, cursor.column);
	cursor_set_column(cursor.column - completion_base.length(), false);
	insert_text_at_cursor(completion_current);

	if (completion_current.ends_with("(") && auto_brace_completion_enabled) {
		insert_text_at_cursor(")");
		cursor.column--;
	}

	end_complex_operation();

	_cancel_completion();
}

void TextEdit::_cancel_code_hint() {

	VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 0);
	raised_from_completion = false;
	completion_hint = "";
	update();
}

void TextEdit::_cancel_completion() {

	VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 0);
	raised_from_completion = false;
	if (!completion_active)
		return;

	completion_active = false;
	update();
}

static bool _is_completable(CharType c) {

	return !_is_symbol(c) || c == '"' || c == '\'';
}

void TextEdit::_update_completion_candidates() {

	String l = text[cursor.line];
	int cofs = CLAMP(cursor.column, 0, l.length());

	String s;

	//look for keywords first

	bool inquote = false;
	int first_quote = -1;

	int c = cofs - 1;
	while (c >= 0) {
		if (l[c] == '"' || l[c] == '\'') {
			inquote = !inquote;
			if (first_quote == -1)
				first_quote = c;
		}
		c--;
	}

	bool pre_keyword = false;
	bool cancel = false;

	//print_line("inquote: "+itos(inquote)+"first quote "+itos(first_quote)+" cofs-1 "+itos(cofs-1));
	if (!inquote && first_quote == cofs - 1) {
		//no completion here
		//print_line("cancel!");
		cancel = true;
	}
	if (inquote && first_quote != -1) {

		s = l.substr(first_quote, cofs - first_quote);
		//print_line("s: 1"+s);
	} else if (cofs > 0 && l[cofs - 1] == ' ') {
		int kofs = cofs - 1;
		String kw;
		while (kofs >= 0 && l[kofs] == ' ')
			kofs--;

		while (kofs >= 0 && l[kofs] > 32 && _is_completable(l[kofs])) {
			kw = String::chr(l[kofs]) + kw;
			kofs--;
		}

		pre_keyword = keywords.has(kw);
		//print_line("KW "+kw+"? "+itos(pre_keyword));

	} else {

		while (cofs > 0 && l[cofs - 1] > 32 && _is_completable(l[cofs - 1])) {
			s = String::chr(l[cofs - 1]) + s;
			if (l[cofs - 1] == '\'' || l[cofs - 1] == '"')
				break;

			cofs--;
		}
	}

	if (cursor.column > 0 && l[cursor.column - 1] == '(' && !pre_keyword && !completion_strings[0].begins_with("\"")) {
		cancel = true;
	}

	update();

	if (cancel || (!pre_keyword && s == "" && (cofs == 0 || !completion_prefixes.has(String::chr(l[cofs - 1]))))) {
		//none to complete, cancel
		_cancel_completion();
		return;
	}

	completion_options.clear();
	completion_index = 0;
	completion_base = s;
	Vector<float> sim_cache;
	for (int i = 0; i < completion_strings.size(); i++) {
		if (s == completion_strings[i]) {
			// A perfect match, stop completion
			_cancel_completion();
			return;
		}
		if (s.is_subsequence_ofi(completion_strings[i])) {
			// don't remove duplicates if no input is provided
			if (s != "" && completion_options.find(completion_strings[i]) != -1) {
				continue;
			}
			// Calculate the similarity to keep completions in good order
			float similarity;
			if (completion_strings[i].to_lower().begins_with(s.to_lower())) {
				// Substrings are the best candidates
				similarity = 1.1;
			} else {
				// Otherwise compute the similarity
				similarity = s.to_lower().similarity(completion_strings[i].to_lower());
			}

			int comp_size = completion_options.size();
			if (comp_size == 0) {
				completion_options.push_back(completion_strings[i]);
				sim_cache.push_back(similarity);
			} else {
				float comp_sim;
				int pos = 0;
				do {
					comp_sim = sim_cache[pos++];
				} while (pos < comp_size && similarity < comp_sim);
				pos = similarity > comp_sim ? pos - 1 : pos; // Pos will be off by one
				completion_options.insert(pos, completion_strings[i]);
				sim_cache.insert(pos, similarity);
			}
		}
	}

	if (completion_options.size() == 0) {
		//no options to complete, cancel
		_cancel_completion();
		return;
	}

	// The top of the list is the best match
	completion_current = completion_options[0];

#if 0 // even there's only one option, user still get the chance to choose using it or not
	if (completion_options.size()==1) {
		//one option to complete, just complete it automagically
		_confirm_completion();
		//		insert_text_at_cursor(completion_options[0].substr(s.length(),completion_options[0].length()-s.length()));
		_cancel_completion();
		return;

	}
#endif

	completion_enabled = true;
}

void TextEdit::query_code_comple() {

	String l = text[cursor.line];
	int ofs = CLAMP(cursor.column, 0, l.length());

	bool inquote = false;

	int c = ofs - 1;
	while (c >= 0) {
		if (l[c] == '"' || l[c] == '\'')
			inquote = !inquote;
		c--;
	}

	if (ofs > 0 && (inquote || _is_completable(l[ofs - 1]) || completion_prefixes.has(String::chr(l[ofs - 1]))))
		emit_signal("request_completion");
}

void TextEdit::set_code_hint(const String &p_hint) {

	VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 1);
	raised_from_completion = true;
	completion_hint = p_hint;
	completion_hint_offset = -0xFFFF;
	update();
}

void TextEdit::code_complete(const Vector<String> &p_strings) {

	VisualServer::get_singleton()->canvas_item_set_z(get_canvas_item(), 1);
	raised_from_completion = true;
	completion_strings = p_strings;
	completion_active = true;
	completion_current = "";
	completion_index = 0;
	_update_completion_candidates();
	//
}

String TextEdit::get_tooltip(const Point2 &p_pos) const {

	if (!tooltip_obj)
		return Control::get_tooltip(p_pos);
	int row, col;
	_get_mouse_pos(p_pos, row, col);

	String s = text[row];
	if (s.length() == 0)
		return Control::get_tooltip(p_pos);
	int beg = CLAMP(col, 0, s.length());
	int end = beg;

	if (s[beg] > 32 || beg == s.length()) {

		bool symbol = beg < s.length() && _is_symbol(s[beg]); //not sure if right but most editors behave like this

		while (beg > 0 && s[beg - 1] > 32 && (symbol == _is_symbol(s[beg - 1]))) {
			beg--;
		}
		while (end < s.length() && s[end + 1] > 32 && (symbol == _is_symbol(s[end + 1]))) {
			end++;
		}

		if (end < s.length())
			end += 1;

		String tt = tooltip_obj->call(tooltip_func, s.substr(beg, end - beg), tooltip_ud);

		return tt;
	}

	return Control::get_tooltip(p_pos);
}

void TextEdit::set_tooltip_request_func(Object *p_obj, const StringName &p_function, const Variant &p_udata) {

	tooltip_obj = p_obj;
	tooltip_func = p_function;
	tooltip_ud = p_udata;
}

void TextEdit::set_line(int line, String new_text) {
	if (line < 0 || line > text.size())
		return;
	_remove_text(line, 0, line, text[line].length());
	_insert_text(line, 0, new_text);
	if (cursor.line == line) {
		cursor.column = MIN(cursor.column, new_text.length());
	}
}

void TextEdit::insert_at(const String &p_text, int at) {
	cursor_set_column(0);
	cursor_set_line(at);
	_insert_text(at, 0, p_text + "\n");
}

void TextEdit::set_show_line_numbers(bool p_show) {

	line_numbers = p_show;
	update();
}

void TextEdit::set_line_numbers_zero_padded(bool p_zero_padded) {

	line_numbers_zero_padded = p_zero_padded;
	update();
}

bool TextEdit::is_show_line_numbers_enabled() const {
	return line_numbers;
}

void TextEdit::set_show_line_length_guideline(bool p_show) {
	line_length_guideline = p_show;
	update();
}

void TextEdit::set_line_length_guideline_column(int p_column) {
	line_length_guideline_col = p_column;
	update();
}

void TextEdit::set_draw_breakpoint_gutter(bool p_draw) {
	draw_breakpoint_gutter = p_draw;
	update();
}

bool TextEdit::is_drawing_breakpoint_gutter() const {
	return draw_breakpoint_gutter;
}

void TextEdit::set_breakpoint_gutter_width(int p_gutter_width) {
	breakpoint_gutter_width = p_gutter_width;
	update();
}

int TextEdit::get_breakpoint_gutter_width() const {
	return cache.breakpoint_gutter_width;
}

bool TextEdit::is_text_field() const {

	return true;
}

void TextEdit::menu_option(int p_option) {

	switch (p_option) {
		case MENU_CUT: {
			if (!readonly) {
				cut();
			}
		} break;
		case MENU_COPY: {
			copy();
		} break;
		case MENU_PASTE: {
			if (!readonly) {
				paste();
			}
		} break;
		case MENU_CLEAR: {
			if (!readonly) {
				clear();
			}
		} break;
		case MENU_UPPERCASE: {
			if (!readonly) {
				convert_case(UPPERCASE);
			}
		} break;
		case MENU_LOWERCASE: {
			if (!readonly) {
				convert_case(LOWERCASE);
			}
		} break;
		case MENU_SELECT_ALL: {
			select_all();
		} break;
		case MENU_UNDO: {
			undo();
		} break;
	};
}

PopupMenu *TextEdit::get_menu() const {
	return menu;
}

void TextEdit::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_input_event"), &TextEdit::_input_event);
	ObjectTypeDB::bind_method(_MD("_scroll_moved"), &TextEdit::_scroll_moved);
	ObjectTypeDB::bind_method(_MD("_cursor_changed_emit"), &TextEdit::_cursor_changed_emit);
	ObjectTypeDB::bind_method(_MD("_text_changed_emit"), &TextEdit::_text_changed_emit);
	ObjectTypeDB::bind_method(_MD("_push_current_op"), &TextEdit::_push_current_op);
	ObjectTypeDB::bind_method(_MD("_click_selection_held"), &TextEdit::_click_selection_held);
	ObjectTypeDB::bind_method(_MD("_toggle_draw_caret"), &TextEdit::_toggle_draw_caret);

	BIND_CONSTANT(SEARCH_MATCH_CASE);
	BIND_CONSTANT(SEARCH_WHOLE_WORDS);
	BIND_CONSTANT(SEARCH_BACKWARDS);

	/*
    ObjectTypeDB::bind_method(_MD("delete_char"),&TextEdit::delete_char);
    ObjectTypeDB::bind_method(_MD("delete_line"),&TextEdit::delete_line);
*/

	ObjectTypeDB::bind_method(_MD("set_text", "text"), &TextEdit::set_text);
	ObjectTypeDB::bind_method(_MD("insert_text_at_cursor", "text"), &TextEdit::insert_text_at_cursor);

	ObjectTypeDB::bind_method(_MD("get_line_count"), &TextEdit::get_line_count);
	ObjectTypeDB::bind_method(_MD("get_text"), &TextEdit::get_text);
	ObjectTypeDB::bind_method(_MD("get_line", "line"), &TextEdit::get_line);

	ObjectTypeDB::bind_method(_MD("cursor_set_column", "column", "adjust_viewport"), &TextEdit::cursor_set_column, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("cursor_set_line", "line", "adjust_viewport"), &TextEdit::cursor_set_line, DEFVAL(false));

	ObjectTypeDB::bind_method(_MD("cursor_get_column"), &TextEdit::cursor_get_column);
	ObjectTypeDB::bind_method(_MD("cursor_get_line"), &TextEdit::cursor_get_line);
	ObjectTypeDB::bind_method(_MD("cursor_set_blink_enabled", "enable"), &TextEdit::cursor_set_blink_enabled);
	ObjectTypeDB::bind_method(_MD("cursor_get_blink_enabled"), &TextEdit::cursor_get_blink_enabled);
	ObjectTypeDB::bind_method(_MD("cursor_set_blink_speed", "blink_speed"), &TextEdit::cursor_set_blink_speed);
	ObjectTypeDB::bind_method(_MD("cursor_get_blink_speed"), &TextEdit::cursor_get_blink_speed);
	ObjectTypeDB::bind_method(_MD("cursor_set_block_mode", "enable"), &TextEdit::cursor_set_block_mode);
	ObjectTypeDB::bind_method(_MD("cursor_is_block_mode"), &TextEdit::cursor_is_block_mode);

	ObjectTypeDB::bind_method(_MD("set_readonly", "enable"), &TextEdit::set_readonly);
	ObjectTypeDB::bind_method(_MD("set_wrap", "enable"), &TextEdit::set_wrap);
	ObjectTypeDB::bind_method(_MD("set_max_chars", "amount"), &TextEdit::set_max_chars);

	ObjectTypeDB::bind_method(_MD("cut"), &TextEdit::cut);
	ObjectTypeDB::bind_method(_MD("copy"), &TextEdit::copy);
	ObjectTypeDB::bind_method(_MD("paste"), &TextEdit::paste);
	ObjectTypeDB::bind_method(_MD("select_all"), &TextEdit::select_all);
	ObjectTypeDB::bind_method(_MD("select", "from_line", "from_column", "to_line", "to_column"), &TextEdit::select);
	ObjectTypeDB::bind_method(_MD("convert_case", "case"), &TextEdit::convert_case);

	ObjectTypeDB::bind_method(_MD("is_selection_active"), &TextEdit::is_selection_active);
	ObjectTypeDB::bind_method(_MD("get_selection_from_line"), &TextEdit::get_selection_from_line);
	ObjectTypeDB::bind_method(_MD("get_selection_from_column"), &TextEdit::get_selection_from_column);
	ObjectTypeDB::bind_method(_MD("get_selection_to_line"), &TextEdit::get_selection_to_line);
	ObjectTypeDB::bind_method(_MD("get_selection_to_column"), &TextEdit::get_selection_to_column);
	ObjectTypeDB::bind_method(_MD("get_selection_text"), &TextEdit::get_selection_text);
	ObjectTypeDB::bind_method(_MD("get_word_under_cursor"), &TextEdit::get_word_under_cursor);
	ObjectTypeDB::bind_method(_MD("search", "flags", "from_line", "from_column", "to_line", "to_column"), &TextEdit::_search_bind);

	ObjectTypeDB::bind_method(_MD("undo"), &TextEdit::undo);
	ObjectTypeDB::bind_method(_MD("redo"), &TextEdit::redo);
	ObjectTypeDB::bind_method(_MD("clear_undo_history"), &TextEdit::clear_undo_history);

	ObjectTypeDB::bind_method(_MD("set_show_line_numbers", "enable"), &TextEdit::set_show_line_numbers);
	ObjectTypeDB::bind_method(_MD("is_show_line_numbers_enabled"), &TextEdit::is_show_line_numbers_enabled);

	ObjectTypeDB::bind_method(_MD("set_highlight_all_occurrences", "enable"), &TextEdit::set_highlight_all_occurrences);
	ObjectTypeDB::bind_method(_MD("is_highlight_all_occurrences_enabled"), &TextEdit::is_highlight_all_occurrences_enabled);

	ObjectTypeDB::bind_method(_MD("set_syntax_coloring", "enable"), &TextEdit::set_syntax_coloring);
	ObjectTypeDB::bind_method(_MD("is_syntax_coloring_enabled"), &TextEdit::is_syntax_coloring_enabled);

	ObjectTypeDB::bind_method(_MD("add_keyword_color", "keyword", "color"), &TextEdit::add_keyword_color);
	ObjectTypeDB::bind_method(_MD("add_color_region", "begin_key", "end_key", "color", "line_only"), &TextEdit::add_color_region, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("set_symbol_color", "color"), &TextEdit::set_symbol_color);
	ObjectTypeDB::bind_method(_MD("set_custom_bg_color", "color"), &TextEdit::set_custom_bg_color);
	ObjectTypeDB::bind_method(_MD("clear_colors"), &TextEdit::clear_colors);
	ObjectTypeDB::bind_method(_MD("menu_option"), &TextEdit::menu_option);
	ObjectTypeDB::bind_method(_MD("get_menu:PopupMenu"), &TextEdit::get_menu);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "syntax_highlighting"), _SCS("set_syntax_coloring"), _SCS("is_syntax_coloring_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_line_numbers"), _SCS("set_show_line_numbers"), _SCS("is_show_line_numbers_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "highlight_all_occurrences"), _SCS("set_highlight_all_occurrences"), _SCS("is_highlight_all_occurrences_enabled"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret/block_caret"), _SCS("cursor_set_block_mode"), _SCS("cursor_is_block_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret/caret_blink"), _SCS("cursor_set_blink_enabled"), _SCS("cursor_get_blink_enabled"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::REAL, "caret/caret_blink_speed", PROPERTY_HINT_RANGE, "0.1,10,0.1"), _SCS("cursor_set_blink_speed"), _SCS("cursor_get_blink_speed"));

	ADD_SIGNAL(MethodInfo("cursor_changed"));
	ADD_SIGNAL(MethodInfo("text_changed"));
	ADD_SIGNAL(MethodInfo("request_completion"));
	ADD_SIGNAL(MethodInfo("breakpoint_toggled", PropertyInfo(Variant::INT, "row")));

	BIND_CONSTANT(MENU_CUT);
	BIND_CONSTANT(MENU_COPY);
	BIND_CONSTANT(MENU_PASTE);
	BIND_CONSTANT(MENU_CLEAR);
	BIND_CONSTANT(MENU_SELECT_ALL);
	BIND_CONSTANT(MENU_UNDO);
	BIND_CONSTANT(MENU_MAX);
}

TextEdit::TextEdit() {

	readonly = false;
	setting_row = false;
	draw_tabs = false;
	draw_caret = true;
	max_chars = 0;
	clear();
	wrap = false;
	set_focus_mode(FOCUS_ALL);
	_update_caches();
	cache.size = Size2(1, 1);
	cache.row_height = 1;
	cache.line_spacing = 1;
	cache.line_number_w = 1;
	cache.breakpoint_gutter_width = 0;
	breakpoint_gutter_width = 0;

	tab_size = 4;
	text.set_tab_size(tab_size);
	text.clear();
	//	text.insert(1,"Mongolia..");
	//	text.insert(2,"PAIS GENEROSO!!");
	text.set_color_regions(&color_regions);

	h_scroll = memnew(HScrollBar);
	v_scroll = memnew(VScrollBar);

	add_child(h_scroll);
	add_child(v_scroll);

	updating_scrolls = false;
	selection.active = false;

	h_scroll->connect("value_changed", this, "_scroll_moved");
	v_scroll->connect("value_changed", this, "_scroll_moved");

	cursor_changed_dirty = false;
	text_changed_dirty = false;

	selection.selecting_mode = Selection::MODE_NONE;
	selection.selecting_line = 0;
	selection.selecting_column = 0;
	selection.selecting_text = false;
	selection.active = false;
	syntax_coloring = false;

	block_caret = false;
	caret_blink_enabled = false;
	caret_blink_timer = memnew(Timer);
	add_child(caret_blink_timer);
	caret_blink_timer->set_wait_time(0.65);
	caret_blink_timer->connect("timeout", this, "_toggle_draw_caret");
	cursor_set_blink_enabled(false);

	custom_bg_color = Color(0, 0, 0, 0);
	idle_detect = memnew(Timer);
	add_child(idle_detect);
	idle_detect->set_one_shot(true);
	idle_detect->set_wait_time(GLOBAL_DEF("display/text_edit_idle_detect_sec", 3));
	idle_detect->connect("timeout", this, "_push_current_op");

	click_select_held = memnew(Timer);
	add_child(click_select_held);
	click_select_held->set_wait_time(0.05);
	click_select_held->connect("timeout", this, "_click_selection_held");

#if 0
	syntax_coloring=true;
	keywords["void"]=Color(0.3,0.0,0.1);
	keywords["int"]=Color(0.3,0.0,0.1);
	keywords["function"]=Color(0.3,0.0,0.1);
	keywords["class"]=Color(0.3,0.0,0.1);
	keywords["extends"]=Color(0.3,0.0,0.1);
	keywords["constructor"]=Color(0.3,0.0,0.1);
	symbol_color=Color(0.1,0.0,0.3,1.0);

	color_regions.push_back(ColorRegion("/*","*/",Color(0.4,0.6,0,4)));
	color_regions.push_back(ColorRegion("//","",Color(0.6,0.6,0.4)));
	color_regions.push_back(ColorRegion("\"","\"",Color(0.4,0.7,0.7)));
	color_regions.push_back(ColorRegion("'","'",Color(0.4,0.8,0.8)));
	color_regions.push_back(ColorRegion("#","",Color(0.2,1.0,0.2)));

#endif

	current_op.type = TextOperation::TYPE_NONE;
	undo_enabled = true;
	undo_stack_pos = NULL;
	setting_text = false;
	last_dblclk = 0;
	current_op.version = 0;
	version = 0;
	saved_version = 0;

	completion_enabled = false;
	completion_active = false;
	completion_line_ofs = 0;
	tooltip_obj = NULL;
	line_numbers = false;
	line_numbers_zero_padded = false;
	line_length_guideline = false;
	line_length_guideline_col = 80;
	draw_breakpoint_gutter = false;
	next_operation_is_complex = false;
	scroll_past_end_of_file_enabled = false;
	auto_brace_completion_enabled = false;
	brace_matching_enabled = false;
	highlight_all_occurrences = false;
	auto_indent = false;
	insert_mode = false;
	window_has_focus = true;

	raised_from_completion = false;

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->add_item(TTR("Cut"), MENU_CUT, KEY_MASK_CMD | KEY_X);
	menu->add_item(TTR("Copy"), MENU_COPY, KEY_MASK_CMD | KEY_C);
	menu->add_item(TTR("Paste"), MENU_PASTE, KEY_MASK_CMD | KEY_V);
	menu->add_separator();
	menu->add_item(TTR("Select All"), MENU_SELECT_ALL, KEY_MASK_CMD | KEY_A);
	menu->add_item(TTR("Clear"), MENU_CLEAR);
	menu->add_separator();
	menu->add_item(TTR("UpperCase"), MENU_UPPERCASE);
	menu->add_item(TTR("LowerCase"), MENU_LOWERCASE);
	menu->add_separator();
	menu->add_item(TTR("Undo"), MENU_UNDO, KEY_MASK_CMD | KEY_Z);
	menu->connect("item_pressed", this, "menu_option");
}

TextEdit::~TextEdit() {
}
