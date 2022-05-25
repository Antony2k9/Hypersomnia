#pragma once
#include <compare>
#include "augs/math/vec2.h"
#include "augs/drawing/flip.h"

#include "augs/templates/maybe.h"
#include "game/assets/image_offsets.h"
#include "view/viewables/regeneration/neon_maps.h"

struct image_usage_as_button {
	// GEN INTROSPECTOR struct image_usage_as_button
	flip_flags flip;
	vec2 bbox_expander;
	// END GEN INTROSPECTOR
};

struct image_extra_loadables {
	// GEN INTROSPECTOR struct image_extra_loadables
	augs::maybe<neon_map_input> generate_neon_map;
	bool generate_desaturation = false;
	pad_bytes<3> pad;
	// END GEN INTROSPECTOR

	bool operator==(const image_extra_loadables&) const = default;

	bool should_generate_desaturation() const {
		return generate_desaturation;
	}

	bool should_generate_neon_map() const {
		return generate_neon_map && generate_neon_map.value.valid();
	}
};

struct image_meta {
	// GEN INTROSPECTOR struct image_meta
	image_extra_loadables extra_loadables;
	image_usage_as_button usage_as_button;
	all_image_offsets offsets; 
	// END GEN INTROSPECTOR
};
