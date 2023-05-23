#pragma once
#include "game/detail/visible_entities.hpp"
#include "game/detail/physics/shape_overlapping.hpp"
#include "game/detail/entity_handle_mixins/get_owning_transfer_capability.hpp"

template <class E>
bool bombsite_in_range_of_entity(const E& queried_entity) {
	const auto capability = queried_entity.get_owning_transfer_capability();
	const auto matched_faction = capability.alive() ? capability.get_official_faction() : faction_type::SPECTATOR;

	const auto& cosm = queried_entity.get_cosmos();

	auto& entities = thread_local_visible_entities();

	entities.reacquire_all({
		cosm,
		camera_cone::from_aabb(queried_entity),
		accuracy_type::PROXIMATE,
		render_layer_filter::whitelist(render_layer::AREA_MARKERS),
		{ { tree_of_npo_type::RENDERABLES } }
	});

	bool found = false;

	entities.for_each<render_layer::AREA_MARKERS>(cosm, [&](const auto& area) {
		return area.template dispatch_on_having_all_ret<invariants::area_marker>([&](const auto& typed_area) {
			if constexpr(is_nullopt_v<decltype(typed_area)>) {
				return callback_result::CONTINUE;
			}
			else {
				const auto& marker = typed_area.template get<invariants::area_marker>();

				if (::is_bombsite(marker.type)) {
					if (matched_faction == typed_area.get_official_faction()) {
						if (entity_overlaps_entity(typed_area, queried_entity)) {
							found = true;
							return callback_result::ABORT;
						}
					}
				}

				return callback_result::CONTINUE;
			}
		});
	});

	return found;
}

template <class E>
bool bombsite_in_range(const E& fused_entity) {
	if (fused_entity.template get<components::hand_fuse>().defused()) {
		return false;
	}

	const auto& fuse_def = fused_entity.template get<invariants::hand_fuse>();

	if (fuse_def.can_only_arm_at_bombsites) {
		return bombsite_in_range_of_entity(fused_entity);
	}

	return true;
}

