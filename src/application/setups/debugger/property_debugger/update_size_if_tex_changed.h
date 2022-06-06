#pragma once
#include "augs/build_settings/offsetof.h"

#include "game/assets/ids/asset_ids.h"
#include "view/viewables/image_cache.h"

#include "application/setups/debugger/debugger_command_input.h"
#include "application/setups/debugger/property_debugger/fae/fae_tree_structs.h"
#include "application/setups/debugger/commands/change_flavour_property_command.h"
#include "application/setups/debugger/detail/field_address.h"

template <class T>
void update_size_if_tex_changed(
	const edit_invariant_input,
	const flavour_field_address&,
	const T&
) {}

void update_size_if_tex_changed(
	const edit_invariant_input in,
	const flavour_field_address&,
	const invariants::sprite&
);

void update_size_if_tex_changed(
	const edit_invariant_input in,
	const flavour_field_address&,
	const invariants::animation&
);
