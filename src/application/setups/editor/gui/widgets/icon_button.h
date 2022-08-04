#pragma once
#include "application/setups/editor/detail/maybe_different_colors.h"

namespace editor_widgets {
	template <class F>
	inline bool icon_button(
		const std::string& id_label, 
		const augs::atlas_entry& icon, 
		F&& after_item_callback,
		const std::string& tooltip,
		const bool enabled,
		rgba tint = white, 
		std::array<rgba, 3> icon_bg_cols = {
			rgba(0, 0, 0, 0),
			rgba(35, 60, 90, 255),
			rgba(35+10, 60+10, 90+10, 255)
		}
	) {
		using namespace augs::imgui;

		if (!enabled) {
			tint = rgba(255, 255, 255, 80);
		}

		const auto max_icon_size = ImGui::GetTextLineHeight();
		const auto scaled_icon_size = vec2::scaled_to_max_size(icon.get_original_size(), max_icon_size);

		bool result = false;

		{
			auto preserved = scoped_preserve_cursor();

			auto colored_selectable = scoped_selectable_colors(icon_bg_cols);

			{
				auto scope = maybe_disabled_cols(!enabled);
				result = ImGui::Selectable(id_label.c_str(), false, ImGuiSelectableFlags_None, ImVec2(scaled_icon_size));
				after_item_callback();
			}

			if (ImGui::IsItemHovered()) {
				auto scope = scoped_tooltip();
				text_color(tooltip, tint);
			}
		}

		game_image(icon, scaled_icon_size, tint);
		return result;
	};
}
