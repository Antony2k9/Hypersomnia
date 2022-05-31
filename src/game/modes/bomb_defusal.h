#pragma once
#include <unordered_map>

#include "augs/math/declare_math.h"
#include "game/modes/arena_mode.h"
#include "game/detail/inventory/requested_equipment.h"
#include "game/enums/faction_type.h"
#include "game/cosmos/solvers/standard_solver.h"
#include "game/modes/mode_entropy.h"
#include "game/detail/economy/money_type.h"
#include "game/modes/mode_player_id.h"
#include "game/modes/mode_common.h"
#include "game/components/movement_component.h"
#include "game/enums/battle_event.h"
#include "augs/misc/enum/enum_array.h"
#include "augs/misc/timing/stepped_timing.h"
#include "augs/misc/timing/speed_vars.h"
#include "game/modes/mode_commands/mode_entropy_structs.h"
#include "game/detail/view_input/predictability_info.h"
#include "augs/enums/callback_result.h"
#include "game/enums/faction_choice_result.h"
#include "game/modes/session_id.h"

class cosmos;
struct cosmos_solvable_significant;

struct bomb_defusal_faction_rules {
	// GEN INTROSPECTOR struct bomb_defusal_faction_rules
	requested_equipment initial_eq;
	requested_equipment warmup_initial_eq;
	// END GEN INTROSPECTOR
};

struct bomb_defusal_economy_rules {
	// GEN INTROSPECTOR struct bomb_defusal_economy_rules
	money_type initial_money = 2000;
	money_type maximum_money = 20000;
	money_type warmup_initial_money = 20000;

	money_type losing_faction_award = 1700;
	money_type winning_faction_award = 3700;
	money_type consecutive_loss_bonus = 500;
	unsigned max_consecutive_loss_bonuses = 4;

	money_type team_kill_penalty = 1000;

	money_type lost_but_bomb_planted_team_bonus = 800;
	money_type defused_team_bonus = 1200;

	money_type bomb_plant_award = 600;
	money_type bomb_explosion_award = 1000;
	money_type bomb_defuse_award = 1000;

	bool give_extra_mags_on_first_purchase = true;
	// END GEN INTROSPECTOR
};

struct bomb_defusal_view_rules : arena_mode_view_rules {
	using base = arena_mode_view_rules;
	using introspect_base = base;
	using theme_flavour_type = base::theme_flavour_type;

	// GEN INTROSPECTOR struct bomb_defusal_view_rules
	theme_flavour_type bomb_soon_explodes_theme;
	unsigned secs_until_detonation_to_start_theme = 10;
	// END GEN INTROSPECTOR
};

struct bomb_defusal_ruleset {
	// GEN INTROSPECTOR struct bomb_defusal_ruleset
	std::string name = "Unnamed bomb mode ruleset";

	std::vector<entity_name_str> bot_names;
	std::vector<rgba> player_colors;

	rgba excess_player_color = orange;
	rgba default_player_color = orange;

	bool enable_player_colors = true;
	unsigned bot_quota = 8;

	unsigned allow_spawn_for_secs_after_starting = 10;
	unsigned max_players_per_team = 32;
	unsigned round_secs = 120;
	unsigned round_end_secs = 5;
	unsigned freeze_secs = 10;
	unsigned buy_secs_after_freeze = 30;
	unsigned warmup_secs = 45;
	unsigned warmup_respawn_after_ms = 2000;
	unsigned max_rounds = 30;
	unsigned match_summary_seconds = 15;
	unsigned game_commencing_seconds = 3;
	meter_value_type minimal_damage_for_assist = 41;
	per_actual_faction<bomb_defusal_faction_rules> factions;

