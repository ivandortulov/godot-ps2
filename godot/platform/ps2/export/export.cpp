/*************************************************************************/
/*  export.cpp                                                           */
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
#include "export.h"
#include "editor/editor_import_export.h"
#include "platform/ps2/logo.gen.h"
#include "scene/resources/texture.h"

void register_ps2_exporter() {

	Image img(_ps2_logo);
	Ref<ImageTexture> logo = memnew(ImageTexture);
	logo->create_from_image(img);
	{
		Ref<EditorExportPlatformPC> exporter = Ref<EditorExportPlatformPC>(memnew(EditorExportPlatformPC));
		exporter->set_binary_extension("elf");
		exporter->set_release_binary32("godot.ps2.opt.32");
		exporter->set_debug_binary32("godot.ps2.debug.32");
		exporter->set_release_binary64("godot.ps2.opt.32");
		exporter->set_debug_binary64("godot.ps2.debug.32");
		exporter->set_name("PlayStation 2");
		exporter->set_logo(logo);
		exporter->set_chmod_flags(0755);
		EditorImportExport::get_singleton()->add_export_platform(exporter);
	}
}
