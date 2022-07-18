#pragma once
#include "augs/misc/imgui/standard_window_mixin.h"
#include "view/necessary_resources.h"
#include "view/viewables/ad_hoc_in_atlas_map.h"
#include "application/setups/editor/nodes/editor_node_id.h"

class editor_setup;

struct editor_filesystem_node;

struct editor_project_files_input {
	editor_setup& setup;
	editor_filesystem_node& files_root;
	const ad_hoc_in_atlas_map& ad_hoc_atlas;
	const necessary_images_in_atlas_map& necessary_images;
};

struct editor_filesystem_gui : standard_window_mixin<editor_filesystem_gui> {
	using base = standard_window_mixin<editor_filesystem_gui>;
	using base::base;
	using introspect_base = base;

	const editor_filesystem_node* dragged_resource = nullptr;
	editor_node_id previewed_created_node;

	void perform(editor_project_files_input);

	void clear_pointers() {
		dragged_resource = nullptr;
	}

	void clear_drag_drop();
};