	constrained_entity_flavour_id<invariants::explosive, invariants::hand_fuse> bomb_flavour;
	bool delete_lying_items_on_round_start = false;
	bool delete_lying_items_on_warmup = true;
	bool allow_game_commencing = true;
	bool refill_all_mags_on_round_start = true;
	bool refill_chambers_on_round_start = true;
	bool allow_spectator_to_see_both_teams = true;
	bool forbid_going_to_spectator_unless_character_alive = true;
	bool allow_spectate_enemy_if_no_conscious_players = true;
	bool hide_details_when_spectating_enemies = true;

	bool enable_item_shop = true;
	bool warmup_enable_item_shop = false;

	bomb_defusal_economy_rules economy;
	bomb_defusal_view_rules view;

	augs::speed_vars speeds;
	augs::maybe<sentience_shake_settings> override_shake;
	// END GEN INTROSPECTOR

	auto get_num_rounds() const {
		/* Make it even */
		return std::max((max_rounds / 2) * 2, 2u);
	}
};

struct bomb_defusal_faction_state {
	// GEN INTROSPECTOR struct bomb_defusal_faction_state
	unsigned current_spawn_index = 0;
	unsigned score = 0;
	unsigned consecutive_losses = 0;
	std::vector<mode_entity_id> shuffled_spawns;
	// END GEN INTROSPECTOR

	void clear_for_next_half() {
		current_spawn_index = 0;
		consecutive_losses = 0;
		shuffled_spawns.clear();
	}
};

struct arena_mode_player_round_state {
	// GEN INTROSPECTOR struct arena_mode_player_round_state
	item_purchases_vector done_purchases;
	arena_mode_awards_vector awards;
	// END GEN INTROSPECTOR
};

struct bomb_defusal_player_stats {
	// GEN INTROSPECTOR struct bomb_defusal_player_stats
	money_type money = 0;

	int knockout_streak = 0;

	int knockouts = 0;
	int assists = 0;
	int deaths = 0;

	int bomb_plants = 0;
	int bomb_explosions = 0;

	int bomb_defuses = 0;

	arena_mode_player_round_state round_state;
	// END GEN INTROSPECTOR

	int calc_score() const;
};

struct bomb_defusal_player {
	// GEN INTROSPECTOR struct bomb_defusal_player
	player_session_data session;
	entity_id controlled_character_id;
	rgba assigned_color = rgba::zero;
	bomb_defusal_player_stats stats;
	uint32_t round_when_chosen_faction = static_cast<uint32_t>(-1); 
	bool is_bot = false;
	// END GEN INTROSPECTOR

	bomb_defusal_player(const entity_name_str& chosen_name = {}) {
		session.chosen_name = chosen_name;
	}

	bool operator<(const bomb_defusal_player& b) const;

	bool is_set() const;

	const auto& get_chosen_name() const {
		return session.chosen_name;
	}

	const auto& get_faction() const {
		return session.faction;
	}

	const auto& get_session_id() const {
		return session.id;
	}

	auto get_order() const {
		return arena_player_order_info { get_chosen_name(), stats.calc_score() };
	}
};

enum class arena_mode_state {
	// GEN INTROSPECTOR enum class arena_mode_state
	INIT,
	WARMUP,
	LIVE,
	ROUND_END_DELAY,
	MATCH_SUMMARY
	// END GEN INTROSPECTOR
};

using cosmos_clock = augs::stepped_clock;

using bomb_defusal_win = arena_mode_win;
using bomb_defusal_knockout = arena_mode_knockout;
using bomb_defusal_knockouts_vector = arena_mode_knockouts_vector;

struct bomb_defusal_round_state {
	// GEN INTROSPECTOR struct bomb_defusal_round_state
	bool cache_players_frozen = false;
	bomb_defusal_win last_win;
	arena_mode_knockouts_vector knockouts;
	mode_player_id bomb_planter;
	// END GEN INTROSPECTOR
};

enum class round_start_type {
	KEEP_EQUIPMENTS,
	DONT_KEEP_EQUIPMENTS
};

struct editor_property_accessors;

class bomb_defusal {
public:
	using ruleset_type = bomb_defusal_ruleset;
	static constexpr bool needs_initial_signi = true;
	static constexpr bool round_based = true;

