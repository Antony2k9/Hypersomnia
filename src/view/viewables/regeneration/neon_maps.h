#pragma once
#include <compare>
#include <vector>
#include <cstddef>
#include <chrono>

#include "augs/graphics/rgba.h"
#include "augs/filesystem/path.h"
#include "augs/filesystem/file_time_type.h"

struct neon_map_input {
	// GEN INTROSPECTOR struct neon_map_input
	float standard_deviation = 6.f;
	vec2u radius = { 80u, 80u };
	float amplification = 60.f;
	float alpha_multiplier = 1.f;

	std::vector<rgba> light_colors;
	// END GEN INTROSPECTOR

	bool operator==(const neon_map_input&) const = default;

	bool valid() const {
		return radius.neither_zero();
	}
};

struct neon_map_stamp {
	// GEN INTROSPECTOR struct neon_map_stamp
	neon_map_input input;
	augs::file_time_type last_write_time_of_source;
	// END GEN INTROSPECTOR
};

struct cached_neon_map_in {
	std::vector<std::byte> new_stamp_bytes;
};

std::optional<cached_neon_map_in> should_regenerate_neon_map(
	const augs::path_type& input_image_path,
	const augs::path_type& output_image_path,
	const neon_map_input in,
	const bool force_regenerate
);

void regenerate_neon_map(
	const augs::path_type& input_image_path,
	const augs::path_type& output_image_path,
	const neon_map_input in,
	cached_neon_map_in
);