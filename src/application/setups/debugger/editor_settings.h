#pragma once
#include "augs/filesystem/path.h"
#include "augs/graphics/rgba.h"

#include "augs/drawing/grid_render_settings.h"

#include "application/setups/debugger/property_editor/property_editor_settings.h"
#include "augs/templates/snapshotted_player_settings.h"

struct editor_autosave_settings {
	// GEN INTROSPECTOR struct editor_autosave_settings
	bool enabled = true;
	bool on_lost_focus = true;
	double once_every_min = 1.0;
	// END GEN INTROSPECTOR

	bool operator!=(const editor_autosave_settings b) const {
		return once_every_min != b.once_every_min || enabled != b.enabled;
	}
};

struct editor_go_to_settings {
	// GEN INTROSPECTOR struct editor_go_to_settings
	unsigned dialog_width = 400;
	unsigned num_lines = 15;
	// END GEN INTROSPECTOR
};

struct editor_entity_selector_settings {
	// GEN INTROSPECTOR struct editor_entity_selector_settings
	rgba held_color = { 65, 131, 196, 160 };
	rgba selected_color = { 65, 131, 196, 120 };
	rgba hovered_color = { 255, 255, 255, 80 };
	rgba hovered_dashed_line_color = { 255, 255, 255, 140 };
	// END GEN INTROSPECTOR
};

struct editor_camera_settings {
	// GEN INTROSPECTOR struct editor_camera_settings
	float panning_speed = 1.f;
	// END GEN INTROSPECTOR
};

struct editor_grid_settings {
	// GEN INTROSPECTOR struct editor_grid_settings
	grid_render_settings render;
	// END GEN INTROSPECTOR
};

struct editor_test_scene_settings {
	// GEN INTROSPECTOR struct editor_test_scene_settings
	unsigned scene_tickrate = 144;
	bool start_bomb_defusal = false; 
	// END GEN INTROSPECTOR
};

struct editor_action_indicator_settings {
	// GEN INTROSPECTOR struct editor_action_indicator_settings
	rgba bg_color = { 0, 0, 0, 180 };
	rgba bg_border_color = { 255, 255, 255, 15 };

	unsigned max_width = 300;
	unsigned show_for_ms = 3000;
	vec2i text_padding = vec2i(5, 5);
	vec2i offset = vec2i(80, 80);
	// END GEN INTROSPECTOR
};

struct editor_settings {
	// GEN INTROSPECTOR struct editor_settings
	editor_autosave_settings autosave;
	bool keep_source_entities_selected_on_mirroring = false;
	
#if TODO
	unsigned remember_last_n_commands = 200;
#endif

	editor_grid_settings grid;
	augs::snapshotted_player_settings player;
	bool save_entropies_to_live_file = false;

	editor_camera_settings camera;
	editor_go_to_settings go_to;
	editor_entity_selector_settings entity_selector;
	property_editor_settings property_editor;
	editor_test_scene_settings test_scene;

	rgba tutorial_text_color = { 210, 210, 210, 255 };

	rgba controlled_entity_color = { 255, 255, 0, 120 };
	rgba matched_entity_color = { 0, 255, 0, 80 };

	rgba rectangular_selection_color = { 65, 131, 196, 60 };
	rgba rectangular_selection_border_color = { 65, 131, 196, 120 };

	editor_action_indicator_settings action_indicator;
	// END GEN INTROSPECTOR
};