	template <bool C>
	struct basic_input {
		const ruleset_type& rules;
		const cosmos_solvable_significant& initial_signi;
		maybe_const_ref_t<C, cosmos> cosm;

		template <bool is_const = C, class = std::enable_if_t<!is_const>>
		operator basic_input<!is_const>() const {
			return { rules, initial_signi, cosm };
		}
	};

	using input = basic_input<false>;
	using const_input = basic_input<true>;

	struct participating_factions {
		faction_type bombing = faction_type::SPECTATOR;
		faction_type defusing = faction_type::SPECTATOR;

		template <class F>
		void for_each(F callback) const {
			callback(bombing);
			callback(defusing);
		}

		std::size_t size() const {
			return 2;
		}

		auto get_all() const {
			augs::constant_size_vector<faction_type, std::size_t(faction_type::COUNT)> result;
			for_each([&result](const auto f) { result.push_back(f); });
			return result;
		}

		void make_swapped(faction_type& f) const {
			if (f == bombing) {
				f = defusing;
				return;
			}

			if (f == defusing) {
				f = bombing;
				return;
			}
		}
	};

	participating_factions calc_participating_factions(const_input) const;
	faction_type calc_weakest_faction(const_input) const;

	bool is_halfway_round(const_input) const;
	bool is_final_round(const_input) const;

	bomb_defusal_player_stats* stats_of(const mode_player_id&);

private:
	struct transferred_inventory {
		struct item {
			constrained_entity_flavour_id<components::item> flavour;

			int charges = -1;
			item_owner_meta owner_meta;
			std::size_t container_index = static_cast<std::size_t>(-1);
			slot_function slot_type = slot_function::INVALID;
			entity_id source_entity_id;
		};

		std::vector<item> items;
	};

	struct round_transferred_player {
		movement_flags movement;
		bool survived = false;
		transferred_inventory saved_eq;
		learnt_spells_array_type saved_spells;
	};

	struct transfer_meta {
		const round_transferred_player& player;
		messages::changed_identities_message& msg;
	};

	using round_transferred_players = std::unordered_map<mode_player_id, round_transferred_player>;
	round_transferred_players make_transferred_players(input) const;

	bool still_freezed() const;
	void make_win(input, logic_step, faction_type);

	entity_id create_character_for_player(input, logic_step, mode_player_id, std::optional<transfer_meta> = std::nullopt);

	void teleport_to_next_spawn(input, entity_id character);
	void init_spawned(
		input, 
		entity_id character, 
		logic_step, 
		std::optional<transfer_meta> = std::nullopt
	);

	void mode_pre_solve(input, const mode_entropy&, logic_step);
	void mode_post_solve(input, const mode_entropy&, const_logic_step);

	void start_next_round(input, logic_step, round_start_type = round_start_type::KEEP_EQUIPMENTS);
	void setup_round(input, logic_step, const round_transferred_players& = {});
	void reshuffle_spawns(const cosmos&, faction_type);

	void set_players_frozen(input in, bool flag);
	void release_triggers_of_weapons_of_players(input in);

	void respawn_the_dead(input, logic_step, unsigned after_ms);

	bool bomb_exploded(const const_input) const;
	entity_id get_character_who_defused_bomb(const_input) const;
	bool bomb_planted(const_input) const;

	void play_start_round_sound(input, const_logic_step);

	void play_faction_sound(const_logic_step, faction_type, assets::sound_id, predictability_info) const;
	void play_faction_sound_for(input, const_logic_step, battle_event, faction_type, predictability_info) const;

	void play_sound_for(input, const_logic_step, battle_event, predictability_info) const;
	void play_win_sound(input, const_logic_step, faction_type) const;
	void play_win_theme(input, const_logic_step, faction_type) const;

	void play_sound_globally(const_logic_step, assets::sound_id, predictability_info) const;

	void play_bomb_defused_sound(input, const_logic_step, faction_type) const;

