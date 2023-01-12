#pragma once
#include "game/organization/special_flavour_id_types.h"
#include "augs/misc/simple_pair.h"
#include "game/cosmos/step_declaration.h"
#include "game/detail/spells/all_spells.h"

using other_equipment_vector = std::vector<augs::simple_pair<int, item_flavour_id>>;

class cosmos;

struct requested_equipment {
	// GEN INTROSPECTOR struct requested_equipment
	item_flavour_id weapon;
	item_flavour_id non_standard_mag;
	item_flavour_id non_standard_charge;
	int num_given_ammo_pieces = -1;

	item_flavour_id back_wearable;
#if 0
	item_flavour_id over_back_wearable;
#endif
	item_flavour_id belt_wearable;
	item_flavour_id personal_deposit_wearable;
	item_flavour_id shoulder_wearable;
	item_flavour_id armor_wearable;

	other_equipment_vector other_equipment;

	learnt_spells_array_type spells_to_give = {};
	// END GEN INTROSPECTOR

	template <class E, class F>
	entity_id generate_for_impl(
		const E& character, 
		cosmos& cosm,
		F&& on_step,
		int max_effects_played
	) const;

	template <class E>
	entity_id generate_for(
		const E& character, 
		logic_step step,
		int max_effects_played = 2
	) const;

	template <class E>
	entity_id generate_for(
		const E& character, 
		cosmos& cosm,
		int max_effects_played = 2
	) const;
};
