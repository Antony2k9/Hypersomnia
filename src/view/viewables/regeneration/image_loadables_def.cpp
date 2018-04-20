#include "augs/image/image.h"
#include "augs/string/string_templates.h"
#include "augs/filesystem/file.h"

#include "view/viewables/regeneration/image_loadables_def.h"
#include "view/viewables/regeneration/desaturations.h"

#include "augs/templates/introspection_utils/introspective_equal.h"

augs::path_type get_neon_map_path(augs::path_type from_source_path) {
	return std::string(GENERATED_FILES_DIR) + from_source_path.replace_extension(".neon_map.png").string();
}

augs::path_type get_desaturation_path(augs::path_type from_source_path) {
	return std::string(GENERATED_FILES_DIR) + from_source_path.replace_extension(".desaturation.png").string();
}

bool image_loadables_def::operator==(const image_loadables_def& b) const {
	return augs::introspective_equal(*this, b);
}

static const auto official_dir = "content/official/gfx";

image_loadables_def_view::image_loadables_def_view(
	const asset_location_context& project_dir,
	const image_loadables_def& d
) : 
	def(d), 
	resolved_source_image_path(
		d.source_image.is_official ? 
		(augs::path_type(official_dir) / d.source_image.path)
		: (project_dir / d.source_image.path)
	)
{
}

std::optional<augs::path_type> image_loadables_def_view::find_neon_map_path() const {
	if (def.extras.custom_neon_map_path) {
		return *def.extras.custom_neon_map_path;
	}
	else if (def.extras.neon_map) {
		return ::get_neon_map_path(resolved_source_image_path);
	}

	return std::nullopt;
}

std::optional<augs::path_type> image_loadables_def_view::find_desaturation_path() const {
	if (def.should_generate_desaturation()) {
		return ::get_desaturation_path(resolved_source_image_path);
	}

	return std::nullopt;
}

augs::path_type image_loadables_def_view::get_source_image_path() const {
	return resolved_source_image_path;
}

vec2u image_loadables_def_view::read_source_image_size() const {
	return augs::image::get_size(resolved_source_image_path);
}

void image_loadables_def_view::regenerate_all_needed(
	const bool force_regenerate
) const {
	const auto diffuse_path = resolved_source_image_path;

	if (def.extras.neon_map) {
		ensure(!def.extras.custom_neon_map_path.has_value() && "neon_map can't be specified if custom_neon_map_path already is.");
		
		regenerate_neon_map(
			diffuse_path,
			find_neon_map_path().value(),
			def.extras.neon_map.value(),
			force_regenerate
		);
	}
	
	if (def.should_generate_desaturation()) {
		regenerate_desaturation(
			diffuse_path,
			find_desaturation_path().value(),
			force_regenerate
		);
	}
}

void image_loadables_def_view::delete_regenerated_files() const {
	augs::remove_file(::get_neon_map_path(resolved_source_image_path));
	augs::remove_file(::get_desaturation_path(resolved_source_image_path));
}