	void process_win_conditions(input, logic_step);

	std::size_t get_round_rng_seed(const cosmos&) const;
	std::size_t get_step_rng_seed(const cosmos&) const;

	void count_knockout(const_logic_step, input, entity_id victim, const components::sentience&);
	void count_knockout(const_logic_step, input, arena_mode_knockout);

	entity_handle spawn_bomb(input);
	bool give_bomb_to_random_player(input, logic_step);
	void spawn_bomb_near_players(input);

	void end_warmup_and_go_live(input, logic_step);

	void execute_player_commands(input, mode_entropy&, logic_step);
	void add_or_remove_players(input, const mode_entropy&, logic_step);
	void handle_special_commands(input, const mode_entropy&, logic_step);
	void spawn_characters_for_recently_assigned(input, logic_step);
	void spawn_and_kick_bots(input, logic_step);

	void handle_game_commencing(input, logic_step);

	template <class S, class E>
	static auto find_player_by_impl(S& self, const E& identifier);

	/* A server might provide its own integer-based identifiers */
	bool add_player_custom(input, const add_player_input&);

	void remove_player(input, logic_step, const mode_player_id&);
	mode_player_id add_player(input, const entity_name_str& chosen_name);

	faction_choice_result auto_assign_faction(input, const mode_player_id&);
	faction_choice_result choose_faction(const_input, const mode_player_id&, const faction_type faction);

	void restart_match(input, logic_step);

	void reset_players_stats(input);
	void set_players_money_to_initial(input);
	void clear_players_round_state(input);

	void post_award(input, mode_player_id, money_type amount);

	bomb_defusal_player* find_player_by(const entity_name_str& chosen_name);
	bomb_defusal_player* find(const mode_player_id&);

	template <class C, class F>
	void for_each_player_handle_in(C&, faction_type, F&& callback) const;

	// GEN INTROSPECTOR class bomb_defusal
	unsigned rng_seed_offset = 0;

	entity_id bomb_entity;
	cosmos_clock clock_before_setup;
	arena_mode_state state = arena_mode_state::INIT;
	per_actual_faction<bomb_defusal_faction_state> factions;
	std::map<mode_player_id, bomb_defusal_player> players;
	bomb_defusal_round_state current_round;

	bool was_first_blood = false;
	bool should_commence_when_ready = false;
	real32 commencing_timer_ms = -1.f;
	unsigned current_num_bots = 0;
	entity_id bomb_detonation_theme;
	augs::speed_vars round_speeds;
	session_id_type next_session_id = session_id_type::first();
	unsigned scramble_counter = 0;
	unsigned prepare_to_fight_counter = 0;

	mode_player_id duellist_1 = mode_player_id::dead();
	mode_player_id duellist_2 = mode_player_id::dead();
	// END GEN INTROSPECTOR

	friend augs::introspection_access;
	friend editor_property_accessors;

	void on_faction_changed_for(const_input, faction_type previous_faction, const mode_player_id&);
	void assign_free_color_to_best_uncolored(const_input in, faction_type previous_faction, rgba free_color);

	void swap_assigned_factions(const participating_factions&);
	void scramble_assigned_factions(const participating_factions&);

public:

	faction_type get_player_faction(const mode_player_id&) const;
	faction_type get_opposing_faction(const_input, faction_type) const;

	mode_entity_id lookup(const mode_player_id&) const;
	mode_player_id lookup(const mode_entity_id&) const;

	unsigned get_current_round_number() const;
	bool is_first_round_in_half(const const_input in) const;

	float get_seconds_passed_in_cosmos(const_input) const;

	float get_warmup_seconds_left(const_input) const;
	float get_match_begins_in_seconds(const_input) const;

	float get_freeze_seconds_left(const_input) const;
	float get_round_seconds_passed(const_input) const;
	float get_round_seconds_left(const_input) const;
	float get_seconds_since_win(const_input) const;
	float get_match_summary_seconds_left(const_input) const;
	float get_round_end_seconds_left(const_input) const;
	float get_buy_seconds_left(const_input) const;

