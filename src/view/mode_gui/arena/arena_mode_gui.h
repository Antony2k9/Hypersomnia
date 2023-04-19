#pragma once
#include <optional>
#include "augs/audio/sound_source.h"
#include "application/setups/draw_setup_gui_input.h"
#include "augs/window_framework/event.h"
#include "view/mode_gui/draw_mode_gui_input.h"

#include "view/mode_gui/arena/arena_scoreboard_gui.h"
#include "view/mode_gui/arena/arena_choose_team_gui.h"
#include "view/mode_gui/arena/arena_buy_menu_gui.h"
#include "view/mode_gui/arena/arena_spectator_gui.h"

#include "augs/gui/formatted_string.h"
#include "augs/misc/randomization.h"
#include "game/detail/view_input/predictability_info.h"

#include "game/modes/mode_entropy.h"

struct warmup_welcome_cache {
	augs::gui::text::formatted_string current;
	augs::gui::text::formatted_string requested;
	std::optional<float> last_seconds_value;
	float completed_at_warmup_secs_left = -1.f;
};

namespace augs {
	struct populate_with_delays_impl;
}

struct arena_gui_state {
	arena_scoreboard_gui scoreboard;
	arena_choose_team_gui choose_team;
	arena_buy_menu_gui buy_menu;
	arena_spectator_gui spectator;
	bool resyncing_notifier = false;

	mutable std::optional<float> last_seconds_value;
	mutable std::unique_ptr<augs::populate_with_delays_impl> populator;
	mutable warmup_welcome_cache warmup;

	mutable augs::sound_source tick_sound;

	arena_gui_state();
	~arena_gui_state();

	bool control(general_gui_intent_input);
	bool escape();

	template <class M>
	mode_player_entropy perform_imgui_and_advance(
		draw_mode_gui_input, 
		const M& mode, 
		const typename M::const_input&,

		prediction_input = prediction_input::offline()
	);

	template <class M>
	void draw_mode_gui(
		const draw_setup_gui_input&,
		const draw_mode_gui_input&,

		const M& mode, 
		const typename M::const_input&,

		prediction_input = prediction_input::offline()
	) const;

	bool requires_cursor() const;

	void reset();
};
