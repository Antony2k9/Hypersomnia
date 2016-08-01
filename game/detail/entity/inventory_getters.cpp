#include "game/transcendental/entity_handle.h"
#include "game/detail/inventory_slot_id.h"
#include "game/detail/inventory_slot_handle.h"
#include "game/components/item_component.h"
#include "game/components/item_slot_transfers_component.h"
#include "game/components/gun_component.h"
#include "game/transcendental/cosmos.h"

#include "inventory_getters.h"

template <bool C, class D>
D inventory_getters<C, D>::get_owning_transfer_capability() const {
	auto& self = *static_cast<const D*>(this);
	auto& cosmos = self.get_cosmos();

	if (self.dead())
		return cosmos[entity_id()];

	if (self.has<components::item_slot_transfers>())
		return self;

	auto* maybe_item = self.find<components::item>();

	if (!maybe_item || cosmos[maybe_item->current_slot].dead())
		return cosmos[entity_id()];

	return cosmos[maybe_item->current_slot].get_container().get_owning_transfer_capability();
}

template <bool C, class D>
typename inventory_getters<C, D>::inventory_slot_handle_type inventory_getters<C, D>::first_free_hand() const {
	auto& self = *static_cast<const D*>(this);
	auto& cosmos = self.get_cosmos();

	auto maybe_primary = self[slot_function::PRIMARY_HAND];
	auto maybe_secondary = self[slot_function::SECONDARY_HAND];

	if (maybe_primary.is_empty_slot())
		return maybe_primary;

	if (maybe_secondary.is_empty_slot())
		return maybe_secondary;

	return cosmos[inventory_slot_id()];
}

template <bool C, class D>
typename inventory_getters<C, D>::inventory_slot_handle_type inventory_getters<C, D>::determine_hand_holstering_slot_in(D searched_root_container) const {
	auto& item_entity = *static_cast<const D*>(this);
	auto& cosmos = item_entity.get_cosmos();

	ensure(item_entity.alive());
	ensure(searched_root_container.alive());

	auto maybe_shoulder = searched_root_container[slot_function::SHOULDER_SLOT];

	if (maybe_shoulder.alive()) {
		if (maybe_shoulder.can_contain(item_entity))
			return maybe_shoulder;
		else if (maybe_shoulder->items_inside.size() > 0) {
			auto maybe_item_deposit = maybe_shoulder.get_items_inside()[0][slot_function::ITEM_DEPOSIT];

			if (maybe_item_deposit.alive() && maybe_item_deposit.can_contain(item_entity))
				return maybe_item_deposit;
		}
	}
	else {
		auto maybe_armor = searched_root_container[slot_function::TORSO_ARMOR_SLOT];

		if (maybe_armor.alive())
			if (maybe_armor.can_contain(item_entity))
				return maybe_armor;
	}

	return cosmos[inventory_slot_id()];
}

template <bool C, class D>
typename inventory_getters<C, D>::inventory_slot_handle_type inventory_getters<C, D>::determine_pickup_target_slot_in(D searched_root_container) const {
	auto& item_entity = *static_cast<const D*>(this);
	ensure(item_entity.alive());
	ensure(searched_root_container.alive());
	auto& cosmos = item_entity.get_cosmos();

	auto hidden_slot = item_entity.determine_hand_holstering_slot_in(searched_root_container);;

	if (hidden_slot.alive())
		return hidden_slot;

	if (searched_root_container[slot_function::PRIMARY_HAND].can_contain(item_entity))
		return searched_root_container[slot_function::PRIMARY_HAND];

	if (searched_root_container[slot_function::SECONDARY_HAND].can_contain(item_entity))
		return searched_root_container[slot_function::SECONDARY_HAND];

	return cosmos[inventory_slot_id()];
}

template <bool C, class D>
typename inventory_getters<C, D>::inventory_slot_handle_type inventory_getters<C, D>::map_primary_action_to_secondary_hand_if_primary_empty(int is_action_secondary) const {
	auto& root_container = *static_cast<const D*>(this);

	auto primary = root_container[slot_function::PRIMARY_HAND];
	auto secondary = root_container[slot_function::SECONDARY_HAND];

	if (primary.is_empty_slot())
		return secondary;
	else
		return is_action_secondary ? secondary : primary;
}

template <bool C, class D>
std::vector<D> inventory_getters<C, D>::guns_wielded() const {
	auto& subject = *static_cast<const D*>(this);
	std::vector<D> result;

	{
		auto hand = subject[slot_function::PRIMARY_HAND];

		if (hand.has_items()) {
			auto wielded = hand.get_items_inside()[0];
			if (wielded.has<components::gun>()) {
				result.push_back(wielded);
			}
		}
	}

	{
		auto hand = subject[slot_function::SECONDARY_HAND];

		if (hand.has_items()) {
			auto wielded = hand.get_items_inside()[0];

			if (wielded.has<components::gun>()) {
				result.push_back(wielded);
			}
		}
	}

	return result;
}

template <bool C, class D>
void inventory_getters<C, D>::for_each_contained_item_recursive(std::function<void(basic_entity_handle<C>)> f) const {
	auto& item = *static_cast<const D*>(this);
	auto& cosmos = item.get_cosmos();

	if (item.has<components::container>()) {
		for (auto& s : item.get<components::container>().slots) {
			auto item_handles = cosmos[s.second.items_inside];

			for (auto it : item_handles) {
				f(it);
				it.for_each_contained_item_recursive(f);
			}
		}
	}
}

// explicit instantiation
template class inventory_getters<false, basic_entity_handle<false>>;
template class inventory_getters<true, basic_entity_handle<true>>;