	real32 get_critical_seconds_left(const_input) const;
	float get_seconds_since_planting(const_input) const;

	unsigned calc_max_faction_score() const;

	std::size_t num_conscious_players_in(const cosmos&, faction_type) const;
	std::size_t num_players_in(faction_type) const;

	mode_player_id find_first_free_player() const;

	arena_mode_match_result calc_match_result(const_input) const;

	unsigned get_score(faction_type) const;

	const bomb_defusal_player* find_player_by(const entity_name_str& chosen_name) const;
	const bomb_defusal_player* find(const mode_player_id&) const;

	const bomb_defusal_player* find(const session_id_type&) const;
	mode_player_id lookup(const session_id_type&) const;

	template <class F>
	void for_each_player_in(faction_type, F&& callback) const;

	template <class F>
	void for_each_player_best_to_worst_in(faction_type, F&& callback) const;

	mode_player_id find_best_player_in(faction_type) const;

	template <class C>
	decltype(auto) advance(
		const input in, 
		mode_entropy entropy, 
		C callbacks,
		const solve_settings settings
	) {
		const auto step_input = logic_step_input { in.cosm, entropy.cosmic, settings };

		return standard_solver()(
			step_input, 
			solver_callbacks(
				[&](const logic_step step) {
					callbacks.pre_solve(step);
					mode_pre_solve(in, entropy, step);
					execute_player_commands(in, entropy, step);
				},
				[&](const const_logic_step step) {
					mode_post_solve(in, entropy, step);
					callbacks.post_solve(step);
				},
				callbacks.post_cleanup
			)
		);
	}

	const auto& get_round_speeds() const {
		return round_speeds;
	}

	auto get_commencing_left_ms() const {
		return commencing_timer_ms;
	}

	const auto& get_current_round() const {
		return current_round;
	}

	const auto& get_players() const {
		return players;
	}

	auto get_state() const {
		return state;
	}

	bool is_match_summary() const {
		return state == arena_mode_state::MATCH_SUMMARY;
	}

	const auto& get_factions_state() const {
		return factions;
	}

	mode_player_id get_next_to_spectate(
		const_input, 
		const arena_player_order_info& after, 
		const faction_type& by_faction, 
		int offset,
		real32 limit_in_seconds
	) const;

	bool suitable_for_spectating(
		const_input, 
		const mode_player_id& who, 
		const mode_player_id& by, 
		real32 limit_in_seconds
	) const;
	bool conscious_or_can_still_spectate(const_input, const mode_player_id& who, real32 limit_in_seconds) const;

	template <class C, class F>
	decltype(auto) on_player_handle(C& cosm, const mode_player_id& id, F&& callback) const {
		if (const auto player_data = find(id)) {
			if (const auto handle = cosm[player_data->controlled_character_id]) {
				return callback(handle);
			}
		}

		return callback(std::nullopt);
	}

	template <class F>
	decltype(auto) on_bomb_entity(const const_input in, F callback) const;

	augs::maybe<rgba> get_current_fallback_color_for(const_input, faction_type faction) const;

	template <class F>
	void for_each_player_id(F callback) const {
		for (const auto& p : players) {
			if (callback(p.first) == callback_result::ABORT) {
				return;
			}
		}
	}

	auto get_next_session_id() const {
		return next_session_id;
	}

	arena_migrated_session emigrate() const;
	void migrate(input, const arena_migrated_session&);

	uint32_t get_num_players() const;
	uint32_t get_num_active_players() const;

	uint32_t get_max_num_active_players(const_input) const;

	void check_duel_of_honor(input, logic_step);
	bool is_a_duellist(const mode_player_id&) const;
	mode_player_id get_opponent_duellist(const mode_player_id&) const;
	void clear_duel();

	void handle_duel_desertion(input, logic_step, const mode_player_id&);
	void report_match_result(input, logic_step);
};
