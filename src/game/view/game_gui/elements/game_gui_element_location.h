#pragma once
#include <variant>

#include "augs/gui/rect_world.h"

#include "game/view/game_gui/elements/locations/slot_button_in_container.h"
#include "game/view/game_gui/elements/locations/item_button_in_item.h"
#include "game/view/game_gui/elements/locations/drag_and_drop_target_drop_item_in_character_gui.h"
#include "game/view/game_gui/elements/locations/hotbar_button_in_character_gui.h"
#include "game/view/game_gui/elements/locations/action_button_in_character_gui.h"
#include "game/view/game_gui/elements/locations/game_gui_root_in_context.h"
#include "game/view/game_gui/elements/locations/value_bar_in_character_gui.h"

using game_gui_element_location = std::variant<
	item_button_in_item,
	slot_button_in_container,
	drag_and_drop_target_drop_item_in_character_gui,
	hotbar_button_in_character_gui,
	action_button_in_character_gui,
	game_gui_root_in_context,
	value_bar_in_character_gui
>;

using game_gui_rect_world = augs::gui::rect_world<game_gui_element_location>;
using game_gui_rect_node = augs::gui::rect_node<game_gui_element_location>;
