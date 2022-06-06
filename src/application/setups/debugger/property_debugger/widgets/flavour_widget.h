#pragma once
#include "game/cosmos/cosmos.h"
#include "application/setups/debugger/property_debugger/widgets/keyboard_acquiring_popup.h"
#include "application/setups/debugger/detail/maybe_different_colors.h"
#include "application/setups/debugger/property_debugger/tweaker_type.h"
#include "application/setups/debugger/detail/sort_flavours_by_name.h"

struct flavour_widget {
	const cosmos& cosm;

	template <class T>
	static constexpr bool handles =
		std::is_convertible_v<T, entity_flavour_id>
	;

	template <class T>
	static constexpr bool handles_prologue = false;

	template <class T>
	auto describe_changed(
		const std::string& formatted_label,
		const T& flavour_id
	) const {
		static_assert(handles<T>);

		if (const auto n = cosm.find_flavour_name(flavour_id)) {
			return typesafe_sprintf("Set %x to %x", formatted_label, *n);
		}

		return typesafe_sprintf("Unset %x", formatted_label);
	}

	template <class T>
	auto handle(const std::string& identity_label, T& flavour_id) {
		static_assert(handles<T>);

		using namespace augs::imgui;

		std::optional<tweaker_type> result;

		const auto displayed_str = [&]() {
			if (const auto n = cosm.find_flavour_name(flavour_id)) {
				return *n;
			}

			return std::string("None");
		}();

		thread_local ImGuiTextFilter filter;
		thread_local keyboard_acquiring_popup track;

		auto id = scoped_id("Flavour selection");

		if (auto combo = track.standard_combo_facade(identity_label.c_str(), displayed_str.c_str())) {
			filter.Draw();

			const bool acquire_keyboard = track.pop_acquire_keyboard();

			if (acquire_keyboard) {
				ImGui::SetKeyboardFocusHere();
			}

			if (detail_select_none(flavour_id)) {
				return std::make_optional(tweaker_type::DISCRETE);
			}

			auto list_flavours_of_type = [&](auto e) {
				using E = decltype(e);
				using id_type = typed_entity_flavour_id<E>;

				thread_local std::vector<id_type> matching_ids;
				matching_ids.clear();

				cosm.for_each_id_and_flavour<E>(
					[&](const id_type& new_id, const auto& flavour) {
						const auto& name = flavour.get_name();

						if (filter.PassFilter(name.c_str())) {
							matching_ids.push_back(new_id);
						}
					}
				);

				if (matching_ids.empty()) {
					return;
				}

				sort_flavours_by_name(cosm, matching_ids);

				static const auto entity_type_label = format_field_name(get_type_name<E>());

				if (cosm.get_flavours_count<E>()) {
					text(entity_type_label);
				}
				else {
					text_disabled(entity_type_label);
				}

				auto scope = scoped_indent();

				for (const auto& new_id : matching_ids) {
					const auto& flavour = cosm.get_flavour(new_id);

					const auto& name = flavour.get_name();
					const bool is_current = flavour_id == new_id;

					if (is_current && acquire_keyboard) {
						ImGui::SetScrollHereY();
					}

					if (ImGui::Selectable(name.c_str(), is_current)) {
						flavour_id = new_id;
						result = tweaker_type::DISCRETE;
					}
				}
			};

			if constexpr(is_typed_flavour_id_v<T>) {
				list_flavours_of_type(entity_type_of<T>());
			}
			else if constexpr(is_constrained_flavour_id_v<T>) {
				for_each_type_in_list<typename T::matching_types>([&](auto e) {
					list_flavours_of_type(e);
				});
			}
		}

		return result;
	}
};
