/*
	BEFORE YOU CONTINUE:

	If it's your first time reading the codebase...
	work.cpp is arguably the most complex source file in Hypersomnia, boasting over 4000 lines of code.

	Why?

	It's where the top-most level flow control happens.
	Examples of what you'll find here:

	- Initializing window, audio, renderers and passing them by reference to whatever subsystems might require them.
	- Client connecting from the command line.
	- Interactions between all the top-level app modules like e.g. editor hosting a server, tutorial exiting to the main menu etc.
	- Keeping the separately running game thread that continuously produces frames to be rendered on the main thread.
	- Inputs being propagated to either gui or gameplay.
	- Running the main loop:
		- waiting on a game thread to produce a frame,
		- calling GL commands on the main thread,
		- finally swapping window buffers.

	It would be extremely counterproductive to separate it into many files because the above tasks are inherently interconnected.
	I believe it's the simplest to just have everything that relates to the main game loop in a single function: "work".
*/

#if PLATFORM_UNIX
#include <csignal>
#endif

#include <functional>

#include "fp_consistency_tests.h"

#include "augs/log_path_getters.h"
#include "augs/unit_tests.h"
#include "augs/global_libraries.h"

#include "augs/templates/identity_templates.h"
#include "augs/templates/container_templates.h"
#include "augs/templates/history.hpp"
#include "augs/templates/traits/in_place.h"
#include "augs/templates/thread_pool.h"

#include "augs/filesystem/file.h"
#include "augs/filesystem/directory.h"

#include "augs/misc/time_utils.h"
#include "augs/misc/imgui/imgui_utils.h"
#include "augs/misc/lua/lua_utils.h"

#include "augs/graphics/renderer.h"
#include "augs/graphics/renderer_backend.h"

#include "augs/window_framework/shell.h"
#include "augs/window_framework/window.h"
#include "augs/window_framework/platform_utils.h"
#include "augs/audio/audio_context.h"
#include "augs/audio/audio_command_buffers.h"
#include "augs/drawing/drawing.hpp"

#include "game/organization/all_component_includes.h"
#include "game/organization/all_messages_includes.h"
#include "game/detail/inventory/inventory_slot_handle.h"
#include "game/detail/entity_handle_mixins/inventory_mixin.hpp"

#include "game/cosmos/data_living_one_step.h"
#include "game/cosmos/cosmos.h"

#include "view/game_gui/game_gui_system.h"

#include "view/audiovisual_state/world_camera.h"
#include "view/audiovisual_state/audiovisual_state.h"
#include "view/rendering_scripts/illuminated_rendering.h"
#include "view/viewables/images_in_atlas_map.h"
#include "view/viewables/streaming/viewables_streaming.h"
#include "view/frame_profiler.h"
#include "view/shader_paths.h"

#include "application/session_profiler.h"
#include "application/config_lua_table.h"

#include "application/gui/settings_gui.h"
#include "application/gui/start_client_gui.h"
#include "application/gui/start_server_gui.h"
#include "application/gui/browse_servers_gui.h"
#include "application/gui/map_catalogue_gui.h"
#include "application/gui/ingame_menu_gui.h"

#include "application/masterserver/masterserver.h"

#include "application/network/network_common.h"
#include "application/setups/all_setups.h"

#include "application/setups/editor/editor_paths.h"

#include "application/main/imgui_pass.h"
#include "application/main/draw_debug_details.h"
#include "application/main/draw_debug_lines.h"
#include "application/main/release_flags.h"
#include "application/main/flash_afterimage.h"
#include "application/main/abortable_popup.h"
#if BUILD_DEBUGGER_SETUP
#include "application/setups/debugger/debugger_player.hpp"
#endif

#include "application/nat/nat_detection_session.h"
#include "application/nat/nat_traversal_session.h"
#include "application/input/input_pass_result.h"
#include "application/main/nat_traversal_details_window.h"

#include "application/setups/draw_setup_gui_input.h"
#include "application/network/resolve_address.h"
#include "augs/network/netcode_socket_raii.h"

#include "cmd_line_params.h"
#include "build_info.h"

#include "augs/readwrite/byte_readwrite.h"
#include "view/game_gui/special_indicator_logic.h"
#include "augs/window_framework/create_process.h"
#include "augs/misc/imgui/simple_popup.h"
#include "application/main/game_frame_buffer.h"
#include "application/main/cached_visibility_data.h"
#include "augs/graphics/frame_num_type.h"
#include "view/rendering_scripts/launch_visibility_jobs.h"
#include "view/rendering_scripts/for_each_vis_request.h"
#include "view/hud_messages/hud_messages_gui.h"
#include "game/cosmos/for_each_entity.h"
#include "application/setups/client/demo_paths.h"
#include "application/nat/stun_server_provider.h"
#include "application/arena/arena_paths.h"

#include "application/setups/editor/editor_setup_for_each_highlight.hpp"

#include "application/main/self_updater.h"
#include "application/setups/editor/packaged_official_content.h"
#include "augs/string/parse_url.h"
#include "steam_integration.h"
#include "steam_integration_helpers.hpp"
#include "steam_integration_callbacks.h"

#include "work_result.h"

std::function<void()> ensure_handler;

extern std::mutex log_mutex;
extern std::string log_timestamp_format;

extern float max_zoom_out_at_edges_v;

#if PLATFORM_UNIX
std::atomic<int> signal_status = 0;
static_assert(std::atomic<int>::is_always_lock_free);
#endif

constexpr bool no_edge_zoomout_v = false;

work_result work(
	const cmd_line_params& parsed_params,
	const bool log_directory_existed,
	const int argc,
	const char* const * const argv
) try {
	const auto& params = parsed_params;

	const bool is_cli_tool = params.is_cli_tool();

	if (is_cli_tool) {
		LOG("Launching a CLI tool. Skipping Steam API initialization.");
		LOG("WARNING: CLI tools will always read \"user\",\n instead of the folder named after the SteamID");
	}
	else {
		const bool running_from_appimage = !params.appimage_path.empty();

		if (!running_from_appimage) {
			/* 
				Creating steam_appid.txt will fail on a read-only system.
			*/

#if CREATE_STEAM_APPID
			LOG("Creating steam_appid.txt");

			augs::save_as_text("steam_appid.txt", std::to_string(::steam_get_appid()));
#elif !IS_PRODUCTION_BUILD
			LOG("Removing steam_appid.txt");

			augs::remove_file("steam_appid.txt");
#endif
		}

		if (::steam_restart()) {
			return work_result::STEAM_RESTART;
		}
	}

	const auto steam_status = 
		is_cli_tool ?
		steam_init_result::DISABLED : 
		steam_init_result(::steam_init())
	;

	if (steam_status == steam_init_result::FAILURE) {
		LOG("Failed to init Steam API!");
		return work_result::FAILURE;
	}

	const bool steam_initialized = steam_status == steam_init_result::SUCCESS;

	auto deinit = augs::scope_guard([steam_initialized]() {
		if (steam_initialized) {
			::steam_deinit();
		}
	});

	const bool is_steam_client = steam_initialized;

	LOG_NVPS(is_steam_client);

	uint32_t steam_auth_request_id = 0;

	if (is_steam_client) {
		const auto steam_id = std::to_string(::steam_get_id());
		::USER_DIR = APPDATA_DIR / steam_id;
	}
	else {
		::USER_DIR = APPDATA_DIR / NONSTEAM_USER_FOLDER_NAME;
	}

	/* 
		Just use the "user" folder when developing.
		CREATE_STEAM_APPID means we're still not really in "true" production.
		Otherwise the numbered user folder will show up in our git as we can't easily ignore it.
	*/

#if !IS_PRODUCTION_BUILD || CREATE_STEAM_APPID
	::USER_DIR = APPDATA_DIR / NONSTEAM_USER_FOLDER_NAME;
#endif

#if PLATFORM_UNIX	
	auto signal_handler = [](const int signal_type) {
   		signal_status = signal_type;
	};

	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGSTOP, signal_handler);
#endif

	setup_float_flags();

	{
		const auto all_created_directories = std::vector<augs::path_type> {
			CACHE_DIR,
			USER_DIR,
			DEMOS_DIR,

			DOWNLOADED_ARENAS_DIR,
			EDITOR_PROJECTS_DIR
		};

		LOG("Creating directories:");

		for (const auto& a : all_created_directories) {
			LOG("%x", a);
			augs::create_directories(a);
		}
	}

	const auto canon_config_path = augs::path_type("default_config.lua");
	const auto local_config_path = USER_DIR / "config.lua";
	const auto force_config_path = USER_DIR / "config.force.lua";
	const auto private_config_path = USER_DIR / "config.private.lua";

	LOG("Creating lua state.");
	auto lua = augs::create_lua_state();

	LOG("Loading the config.");

	const auto canon_config_ptr = [&]() {
		auto result_ptr = std::make_unique<config_lua_table>(
			lua,
			canon_config_path
		);

		auto& result = *result_ptr;

#if !IS_PRODUCTION_BUILD
		/* Some developer-friendly options */
		result.http_client.update_on_launch = false;
		result.client.suppress_webhooks = true;
		result.server.suppress_new_community_server_webhook = true;
		result.unit_tests.run = true;

#if PLATFORM_UNIX
		result.window.fullscreen = false;
#endif
#endif

		/* Tweak performance defaults */

		const auto concurrency = std::thread::hardware_concurrency();

		if (concurrency <= 4) {
			result.audio.enable_hrtf = false;
			result.window.max_fps.value = 60;
		}

#if !NDEBUG
		result.audio.enable_hrtf = false;
#endif

#if PLATFORM_LINUX
		/* 
			We don't yet properly implement detecting whether the app is in fs or not. 
			So better use the system cursor so that we don't clip it accidentally. 
		*/

		result.window.draw_own_cursor_in_fullscreen = false;
#else
		result.window.draw_own_cursor_in_fullscreen = true;
#endif

#if PLATFORM_MACOS
		result.window.fullscreen = true;
#endif

#if PLATFORM_WINDOWS
		result.drawing.stencil_before_light_pass = true;
#else
		result.drawing.stencil_before_light_pass = false;
#endif

		return result_ptr;
	}();

	auto& canon_config = *canon_config_ptr;

	auto config_ptr = [&]() {
		auto result = std::make_unique<config_lua_table>(canon_config);

		if (augs::exists(local_config_path)) {
			result->load_patch(lua, local_config_path);
		}

		if (!params.apply_config.empty()) {
			const auto cli_config_path = CALLING_CWD / params.apply_config;
			result->load_patch(lua, cli_config_path);
		}

		if (augs::exists(force_config_path)) {
			result->load_patch(lua, force_config_path);
		}

		if (augs::exists(private_config_path)) {
			result->load_patch(lua, private_config_path);
		}

		if (params.no_router) {
			result->server.allow_nat_traversal = false;
		}

		if (params.daily_autoupdate) {
			result->server.daily_autoupdate = true;
		}

		if (params.autoupdate_delay.has_value()) {
			result->server.autoupdate_delay = *params.autoupdate_delay;
		}

		if (result->client.nickname.empty()) {
			result->client.nickname = augs::get_user_name();
		}

		if (is_steam_client) {
			if (result->client.use_account_nickname) {
				if (const auto steam_username = ::steam_get_username()) {
					result->client.nickname = std::string(steam_username);
				}
			}

			if (result->client.use_account_avatar) {
				if (const auto avatar = ::steam_get_avatar_image(); avatar.get_size().is_nonzero()) {
					const auto cached_file_path = USER_DIR / "cached_avatar.png";
					avatar.save_as_png(cached_file_path);

					result->client.avatar_image_path = cached_file_path;
				}
			}
		}

		return result;
	}();

	LOG("Loaded user configs.");

	auto& config = *config_ptr;

	{
		std::unique_lock<std::mutex> lock(log_mutex);
		::log_timestamp_format = config.log_timestamp_format;
	}

	const auto fp_test_settings = [&]() {
		auto result = config.float_consistency_test;

		if (params.test_fp_consistency != -1) {
			result.passes = params.test_fp_consistency;
			LOG("Forcing %x fp consistency passes.", params.test_fp_consistency);
		}

		if (!params.consistency_report.empty()) {
			result.report_filename = params.consistency_report;
		}

		return result;
	}();	

	const auto float_tests_succeeded = 
		perform_float_consistency_tests(fp_test_settings)
	;

	LOG("Initializing network RAII.");

	auto network_raii = augs::network_raii();

	if (config.unit_tests.run || params.unit_tests_only) {
		/* Needed by some unit tests */

		LOG("Running unit tests.");
		augs::run_unit_tests(config.unit_tests);

		LOG("All unit tests have passed.");

		if (params.unit_tests_only) {
			return work_result::SUCCESS;
		}
	}
	else {
		LOG("Unit tests were disabled.");
	}

	LOG("Initializing ImGui.");

	const auto imgui_ini_path = (USER_DIR / (get_preffix_for(current_app_type) + "imgui.ini")).string();
	const auto imgui_log_path = get_path_in_log_files("imgui_log.txt");

	const auto imgui_raii = augs::imgui::context_raii(
		imgui_ini_path.c_str(),
		imgui_log_path.c_str(),
		config.gui_style
	);

	LOG("Creating the ImGui atlas image.");
	const auto imgui_atlas_image = std::make_unique<augs::image>(augs::imgui::create_atlas_image(config.gui_fonts.gui));

	auto last_update_result = self_update_result();

	const bool should_run_self_updater = 
		params.update_once_now ||
		params.only_check_update_availability_and_quit ||
		(config.http_client.update_on_launch && !params.no_update_on_launch)
	;

	if (should_run_self_updater) {
		using update_result = self_update_result_type;

		LOG("Checking for updates");

		const bool should_update_headless = is_cli_tool;

		LOG_NVPS(should_update_headless);

		const bool only_check = is_steam_client || params.only_check_update_availability_and_quit;

		last_update_result = check_and_apply_updates(
			params.appimage_path,
			only_check,
			*imgui_atlas_image,
			config.http_client,
			config.window,
			should_update_headless
		);

		LOG_NVPS(last_update_result.type);

		if (is_steam_client) {
			LOG("No need to run updater as this is a Steam client.");
		}
		else {
			if (params.only_check_update_availability_and_quit) {
				if (last_update_result.type == update_result::UPDATE_AVAILABLE) {
					return work_result::REPORT_UPDATE_AVAILABLE;
				}
				else {
					return work_result::REPORT_UPDATE_UNAVAILABLE;
				}
			}

			if (last_update_result.type == update_result::UPGRADED) {
				LOG("work: Upgraded successfully. Requesting relaunch.");
				return work_result::RELAUNCH_UPGRADED;
			}

			if (last_update_result.type == update_result::EXIT_APPLICATION) {
				return work_result::SUCCESS;
			}

			if (last_update_result.exit_with_failure_if_not_upgraded) {
				return work_result::FAILURE;
			}

			if (last_update_result.type == update_result::UP_TO_DATE) {
				if (params.upgraded_successfully) {
					last_update_result.type = update_result::FIRST_LAUNCH_AFTER_UPGRADE;
				}
			}
		}
	}
	else {
		if (params.no_update_on_launch) {
			LOG("Skipping update check due to --no-update-on-launch flag.");
		}
		else {
			LOG("Skipping update check due to update_on_launch = false.");
		}
	}

	augs::timer until_first_swap;
	bool until_first_swap_measured = false;

	session_profiler render_thread_performance;
	network_profiler network_performance;
	network_info network_stats;
	server_network_info server_stats;

	dump_detailed_sizeof_information(get_path_in_log_files("detailed_sizeofs.txt"));

	auto last_saved_config_ptr = std::make_unique<config_lua_table>(config);
	auto& last_saved_config = *last_saved_config_ptr;

	auto change_with_save = [&](auto setter) {
		setter(config);
		setter(last_saved_config);

		last_saved_config.save_patch(lua, canon_config, local_config_path);
	};

	auto failed_to_load_arena_popup = std::optional<simple_popup>();
	auto last_exit_incorrect_popup = std::optional<simple_popup>();

	auto perform_failed_to_load_arena_popup = [&]() {
		if (failed_to_load_arena_popup.has_value()) {
			if (failed_to_load_arena_popup->perform()) {
				failed_to_load_arena_popup = std::nullopt;
			}
		}
	};

	auto perform_last_exit_incorrect = [&]() {
		if (last_exit_incorrect_popup.has_value()) {
			if (last_exit_incorrect_popup->perform()) {
				last_exit_incorrect_popup = std::nullopt;
			}
		}
	};

#if IS_PRODUCTION_BUILD
	if (log_directory_existed) {
		if (const auto last_failure_log = find_last_incorrect_exit()) {
			change_with_save([&](config_lua_table& cfg) {
				cfg.last_activity = activity_type::MAIN_MENU;
			});

			const auto notice_pre_content = "Looks like the game has crashed since the last time.\n\n";

			const auto notice_content = last_failure_log == "" ? 
				"It was most likely a segmentation fault.\n"
#if PLATFORM_UNIX
				"Consider sending developers the core file generated upon crash.\n\n"
#else
				"Consider contacting developers about the problem,\n"
				"describing exactly the game's behavior prior to the crash.\n\n"
#endif
				:

				"Consider sending developers the log file located at:\n"
				"%x\n\n"
			;

			const auto notice_post_content =
				"If you experience repeated crashes,\n"
				"you might try resetting all your settings,\n"
				"which can be done by pressing \"Reset to factory default\" in Settings->General,\n"
				"and then hitting Save settings."
			;

			const auto full_content = 
				notice_pre_content 
				+ typesafe_sprintf(notice_content, *last_failure_log) 
				+ notice_post_content
			;

			last_exit_incorrect_popup = simple_popup { "Warning", full_content, "" };
		}
	}
#else
	(void)log_directory_existed;
#endif

	augs::remove_file(get_crashed_controllably_path());
	augs::remove_file(get_exit_success_path());

	LOG("Initializing freetype");

	auto freetype_library = std::optional<augs::freetype_raii>();

	if (params.type == app_type::GAME_CLIENT) {
		freetype_library.emplace();
	}

	if (params.type == app_type::MASTERSERVER) {
		auto adjusted_config = config;
		auto& masterserver = adjusted_config.masterserver;

		if (params.first_udp_command_port.has_value()) {
			masterserver.first_udp_command_port = *params.first_udp_command_port;
		}

		if (params.server_list_port.has_value()) {
			masterserver.server_list_port = *params.server_list_port;
		}

		LOG(
			"Starting the masterserver at ports: %x (Server list), %x-%x (UDP commands ports)",
			masterserver.server_list_port,
			masterserver.first_udp_command_port,
			masterserver.get_last_udp_command_port()
		);

		perform_masterserver(adjusted_config);

		return work_result::SUCCESS;
	}

	auto chosen_server_port = [&](){
		if (params.server_port.has_value()) {
			return *params.server_port;
		}

		return config.server_start.port;
	};

	auto chosen_server_nat = nat_detection_result();

	auto auxiliary_socket = std::optional<netcode_socket_raii>();

	auto get_bound_local_port = [&]() {
		return auxiliary_socket ? auxiliary_socket->socket.address.port : 0;
	};

	auto last_requested_local_port = port_type(0);

	auto recreate_auxiliary_socket = [&](std::optional<port_type> temporary_port = std::nullopt) {
		const auto preferred_port = temporary_port.has_value() ? *temporary_port : chosen_server_port();
		last_requested_local_port = preferred_port;

		try {
			auxiliary_socket.emplace(preferred_port);

			LOG("Successfully bound the nat detection socket to the preferred server port: %x.", preferred_port);
		}
		catch (const netcode_socket_raii_error&) {
			LOG("WARNING! Could not bind the nat detection socket to the preferred server port: %x.", preferred_port);

			auxiliary_socket.reset();
			auxiliary_socket.emplace();
		}

		LOG_NVPS(get_bound_local_port());
		ensure(get_bound_local_port() != 0);
	};

	recreate_auxiliary_socket();

	auto pending_launch = std::optional<activity_type>();

	auto stun_provider = stun_server_provider(config.nat_detection.stun_server_list);

	auto nat_detection = std::optional<nat_detection_session>();
	auto nat_detection_popup = abortable_popup_state();

	auto nat_detection_complete = [&]() {
		if (nat_detection == std::nullopt) {
			return false;
		}

		return nat_detection->query_result().has_value();
	};

	auto restart_nat_detection = [&]() {
		nat_detection.reset();
		nat_detection.emplace(config.nat_detection, stun_provider);
	};

	auto get_detected_nat = [&]() {
		if (nat_detection == std::nullopt) {
			return nat_detection_result();
		}

		if (const auto detected_nat = nat_detection->query_result()) {
			return *detected_nat;
		}

		return nat_detection_result();
	};

	restart_nat_detection();

	auto nat_traversal = std::optional<nat_traversal_session>();
	auto nat_traversal_details = nat_traversal_details_window();

	auto do_traversal_details_popup = [&](auto& window) {
		if (const bool aborted = nat_traversal_details.perform(window, get_bound_local_port(), nat_traversal)) {
			nat_traversal.reset();
			pending_launch = std::nullopt;

			if (chosen_server_port() == 0) {
				const auto next_port = get_bound_local_port();
				recreate_auxiliary_socket(next_port + 1);
			}
		}
	};

	auto do_detection_details_popup = [&]() {
		const auto message = 
			typesafe_sprintf("NAT detection for port %x is in progress...\nPlease be patient.", get_bound_local_port())
		;

		const bool should_be_open = 
			pending_launch.has_value() 
			&& !nat_detection_complete()
		;

		const auto title = "Launching setup...";

		if (const bool aborted = nat_detection_popup.perform(should_be_open, title, message)) {
			pending_launch = std::nullopt;
		}
	};

	auto make_server_nat_traversal_input = [&]() {
		return server_nat_traversal_input {
			config.nat_detection,
			config.nat_traversal,

			stun_provider
		};
	};

	const auto official = std::make_unique<packaged_official_content>(lua);

	auto handle_sigint = [&]() {
#if PLATFORM_UNIX
		if (signal_status != 0) {
			const auto sig = signal_status.load();

			LOG("%x received.", strsignal(sig));

			if(
				sig == SIGINT
				|| sig == SIGSTOP
				|| sig == SIGTERM
			) {
				LOG("Gracefully shutting down.");
				return true;
			}
		}
#endif

		return false;

	};

	{
		bool sync = false;
		bool quit_after_sync = false;

		if (config.server.sync_all_external_arenas_on_startup) {
			LOG("sync_all_external_arenas_on_startup specified.");
			sync = true;
		}
		else if (params.sync_external_arenas) {
			LOG("--sync-external-arenas specified.");
			sync = true;
		}
		else if (params.sync_external_arenas_and_quit) {
			LOG("--sync-external-arenas-and-quit specified.");
			sync = true;
			quit_after_sync = true;
		}

		if (sync) {
			const auto& provider = config.server.external_arena_files_provider;

			if (const auto parsed = parsed_url(provider); parsed.valid()) {
				LOG("External arena provider: %x", provider);

				headless_map_catalogue headless;

				augs::timer last_report;

				auto report_progress = [&]() {
					if (const auto& dl = headless.get_downloading()) {
						if (const auto current = dl->get_current_map_name()) {
							const auto i = dl->get_current_map_index();
							const auto total = dl->get_total_maps();

							LOG(
								"Syncing %x (%x/%x): %2f% complete. (Total: %2f%)", 
								*current, i, total,
								100 * dl->get_current_map_progress(),
								100 * dl->get_total_progress()
							);
						}
					}
				};

				while (true) {
					if (headless.advance({ provider }) == headless_catalogue_result::LIST_REFRESH_COMPLETE) {
						const auto last_error = headless.get_list_catalogue_error();

						if (last_error.size() > 0) {
							LOG("Failed to download the catalogue. Reason:\n%x", last_error);
							break;
						}
						else {
							const auto all = headless.launch_download_all({ provider });

							if (all.empty()) {
								LOG("All arenas are up to date. There's nothing to sync.");
								break;
							}

							std::string all_report;

							for (const auto& a : all) {
								all_report += a.name + ", ";
							}

							if (all_report.size() >= 2) {
								all_report.pop_back();
								all_report.pop_back();
							}

							LOG("Syncing %x arena(s): %x", all.size(), all_report);
						}
					}

					if (last_report.get<std::chrono::seconds>() > 0.5) {
						last_report.reset();
						report_progress();
					}

					if (headless.finalize_download()) {
						LOG("Finished syncing all arenas.");
						break;
					}

					if (handle_sigint()) {
						return work_result::SUCCESS;
					}

					yojimbo_sleep(1.0 / 1000);
				}
			}
			else {
				LOG("Couldn't parse arena provider URL: \"%x\". Aborting sync.", provider);
			}

			if (quit_after_sync) {
				return work_result::SUCCESS;
			}
		}
	}

	if (params.type == app_type::DEDICATED_SERVER) {
		LOG("Starting the dedicated server at port: %x", chosen_server_port());

		if (config.server.allow_nat_traversal) {
			if (nat_detection.has_value()) {
				if (auxiliary_socket.has_value()) {
					LOG("Waiting for NAT detection to complete...");

					while (!nat_detection_complete()) {
						nat_detection->advance(auxiliary_socket->socket);

						if (handle_sigint()) {
							return work_result::SUCCESS;
						}

						yojimbo_sleep(1.0 / 1000);
					}
				}
			}
		}

		const auto bound_port = get_bound_local_port();
		auxiliary_socket.reset();

		LOG("Starting a dedicated server. Binding to a port: %x (%x was preferred)", bound_port, last_requested_local_port);

		auto start = config.server_start;
		start.port = bound_port;

#if BUILD_NETWORKING
		auto server_ptr = std::make_unique<server_setup>(
			lua,
			*official,
			start,
			config.server,
			config.server_private,
			config.client,
			config.dedicated_server,

			make_server_nat_traversal_input(),
			params.suppress_server_webhook
		);

		auto& server = *server_ptr;

		std::future<self_update_result> availability_check;

		auto write_vars_to_disk = [&]() {
			change_with_save([&](auto& cfg) {
				cfg.server = server.get_current_vars();
			});
		};

		auto save_config = augs::scope_guard([&]() {
			write_vars_to_disk();
		});

		while (server.is_running()) {
			const auto zoom = 1.f;

			if (handle_sigint()) {
				return work_result::SUCCESS;
			}

			server.advance(
				{
					vec2i(),
					config.input,
					zoom,
					get_detected_nat(),
					network_performance,
					server_stats
				},
				solver_callbacks()
			);

			if (server.should_write_vars_to_disk_once()) {
				write_vars_to_disk();
			}

			if (server.should_check_for_updates_once()) {
				LOG("Launching an async check for updates.");

				auto config_http_client = config.http_client;

				/* Give it a little longer, it's async anyway. */
				config_http_client.update_connection_timeout_secs = 10;

				const auto config_window = config.window;

				availability_check = launch_async(
					[&imgui_atlas_image, params, config_http_client, config_window]() {
						const bool only_check_update_availability_and_quit = true;

						return check_and_apply_updates(
							params.appimage_path,
							only_check_update_availability_and_quit,
							*imgui_atlas_image,
							config_http_client,
							config_window,
							true // should_update_headless
						);
					}
				);
			}

			if (valid_and_is_ready(availability_check)) {
				LOG("Finished the async check for updates.");

				using update_result = self_update_result_type;

				const auto result = availability_check.get();

				if (result.type == update_result::UPDATE_AVAILABLE) {
					return work_result::RELAUNCH_AND_UPDATE_DEDICATED_SERVER;
				}
				else {
					LOG("The dedicated server is up to date.");
				}
			}

			server.sleep_until_next_tick();
		}
#endif

		if (server.server_restart_requested()) {
			return work_result::RELAUNCH_DEDICATED_SERVER;
		}

		return work_result::SUCCESS;
	}

	LOG("Initializing the audio context.");

	augs::audio_context audio(config.audio);

	LOG("Logging all audio devices.");
	augs::log_all_audio_devices(get_path_in_log_files("audio_devices.txt"));

	auto thread_pool = augs::thread_pool(config.performance.get_num_pool_workers());

	augs::audio_command_buffers audio_buffers(thread_pool);

	LOG("Initializing the window.");
	augs::window window(config.window);

	LOG("Initializing the renderer backend.");
	augs::graphics::renderer_backend renderer_backend;

	game_frame_buffer_swapper buffer_swapper;

	auto get_read_buffer = [&]() -> game_frame_buffer& {
		return buffer_swapper.get_read_buffer();
	};

	auto get_write_buffer = [&]() -> game_frame_buffer& {
		return buffer_swapper.get_write_buffer();
	};

	get_write_buffer().screen_size = window.get_screen_size();
	get_read_buffer().new_settings = config.window;
	get_read_buffer().swap_when = config.performance.swap_window_buffers_when;

	auto logic_get_screen_size = [&]() {
		return get_write_buffer().screen_size;
	};

	auto get_general_renderer = [&]() -> augs::renderer& {
		return get_write_buffer().renderers.all[renderer_type::GENERAL];
	};

	LOG_NVPS(renderer_backend.get_max_texture_size());

	LOG("Initializing the necessary fbos.");
	all_necessary_fbos necessary_fbos(
		logic_get_screen_size(),
		config.drawing
	);

	LOG("Initializing the necessary shaders.");
	all_necessary_shaders necessary_shaders(
		get_general_renderer(),
		CANON_SHADER_FOLDER,
		LOCAL_SHADER_FOLDER,
		config.drawing
	);

	LOG("Initializing the necessary sounds.");
	all_necessary_sounds necessary_sounds(
		"content/sfx/necessary"
	);

	LOG("Initializing the necessary image definitions.");
	const necessary_image_definitions_map necessary_image_definitions(
		lua,
		"content/gfx/necessary",
		config.content_regeneration.regenerate_every_time
	);

	LOG("Creating the ImGui atlas.");
	auto imgui_atlas = augs::graphics::texture(*imgui_atlas_image);

	const auto configurables = configuration_subscribers {
		window,
		necessary_fbos,
		audio,
		get_general_renderer()
	};

	atlas_profiler atlas_performance;
	frame_profiler game_thread_performance;

	/* 
		unique_ptr is used to avoid stack overflow.

		Main menu setup state may be preserved, 
		therefore it resides in a separate unique_ptr.
	*/

	std::unique_ptr<main_menu_setup> main_menu;
	main_menu_gui main_menu_gui;

	auto has_main_menu = [&]() {
		return main_menu != nullptr;
	};

	settings_gui_state settings_gui = std::string("Settings");
	start_client_gui_state start_client_gui = std::string("Connect to server");
	start_server_gui_state start_server_gui = std::string("Host a server");

	start_client_gui.is_steam_client = is_steam_client;
	start_server_gui.is_steam_client = is_steam_client;

	bool was_browser_open_in_main_menu = false;
	browse_servers_gui_state browse_servers_gui = std::string("Browse servers");
	map_catalogue_gui_state map_catalogue_gui = std::string("Download maps");
	std::string displayed_connecting_server_name;

	auto emplace_main_menu = [&map_catalogue_gui, &p = main_menu] (auto&&... args) {
		if (p == nullptr) {
			p = std::make_unique<main_menu_setup>(std::forward<decltype(args)>(args)...);
			map_catalogue_gui.rebuild_miniatures();
		}
	};

	auto find_chosen_server_info = [&]() {
		return browse_servers_gui.find_entry(config.client_connect);
	};

	ingame_menu_gui ingame_menu;

	/*
		Runtime representations of viewables,
		loaded from the definitions provided by the current setup.
		The setup's chosen viewables_loading_type decides if they are 
		loaded just once or if they are for example continuously streamed.
	*/

	LOG("Initializing the streaming of viewables.");

	auto streaming_ptr = std::make_unique<viewables_streaming>();
	viewables_streaming& streaming = *streaming_ptr;

	auto get_blank_texture = [&]() {
		return streaming.necessary_images_in_atlas[assets::necessary_image_id::BLANK];
	};

	auto get_drawer_for = [&](augs::renderer& chosen_renderer) { 
		return augs::drawer_with_default {
			chosen_renderer.get_triangle_buffer(),
			get_blank_texture()
		};
	};

	world_camera gameplay_camera;
	LOG("Initializing the audiovisual state.");
	auto audiovisuals = std::make_unique<audiovisual_state>();

	auto get_audiovisuals = [&]() -> audiovisual_state& {
		return *audiovisuals;
	};


	/*
		The lambdas that aid to make the main loop code more concise.
	*/	

	std::unique_ptr<setup_variant> current_setup = nullptr;
	std::unique_ptr<setup_variant> background_setup = nullptr;

	auto has_current_setup = [&]() {
		return current_setup != nullptr;
	};

	auto is_during_tutorial = [&]() {
		if (has_current_setup()) {
			if (const auto setup = std::get_if<test_scene_setup>(std::addressof(*current_setup))) {
				return setup->is_tutorial();
			}
		}

		return false;
	};

	auto restore_background_setup = [&]() {
		current_setup = std::move(background_setup);
		background_setup = std::unique_ptr<setup_variant>();

		ingame_menu.show = false;
	};

	auto visit_current_setup = [&](auto callback) -> decltype(auto) {
		if (has_current_setup()) {
			return std::visit(
				[&](auto& setup) -> decltype(auto) {
					return callback(setup);
				}, 
				*current_setup
			);
		}
		else {
			return callback(*main_menu);
		}
	};

	auto setup_requires_cursor = [&]() {
		return visit_current_setup([&](const auto& s) {
			return s.requires_cursor();
		});
	};

	auto get_interpolation_ratio = [&]() {
		return visit_current_setup([&](auto& setup) {
			return setup.get_interpolation_ratio();
		});
	};

	auto on_specific_setup = [&](auto callback) -> decltype(auto) {
		using T = remove_cref<argument_t<decltype(callback), 0>>;

		if constexpr(std::is_same_v<T, main_menu_setup>) {
			if (has_main_menu()) {
				callback(*main_menu);
			}
		}
		else {
			if (has_current_setup()) {
				if (auto* setup = std::get_if<T>(&*current_setup)) {
					callback(*setup);
				}
			}
		}
	};

	auto get_unofficial_content_dir = [&]() {
		return visit_current_setup([&](const auto& s) { return s.get_unofficial_content_dir(); });
	};

	auto get_render_layer_filter = [&]() {
		return visit_current_setup([&](const auto& s) { return s.get_render_layer_filter(); });
	};

	/* TODO: We need to have one game gui per cosmos. */
	game_gui_system game_gui;
	bool game_gui_mode_flag = false;

	hud_messages_gui hud_messages;

	std::atomic<augs::frame_num_type> current_frame = 0;

	auto load_all = [&](const all_viewables_defs& new_defs) {
		const auto frame_num = current_frame.load();

		std::optional<arena_player_metas> new_player_metas;
		std::optional<ad_hoc_atlas_subjects> new_ad_hoc_images;

		if (streaming.avatars.work_slot_free(frame_num)) {
			visit_current_setup([&](auto& setup) {
				new_player_metas = setup.get_new_player_metas();
				new_ad_hoc_images = setup.get_new_ad_hoc_images();
			});

			if (!has_current_setup()) {
				new_ad_hoc_images = map_catalogue_gui.get_new_ad_hoc_images();
			}
		}

		streaming.load_all({
			frame_num,
			new_defs,
			necessary_image_definitions,
			config.gui_fonts,
			config.content_regeneration,
			get_unofficial_content_dir(),
			get_general_renderer(),
			renderer_backend.get_max_texture_size(),

			new_player_metas,
			new_ad_hoc_images
		});
	};

	bool set_rich_presence_now = true;

	auto setup_launcher = [&](auto&& setup_init_callback) {
		::steam_clear_rich_presence();
		set_rich_presence_now = true;
		
		get_audiovisuals().get<particles_simulation_system>().clear();
		
		game_gui_mode_flag = false;

		audio_buffers.finish();
		audio_buffers.stop_all_sources();

		get_audiovisuals().get<sound_system>().clear();

		network_stats = {};
		server_stats = {};

		if (main_menu != nullptr) {
			was_browser_open_in_main_menu = browse_servers_gui.show;
		}

		browse_servers_gui.close();
		map_catalogue_gui.close();
		settings_gui.close();

		main_menu.reset();
		current_setup.reset();
		ingame_menu.show = false;

		setup_init_callback();
		
		visit_current_setup([&](const auto& setup) {
			using T = remove_cref<decltype(setup)>;
			
			if constexpr(T::loading_strategy == viewables_loading_type::LOAD_ALL_ONLY_ONCE) {
				load_all(setup.get_viewable_defs());
			}
		});

		if (main_menu != nullptr) {
			if (was_browser_open_in_main_menu) {
				browse_servers_gui.open();
			}

			if (auxiliary_socket == std::nullopt) {
				recreate_auxiliary_socket();

				if (!nat_detection_complete()) {
					restart_nat_detection();
				}
			}
		}
	};

	auto emplace_current_setup = [&p = current_setup] (auto tag, auto&&... args) {
		using Tag = decltype(tag);
		using T = type_of_in_place_type_t<Tag>; 

		if (p == nullptr) {
			p = std::make_unique<setup_variant>(
				tag,
				std::forward<decltype(args)>(args)...
			);
		}
		else {
			p->emplace<T>(std::forward<decltype(args)>(args)...);
		}
	};

#if BUILD_DEBUGGER_SETUP
	auto launch_debugger = [&](auto&&... args) {
		setup_launcher([&]() {
			emplace_current_setup(std::in_place_type_t<debugger_setup>(),
				std::forward<decltype(args)>(args)...
			);
		});
	};
#else
	auto launch_debugger = [&](auto&&...) {

	};
#endif

	auto save_last_activity = [&](const activity_type mode) {
		change_with_save([mode](config_lua_table& cfg) {
			cfg.last_activity = mode;
		});
	};

	auto launch_main_menu = [&]() {
		if (!has_main_menu()) {
			main_menu_gui = {};
			
			setup_launcher([&]() {
				emplace_main_menu(lua, *official, config.main_menu);
			});

			map_catalogue_gui.request_refresh();
			streaming.recompress_demos();
		}
	};

	auto get_browse_servers_input = [&]() {
		return browse_servers_input {
			config.server_list_provider,
			config.client_connect,
			displayed_connecting_server_name,
			config.official_arena_servers,
			config.faction_view,
			config.streamer_mode && config.streamer_mode_flags.community_servers
		};
	};

	auto launch_client_setup = [&](
		const bool ignore_nat_check,

		/* 
			NAT traversal could make us connect to a different server port, 
			so for Steam Rich Presence to provide a correct Join Game server address,
			we need to keep track of what address we wanted to connect to originally.
		*/

		const std::optional<netcode_address_t> before_traversal_server_address = std::nullopt
	) {
		bool public_internet = false;

		streaming.wait_demos_compressed();

		if (ignore_nat_check) {
			LOG("Finished NAT traversal. Connecting immediately.");
		}
		else {
			if (const bool is_ip_address = find_netcode_addr(config.client_connect).has_value()) {
				if (auto info = find_chosen_server_info(); info == nullptr) {
					/* 
						If we're connecting to a specific IP address - as opposed to e.g. a domain -
						either via Steam API, CLI flag or relaunching with last remembered activity -
						we need to ask the server list to know if the server is behind NAT.
					*/

					browse_servers_gui.sync_download_server_entry(
						get_browse_servers_input(),
						config.client_connect
					);
				}
				else {
					LOG("Already have this server entry, no need to download info about this server.");
				}
			}

			if (auto info = find_chosen_server_info()) {
				LOG("Found the chosen server in the browser list.");

				if (info->heartbeat.is_behind_nat()) {
					LOG("The chosen server is behind NAT. Delaying the client launch until it is traversed.");

					chosen_server_nat = info->heartbeat.nat;
					pending_launch = activity_type::CLIENT;
					launch_main_menu();
					return false;
				}
				else {
					LOG("The chosen server is in the public internet. Connecting immediately.");
					public_internet = true;
				}
			}
			else {
				LOG("The chosen server was not found in the browser list.");
				public_internet = true;
			}
		}

		if (is_steam_client) {
			LOG("Calling steam_request_auth_ticket.");
			steam_auth_request_id = ::steam_request_auth_ticket("hypersomnia_gameserver");
			LOG_NVPS(steam_auth_request_id);
		}

		setup_launcher([&]() {
			auto bound_port = get_bound_local_port();
			auxiliary_socket.reset();

			if (public_internet) {
				LOG("Connecting without a specific port requirement.");
				bound_port = 0;
			}

			LOG("Starting client setup. Binding to a port: %x (%x was preferred)", bound_port, last_requested_local_port);

			emplace_current_setup(std::in_place_type_t<client_setup>(),
				lua,
				*official,
				config.client_connect,
				displayed_connecting_server_name,
				config.client,
				config.nat_detection,
				bound_port,
				before_traversal_server_address
			);
		});

		displayed_connecting_server_name.clear();

		save_last_activity(activity_type::CLIENT);

		return true;
	};

	auto launch_editor = [&](auto&&... args) {
		setup_launcher([&]() {
			emplace_current_setup(
				std::in_place_type_t<editor_setup>(),
				config.editor,
				*official,
				std::forward<decltype(args)>(args)...
			);
		});

		save_last_activity(activity_type::EDITOR);
	};

	auto launch_setup = [&](const activity_type mode) {
		if (mode != activity_type::TUTORIAL && !config.skip_tutorial) {
			change_with_save(
				[&](auto& cfg) {
					cfg.skip_tutorial = true;
				}
			);
		}

		if (map_catalogue_gui.is_downloading()) {
			LOG("Cannot launch %x. Download in progress.", augs::enum_to_string(mode));
			return;
		}

		LOG("Launched mode: %x", augs::enum_to_string(mode));

		switch (mode) {
			case activity_type::MAIN_MENU:
				launch_main_menu();
				break;

#if BUILD_NETWORKING
			case activity_type::CLIENT: {
				const bool ignore_nat_check = false;

				if (!launch_client_setup(ignore_nat_check)) {
					return;
				}

				break;
			}

			case activity_type::SERVER: {
				if (config.server.allow_nat_traversal) {
					if (!nat_detection_complete()) {
						LOG("NAT detection in progress. Delaying the server launch.");

						pending_launch = activity_type::SERVER;
						launch_main_menu();
						return;
					}
				}

				const auto bound_port = get_bound_local_port();
				auxiliary_socket.reset();

				LOG("Starting server setup. Binding to a port: %x (%x was preferred)", bound_port, last_requested_local_port);

				auto start = config.server_start;
				start.port = bound_port;

				setup_launcher([&]() {
					emplace_current_setup(std::in_place_type_t<server_setup>(),
						lua,
						*official,
						start,
						config.server,
						config.server_private,
						config.client,
						std::nullopt,

						make_server_nat_traversal_input(),
						params.suppress_server_webhook
					);
				});
#endif

				break;
			}

			case activity_type::DEBUGGER:
				launch_debugger(lua);

				break;

			case activity_type::EDITOR:
				if (!params.editor_target.empty()) {
					launch_editor(params.editor_target);
				}
				else {
					try {
						const auto loaded_path = augs::path_type(augs::file_to_string(get_editor_last_project_path()));

						try {
							launch_editor(loaded_path);
							break;
						}
						catch (const std::runtime_error& err) {
							const auto full_content = typesafe_sprintf("Failed to load: %x\nReason:\n\n%x", loaded_path, err.what());
							failed_to_load_arena_popup = simple_popup { "Error", full_content, "" };
						}
						catch (...) {

						}
					}
					catch (...) {
						LOG("No %x detected. Launching the project selector.", get_editor_last_project_path());
					}
				}

			case activity_type::EDITOR_PROJECT_SELECTOR:
				setup_launcher([&]() {
					emplace_current_setup(
						std::in_place_type_t<project_selector_setup>()
					);
				});

				break;

			case activity_type::SHOOTING_RANGE:
				setup_launcher([&]() {
					emplace_current_setup(std::in_place_type_t<test_scene_setup>(),
						config.client.nickname,
						*official,
						test_scene_type::SHOOTING_RANGE
					);
				});

				break;

			case activity_type::TUTORIAL:
				setup_launcher([&]() {
					emplace_current_setup(std::in_place_type_t<test_scene_setup>(),
						config.client.nickname,
						*official,
						test_scene_type::TUTORIAL
					);
				});

				break;

			default:
				ensure(false && "The launch_setup mode you have chosen is currently out of service.");
				break;
		}

		save_last_activity(mode);
	};

	auto finalize_pending_launch = [&](std::optional<netcode_address_t> before_addr = std::nullopt) {
		if (pending_launch == activity_type::CLIENT) {
			const bool ignore_nat_check = true;
			launch_client_setup(ignore_nat_check, before_addr);
		}
		else {
			launch_setup(*pending_launch);
		}

		pending_launch = std::nullopt;
	};

	auto next_nat_traversal_attempt = [&]() {
		const auto& server_nat = chosen_server_nat;
		const auto& client_connect = config.client_connect;
		const auto traversed_address = find_netcode_addr(client_connect);

		ensure(traversed_address.has_value());
		ensure(nat_detection_complete());

		const auto masterserver_address = nat_detection->get_resolved_port_probing_host();

		ensure(masterserver_address.has_value());

		nat_traversal_details.next_attempt();

		nat_traversal.reset();
		nat_traversal.emplace(nat_traversal_input {
			*masterserver_address,
			*traversed_address,

			get_detected_nat(),
			server_nat,
			config.nat_detection,
			config.nat_traversal
		}, stun_provider);
	};

	auto start_nat_traversal = [&]() {
		nat_traversal_details.reset();

		next_nat_traversal_attempt();
	};
	
	auto advance_nat_traversal = [&]() {
		if (nat_traversal_details.aborted) {
			return;
		}

		if (nat_traversal.has_value()) {
			if (auxiliary_socket.has_value()) {
				nat_traversal->advance(auxiliary_socket->socket);
			}
		}
	};

	auto check_nat_traversal_result = [&]() {
		const auto state = nat_traversal->get_current_state();

		if (state == nat_traversal_session::state::TRAVERSAL_COMPLETE) {
			const auto before_traversal_server_address = ::find_netcode_addr(config.client_connect);
			config.client_connect = ::ToString(nat_traversal->get_opened_address());
			nat_traversal.reset();

			finalize_pending_launch(before_traversal_server_address);
		}
		else if (state == nat_traversal_session::state::TIMED_OUT) {
			const auto next_port = get_bound_local_port();
			recreate_auxiliary_socket(next_port + 1);
			next_nat_traversal_attempt();
		}
	};

	bool client_start_requested = false;
	bool server_start_requested = false;

	auto start_client_setup = [&]() {
		change_with_save(
			[&](auto& cfg) {
				cfg.client_connect = config.client_connect;
				cfg.client = config.client;
			}
		);

		client_start_requested = false;

		launch_setup(activity_type::CLIENT);
	};

	auto get_map_catalogue_input = [&]() {
		return map_catalogue_input {
			config.server.external_arena_files_provider,
			streaming.ad_hoc.in_atlas,
			streaming.necessary_images_in_atlas,
			window,
			config.streamer_mode && config.streamer_mode_flags.map_catalogue
		};
	};

	auto perform_browse_servers = [&]() {
		const bool perform_result = browse_servers_gui.perform(get_browse_servers_input());

		if (perform_result) {
			start_client_setup();
		}
	};

	auto perform_map_catalogue = [&]() {
		const bool perform_result = map_catalogue_gui.perform(get_map_catalogue_input());

		if (perform_result) {
			change_with_save(
				[&](auto& cfg) {
					cfg.server.external_arena_files_provider = config.server.external_arena_files_provider;
				}
			);
		}

		if (map_catalogue_gui.open_host_server_window.has_value()) {
			config.server.arena = *map_catalogue_gui.open_host_server_window;
			start_server_gui.open();

			map_catalogue_gui.open_host_server_window = std::nullopt;
		}
	};

	auto perform_start_client_gui = [&](const auto frame_num) {
		const auto best_server = browse_servers_gui.find_best_ranked();

		const bool perform_result = start_client_gui.perform(
			best_server,
			browse_servers_gui.refresh_in_progress(),
			frame_num,
			get_general_renderer(), 
			streaming.avatar_preview_tex, 
			window, 
			config.client_connect, 
			displayed_connecting_server_name,
			config.client,
			config.official_arena_servers
		);

		if (perform_result || client_start_requested) {
			start_client_setup();
		}

		if (start_client_gui.request_server_list_open) {
			start_client_gui.request_server_list_open = false;

			browse_servers_gui.open();

			if (best_server != nullptr) {
				browse_servers_gui.select_server(*best_server);
			}
		}
	};

	auto perform_start_server_gui = [&]() {
		const bool launched_from_server_start_gui = start_server_gui.perform(
			config.server_start, 
			config.server, 
			nat_detection.has_value() ? std::addressof(*nat_detection) : nullptr,
			get_bound_local_port()
		);

		if (launched_from_server_start_gui || server_start_requested) {
			server_start_requested = false;

			change_with_save(
				[&](auto& cfg) {
					cfg.server_start = config.server_start;
					cfg.client = config.client;
					cfg.server = config.server;
				}
			);

			if (start_server_gui.instance_type == server_instance_type::INTEGRATED) {
				launch_setup(activity_type::SERVER);
			}
			else {
				const auto chosen_port = get_bound_local_port();
				LOG_NVPS(get_bound_local_port(), chosen_server_port());

				auxiliary_socket.reset();

				const auto cmd_line = typesafe_sprintf(
					"--dedicated-server --server-port %x",
					chosen_port
				);

				augs::restart_application(argc, argv, params.exe_path.string(), { cmd_line });
				displayed_connecting_server_name = "local dedicated server";

				const auto address_str = typesafe_sprintf("%x:%x", config.server_start.ip, chosen_port);

				LOG("Connecting to the launched dedicated server at: %x", address_str);

				config.client_connect = address_str;

				launch_setup(activity_type::CLIENT);
			}
		}
	};

	auto get_viewable_defs = [&]() -> const all_viewables_defs& {
		return visit_current_setup([&](auto& setup) -> const all_viewables_defs& {
			return setup.get_viewable_defs();
		});
	};

	auto create_game_gui_deps = [&](const config_lua_table& viewing_config) {
		return game_gui_context_dependencies {
			get_viewable_defs().image_definitions,
			streaming.images_in_atlas,
			streaming.necessary_images_in_atlas,
			streaming.get_loaded_gui_fonts().gui,
			get_audiovisuals().randomizing,
			viewing_config.game_gui,
			viewing_config.hotbar
		};
	};

	auto create_menu_context_deps = [&](const config_lua_table& viewing_config) {
		return menu_context_dependencies{
			streaming.necessary_images_in_atlas,
			streaming.get_loaded_gui_fonts().gui,
			necessary_sounds,
			viewing_config.audio_volume,
			background_setup != nullptr,
			has_current_setup() && std::holds_alternative<editor_setup>(*current_setup),
			is_during_tutorial()
		};
	};

	auto get_game_gui_subject = [&]() -> const_entity_handle {
		const auto& viewed_cosmos = visit_current_setup([&](auto& setup) -> const cosmos& {
			return setup.get_viewed_cosmos();
		});

		const auto gui_character_id = visit_current_setup([&](auto& setup) {
			return setup.get_game_gui_subject_id();
		});

		return viewed_cosmos[gui_character_id];
	};

	auto get_viewed_character = [&]() -> const_entity_handle {
		const auto& viewed_cosmos = visit_current_setup([&](auto& setup) -> const cosmos& {
			return setup.get_viewed_cosmos();
		});

		const auto viewed_character_id = visit_current_setup([&](auto& setup) {
			return setup.get_viewed_character_id();
		});

		return viewed_cosmos[viewed_character_id];
	};

	auto get_controlled_character = [&]() -> const_entity_handle {
		const auto& viewed_cosmos = visit_current_setup([&](auto& setup) -> const cosmos& {
			return setup.get_viewed_cosmos();
		});

		const auto controlled_character_id = visit_current_setup([&](auto& setup) {
			return setup.get_controlled_character_id();
		});

		return viewed_cosmos[controlled_character_id];
	};
		
	auto should_draw_game_gui = [&]() {
		{
			bool should = true;

#if BUILD_DEBUGGER_SETUP
			on_specific_setup([&](debugger_setup& setup) {
				if (!setup.anything_opened() || setup.is_editing_mode()) {
					should = false;
				}
			});
#endif

			if (has_main_menu() && !has_current_setup()) {
				should = false;
			}

			if (!should) {
				return false;
			}
		}

		const auto viewed = get_game_gui_subject();

		if (!viewed.alive()) {
			return false;
		}

		if (!viewed.has<components::item_slot_transfers>()) {
			return false;
		}

		return true;
	};

	auto get_logic_eye = [&](const bool with_edge_zoomout) {
		if (const auto custom = visit_current_setup(
			[](const auto& setup) { 
				return setup.find_current_camera_eye(); 
			}
		)) {
			return *custom;
		}

		if (get_viewed_character().dead()) {
			return camera_eye();
		}

		return gameplay_camera.get_current_eye(with_edge_zoomout);
	};


	auto get_camera_edge_zoomout_mult = [&]() {		
		return gameplay_camera.current_edge_zoomout_mult;
	};

	auto get_camera_requested_fov_expansion = [&]() {		
		auto result = 1.0f / get_logic_eye(false).zoom;

		if (get_camera_edge_zoomout_mult() > 0.001f) {
			result /= max_zoom_out_at_edges_v;
		}

		return result;
	};

	auto get_camera_eye = [&](const config_lua_table& viewing_config, bool with_edge_zoomout = true) {		
		auto logic_eye = get_logic_eye(with_edge_zoomout);

		const auto considered_fov_size = viewing_config.drawing.fog_of_war.size;
		const float target_visible_pixels_y = considered_fov_size.y;

		const bool should_zoom_to_fov = viewing_config.drawing.snap_zoom_to_fov_size;

		if (should_zoom_to_fov && target_visible_pixels_y != 0.0f) {
			const float screen_y = float(logic_get_screen_size().y);

			float closest_zoom_multiple = 1.0f;
			float closest_multiple_dist = std::numeric_limits<float>::max();

			for (int i = -10; i <= 10; ++i) {
				if (i == 0) {
					continue;
				}

				const auto mult = i < 0 ? (1.0f / (-i)) : float(i);
				const auto scaled = target_visible_pixels_y * mult;

				const auto dist = std::abs(scaled - screen_y);

				if (dist < closest_multiple_dist) {
					closest_multiple_dist = dist;
					closest_zoom_multiple = mult;
				}
			}

			const auto eps = viewing_config.drawing.snap_zoom_to_multiple_if_different_by_pixels;

			auto zoom_to_snap_to_fov = screen_y / target_visible_pixels_y;

			if (closest_multiple_dist <= eps) {
				zoom_to_snap_to_fov = closest_zoom_multiple;
			}

			logic_eye.zoom *= zoom_to_snap_to_fov;
		}

		logic_eye.zoom *= std::max(1.0f, viewing_config.drawing.custom_zoom);

		return logic_eye;
	};

	auto get_camera_cone = [&](const config_lua_table& viewing_config) {		
		return camera_cone(get_camera_eye(viewing_config), logic_get_screen_size());
	};

	auto get_nonzoomedout_visible_world_area = [&](const config_lua_table& viewing_config) {
		return vec2(logic_get_screen_size()) / get_camera_eye(viewing_config, no_edge_zoomout_v).zoom;
	};

	auto get_queried_cone = [&](const config_lua_table& viewing_config) {		
		const auto query_mult = viewing_config.session.camera_query_aabb_mult;

		const auto queried_cone = [&]() {
			auto c = get_camera_cone(viewing_config);
			c.eye.zoom /= query_mult;
			return c;
		}();

		return queried_cone;
	};

	auto get_setup_customized_config = [&]() {
		return visit_current_setup([&]<typename S>(S& setup) {
			auto config_copy = config;

			/*
				For example, the main menu might want to disable HUD or tune down the sound effects.
				Editor might want to change the window name to the current file.
			*/

			setup.customize_for_viewing(config_copy);
			setup.apply(config_copy);

			if constexpr(is_one_of_v<S, client_setup, server_setup>) {
				setup.apply_nonzoomedout_visible_world_area(get_nonzoomedout_visible_world_area(config_copy));
			}

			if (get_camera_eye(config_copy).zoom < 1.f) {
				/* Force linear filtering when zooming out */
				config_copy.renderer.default_filtering = augs::filtering_type::LINEAR;
			}

			if (config_copy.drawing.cinematic_mode) {
				auto& d = config_copy.drawing;

				d.draw_crosshairs = false;
				//d.draw_weapon_laser = false;
				d.draw_aabb_highlighter = false;
				d.draw_inventory = false;
				d.draw_hotbar = false;
				d.draw_area_markers.is_enabled = false;
				d.draw_callout_indicators.is_enabled = false;
				d.enemy_hud_mode = character_hud_type::NONE;
				d.draw_hp_bar = false;
				d.draw_cp_bar = false;
				d.draw_pe_bar = false;
				d.draw_character_status = false;
				d.draw_remaining_ammo = false;

				d.draw_offscreen_indicators = false;
				d.draw_offscreen_callouts = false;
				d.draw_nicknames = false;
				d.draw_small_health_bars = false;
				d.draw_health_numbers = false;
				//d.draw_damage_indicators = false;

				d.draw_teammate_indicators.is_enabled = false;
				d.draw_danger_indicators.is_enabled = false;
				d.draw_tactical_indicators.is_enabled = false;
				d.print_current_character_callout = false;
				d.show_danger_indicator_for_seconds = 0;
				d.show_death_indicator_for_seconds = 0;
				d.nickname_characters_for_offscreen_indicators = 0;
			}

			if (!game_gui_mode_flag) {
				config_copy.drawing.draw_inventory = false;
			}

			return config_copy;
		});
	};

	auto is_replaying_demo = [&]() {
		bool result = false;

		on_specific_setup([&](client_setup& setup) {
			result = setup.is_replaying();
		});

		return result;
	};

	augs::timer ad_hoc_animation_timer;

	auto perform_setup_custom_imgui = [&]() {
		/*
			The debugger setup might want to use IMGUI to create views of entities or resources,
			thus we ask the current setup for its custom ImGui logic.

			Similarly, client and server setups might want to perform ImGui for things like team selection.
		*/

		visit_current_setup([&](auto& setup) {
			streaming.ad_hoc.in_atlas.animation_time = ad_hoc_animation_timer.get<std::chrono::seconds>();

			const auto result = setup.perform_custom_imgui({ 
				lua,
				window,
				streaming.images_in_atlas,
				streaming.ad_hoc.in_atlas,
				streaming.necessary_images_in_atlas,
				config,
				is_replaying_demo()
			});

			using S = remove_cref<decltype(setup)>;

			switch (result) {
				case custom_imgui_result::GO_TO_MAIN_MENU:
					launch_setup(activity_type::MAIN_MENU);
					break;

				case custom_imgui_result::GO_TO_PROJECT_SELECTOR:
					launch_setup(activity_type::EDITOR_PROJECT_SELECTOR);
					break;

				case custom_imgui_result::RETRY:
					if constexpr(std::is_same_v<S, client_setup>) {
						launch_setup(activity_type::CLIENT);
					}

					break;

				case custom_imgui_result::OPEN_SELECTED_PROJECT:
					if constexpr(std::is_same_v<S, project_selector_setup>) {
						auto backup = std::make_unique<setup_variant>(setup);

						const auto path = setup.get_selected_project_path();

						try {
							launch_editor(path);
						}
						catch (const std::runtime_error& err) {
							const auto full_content = typesafe_sprintf("Failed to load: %x\nReason:\n\n%x", path, err.what());
							failed_to_load_arena_popup = simple_popup { "Error", full_content, "" };

							current_setup = std::move(backup);
						}
					}

					break;

				case custom_imgui_result::PLAYTEST_ONLINE:
					if constexpr(std::is_same_v<S, editor_setup>) {
						setup.prepare_for_online_playtesting();

						if (config.server.allow_nat_traversal) {
							if (!nat_detection_complete()) {
								setup.set_recent_message("NAT detection in progress. Please try again in a few seconds.");
								break;
							}
						}

						const auto bound_port = get_bound_local_port();
						auxiliary_socket.reset();

						LOG("Starting server setup for playtesting. Binding to a port: %x (%x was preferred)", bound_port, last_requested_local_port);

						auto start = config.server_start;
						start.port = bound_port;

						auto playtest_vars = config.server;
						playtest_vars.server_name = "${MY_NICKNAME} is creating " + setup.get_arena_name();
						playtest_vars.playtesting_context = setup.make_playtesting_context();
						playtest_vars.arena = setup.get_arena_name();

						background_setup = std::move(current_setup);

						setup_launcher([&]() {
							emplace_current_setup(std::in_place_type_t<server_setup>(),
								lua,
								*official,
								start,
								playtest_vars,
								config.server_private,
								config.client,
								std::nullopt,

								make_server_nat_traversal_input(),
								params.suppress_server_webhook
							);
						});
					}

					break;

				case custom_imgui_result::QUIT_PLAYTESTING:
					restore_background_setup();
					break;

				default: break;
			}
		});
	};

	auto process_steam_callbacks = [&]() {
		auto connect_to = [&](const std::string& address_string) {
			if (address_string.length() > 0) {
				LOG("(Steam Callback) Joining server (length: %x): %x", address_string.length(), address_string);

				config.client_connect = address_string;

				/* We must know if it's behind NAT */

				browse_servers_gui.sync_download_server_entry(
					get_browse_servers_input(),
					config.client_connect
				);

				start_client_setup();
			}
			else {
				LOG("(Steam Callback) Server address was empty.");
			}
		};

		using steam_queue_type = std::vector<
			std::variant<
				steam_new_url_launch_parameters,
				steam_new_join_game_request,
				steam_change_server_request,
				steam_auth_ticket
			>
		>; 
		
		thread_local steam_queue_type steam_events;

		steam_events.clear();

		auto handler_c = [](void* ctx, uint32_t idx, void* object) {
			auto check_index = [&]<typename T>(T) {
				if (idx == T::index) {
					auto typed_ctx = reinterpret_cast<steam_queue_type*>(ctx);
					typed_ctx->push_back(*reinterpret_cast<T*>(object));
				}
			};

			check_index(steam_new_url_launch_parameters());
			check_index(steam_new_join_game_request());
			check_index(steam_change_server_request());
			check_index(steam_auth_ticket());
		};

		steam_run_callbacks(handler_c, &steam_events);

		auto handler_lbd = [connect_to, steam_auth_request_id, visit_current_setup]<typename E>(const E& event) {
			if constexpr(std::is_same_v<E, steam_new_url_launch_parameters>) {
				LOG("(Steam Callback) steam_new_url_launch_parameters.");
				(void)event;

				connect_to(steam_get_launch_command_line_string());
			}
			else if constexpr(std::is_same_v<E, steam_new_join_game_request>) {
				LOG("(Steam Callback) steam_new_join_game_request.");

				connect_to(std::string(event.connect_cli.data()));
			}
			else if constexpr(std::is_same_v<E, steam_change_server_request>) {
				LOG("(Steam Callback) steam_change_server_request.");

				connect_to(std::string(event.server_address.data()));
			}
			else if constexpr(std::is_same_v<E, steam_auth_ticket>) {
				LOG("(Steam Callback) steam_auth_ticket.");

				if (event.request_id == steam_auth_request_id) {
					visit_current_setup([&](auto& setup) {
						using T = remove_cref<decltype(setup)>;

						if constexpr(is_one_of_v<T, client_setup>) {
							setup.send_auth_ticket(event);
						}
					});
				}
			}
			else {
				static_assert(always_false_v<E>, "Non-exhaustive");
			}
		};

		for (auto& event : steam_events) {
			std::visit(handler_lbd, event);
		}
	};

	auto do_imgui_pass = [&](const auto frame_num, auto& new_window_entropy, const auto& frame_delta, const bool in_direct_gameplay) {
		bool freeze_imgui_inputs = false;

		on_specific_setup([&](editor_setup& editor) {
			if (editor.is_mover_active()) {
				freeze_imgui_inputs = true;
			}
		});

		perform_imgui_pass(
			window,
			new_window_entropy,
			logic_get_screen_size(),
			frame_delta,
			canon_config,
			config,
			last_saved_config,
			local_config_path,
			settings_gui,
			audio,
			lua,
			[&]() {
				auto do_nat_detection_logic = [&]() {
					bool do_nat_detection = true;

					visit_current_setup([&](const auto& setup) {
						using T = remove_cref<decltype(setup)>;

						if constexpr(is_one_of_v<T, client_setup, server_setup>) {
							/* Don't do NAT detection when it's already too late to do so */
							do_nat_detection = false;
						}
					});

					if (!do_nat_detection) {
						return;
					}

					if (last_requested_local_port != chosen_server_port()) {
						recreate_auxiliary_socket();
						restart_nat_detection();
					}

					if (nat_detection.has_value()) {
						if (config.nat_detection != nat_detection->get_settings()) {
							restart_nat_detection();
						}

						if (auxiliary_socket.has_value()) {
							nat_detection->advance(auxiliary_socket->socket);
						}
					}
				};

				/* 
					Prevent nat detection from tampering with traversal logic, 
					as we'll rebind the local port frequently 
				*/
				if (nat_traversal == std::nullopt) {
					do_nat_detection_logic();
				}

				if (start_client_gui.show) {
					if (start_client_gui.current_tab == start_client_tab_type::BEST_RANKED) {
						if (!browse_servers_gui.refreshed_at_least_once()) {
							browse_servers_gui.refresh_server_list(get_browse_servers_input());
						}
					}
				}

				if (start_client_gui.request_refresh_best_server) {
					start_client_gui.request_refresh_best_server = false;

					browse_servers_gui.refresh_server_list(get_browse_servers_input());
				}

				browse_servers_gui.advance_ping_logic();

				if (const bool show_server_browser = !has_current_setup() || ingame_menu.show) {
					perform_browse_servers();
				}

				if (const bool show_map_catalogue = !has_current_setup()) {
					perform_map_catalogue();
				}

				if (!has_current_setup()) {
					perform_last_exit_incorrect();
					perform_start_client_gui(frame_num);
					perform_start_server_gui();
				}

				perform_failed_to_load_arena_popup();

				streaming.display_loading_progress();

				advance_nat_traversal();
				do_traversal_details_popup(window);
				do_detection_details_popup();

				if (pending_launch.has_value()) {
					const auto l = pending_launch;

					if (l == activity_type::SERVER) {
						if (nat_detection_complete()) {
							finalize_pending_launch();
						}
					}

					if (l == activity_type::CLIENT) {
						if (nat_detection_complete()) {
							if (nat_traversal == std::nullopt) {
								start_nat_traversal();
							}

							check_nat_traversal_result();
						}
					}
				}
			},

			[&]() {
				perform_setup_custom_imgui();
			},

			/* Flags controlling IMGUI behaviour */

			ingame_menu.show,
			has_current_setup(),

			in_direct_gameplay,
			float_tests_succeeded
		);

		if (freeze_imgui_inputs) {
			ImGui::GetIO().WantCaptureMouse = false;
		}

		new_window_entropy = augs::imgui::filter_inputs(new_window_entropy);
	};

	auto decide_on_cursor_clipping = [&](const bool in_direct_gameplay, const auto& cfg) {
		get_write_buffer().should_clip_cursor = (
			in_direct_gameplay
			|| cfg.window.draws_own_cursor()
		);
	};

	auto get_current_input_settings = [&](const auto& cfg) {
		auto settings = cfg.input;

#if BUILD_NETWORKING
		on_specific_setup([&](client_setup& setup) {
			settings.character = setup.get_current_requested_settings().public_settings.character_input;
		});
#endif

		return settings;
	};

	auto handle_app_intent = [&](const app_intent_type intent) {
		using T = decltype(intent);

		switch (intent) {
			case T::SHOW_PERFORMANCE: {
				change_with_save([&](config_lua_table& cfg) {
					bool& f = cfg.session.show_performance;
					f = !f;

					if (f) {
						cfg.session.show_logs = false;
					}
				});

				break;
			}

			case T::SHOW_LOGS: {
				change_with_save([&](config_lua_table& cfg) {
					bool& f = cfg.session.show_logs;
					f = !f;

					if (f) {
						cfg.session.show_performance = false;
					}
				});

				break;
			}

			case T::TOGGLE_STREAMER_MODE: {
				bool& f = config.streamer_mode;
				f = !f;
				break;
			}

			case T::TOGGLE_CINEMATIC_MODE: {
				bool& f = config.drawing.cinematic_mode;
				f = !f;
				break;
			}

			default: break;
		}
	};
	
	auto handle_general_gui_intent = [&](const general_gui_intent_type intent) {
		using T = decltype(intent);

		switch (intent) {
			case T::CLEAR_DEBUG_LINES:
				DEBUG_PERSISTENT_LINES.clear();
				return true;

			case T::TOGGLE_WEAPON_LASER: {
				bool& f = config.drawing.draw_weapon_laser;
				f = !f;
				return true;
			}

			case T::TOGGLE_MOUSE_CURSOR: {
				bool& f = game_gui_mode_flag;
				f = !f;
				return true;
			}
			
			default: return false;
		}
	};
 
	auto main_ensure_handler = [&]() {
		visit_current_setup(
			[&](auto& setup) {
				setup.ensure_handler();
			}
		);
	};

	::ensure_handler = main_ensure_handler;

	bool should_quit = false;

	augs::event::state common_input_state;
	common_input_state.mouse.pos = window.get_last_mouse_pos();

	std::function<void()> request_quit;

	auto do_main_menu_option = [&](const main_menu_button_type t) {
		LOG("Menu option: %x", augs::enum_to_string(t));
		
		using T = decltype(t);

		switch (t) {
			case T::JOIN_DISCORD:
				augs::open_url("https://discord.com/invite/YC49E4G");
				break;

			case T::DOWNLOAD_MAPS:
				map_catalogue_gui.open();
				break;

			case T::BROWSE_SERVERS:
				browse_servers_gui.open();

				break;

			case T::PLAY_RANKED:
				if (common_input_state[augs::event::keys::key::LSHIFT]) {
#if !IS_PRODUCTION_BUILD
					client_start_requested = true;
#endif
				}
				else {
					start_client_gui.open();
					start_client_gui.current_tab = start_client_tab_type::BEST_RANKED;
				}

				break;

			case T::CONNECT_TO_SERVER:
				if (common_input_state[augs::event::keys::key::LSHIFT]) {
#if !IS_PRODUCTION_BUILD
					client_start_requested = true;
#endif
				}
				else {
					start_client_gui.open();
					start_client_gui.current_tab = start_client_tab_type::CUSTOM_ADDRESS;
				}

				break;
				
			case T::HOST_SERVER:
				start_server_gui.open();

				if (common_input_state[augs::event::keys::key::LSHIFT]) {
					server_start_requested = true;
				}

				break;

			case T::SHOOTING_RANGE:
				launch_setup(activity_type::SHOOTING_RANGE);
				break;

			case T::TUTORIAL:
				launch_setup(activity_type::TUTORIAL);
				break;

			case T::EDITOR:
				launch_setup(activity_type::EDITOR_PROJECT_SELECTOR);
				break;

			case T::SETTINGS:
				settings_gui.open();
				break;

			case T::CREDITS:
				augs::open_url("https://hypersomnia.xyz/credits");
				break;

			case T::QUIT:
				LOG("Quitting due to Quit pressed in main menu.");
				request_quit();
				break;

			default: break;
		}
	};

	auto do_ingame_menu_option = [&](const ingame_menu_button_type t) {
		using T = decltype(t);

		switch (t) {
			case T::SERVER_DETAILS:
				if (is_during_tutorial()) {
					std::get<test_scene_setup>(*current_setup).request_checkpoint_restart();
					ingame_menu.show = false;
				}

				break;

			case T::BROWSE_SERVERS:
				browse_servers_gui.open();
				break;

			case T::RESUME:
				ingame_menu.show = false;
				break;

			case T::QUIT_TO_MENU:
				if (background_setup != nullptr) {
					restore_background_setup();
				}
				else {
					if (has_current_setup() && std::holds_alternative<editor_setup>(*current_setup)) {
						launch_setup(activity_type::EDITOR_PROJECT_SELECTOR);
					}
					else {
						launch_setup(activity_type::MAIN_MENU);
					}
				}

				break;

			case T::SETTINGS:
				settings_gui.open();
				break;

			case T::QUIT_GAME:
				LOG("Quitting due to Quit pressed in ingame menu.");
				request_quit();
				break;

			default: break;
		}
	};

	auto setup_pre_solve = [&](auto...) {
		get_general_renderer().save_debug_logic_step_lines_for_interpolation(DEBUG_LOGIC_STEP_LINES);
		DEBUG_LOGIC_STEP_LINES.clear();
	};

	/* 
		The audiovisual_step, advance_setup and advance_current_setup lambdas
		are separated only because MSVC outputs ICEs if they become nested.
	*/

	visible_entities all_visible;

	auto get_character_camera = [&](const config_lua_table& viewing_config) -> character_camera {
		return { get_viewed_character(), { get_camera_eye(viewing_config), logic_get_screen_size() } };
	};

	auto reacquire_visible_entities = [&](
		const vec2i& screen_size,
		const const_entity_handle& viewed_character,
		const config_lua_table& viewing_config
	) {
		auto scope = measure_scope(game_thread_performance.camera_visibility_query);

		auto queried_eye = get_camera_eye(viewing_config);
		queried_eye.zoom /= viewing_config.session.camera_query_aabb_mult;

		const auto queried_cone = camera_cone(queried_eye, screen_size);
		const auto& cosm = viewed_character.get_cosmos();

		all_visible.reacquire_all({ 
			cosm, 
			queried_cone, 
			accuracy_type::PROXIMATE,
			get_render_layer_filter(),
			tree_of_npo_filter::all()
		});

		all_visible.sort(cosm);

		game_thread_performance.num_visible_entities.measure(all_visible.count_all());
	};

	auto calc_pre_step_crosshair_displacement = [&](const config_lua_table& viewing_config) {
		if (get_viewed_character() != get_controlled_character()) {
			return vec2::zero;
		}

		return visit_current_setup([&](const auto& setup) {
			using T = remove_cref<decltype(setup)>;

			if constexpr(!std::is_same_v<T, main_menu_setup>) {
				const auto& total_collected = setup.get_entropy_accumulator();

				const auto input_cfg = get_current_input_settings(viewing_config);

				if (const auto motion = total_collected.calc_motion(
					get_viewed_character(), 
					game_motion_type::MOVE_CROSSHAIR,
					entropy_accumulator::input {
						input_cfg, 
						get_nonzoomedout_visible_world_area(viewing_config)
					}
				)) {
					return vec2(motion->offset) * input_cfg.character.crosshair_sensitivity;
				}
			}

			return vec2::zero;
		});
	};

	bool pending_new_state_sample = true;
	auto last_sampled_cosmos = cosmos_id_type(-1);

	augs::timer state_changed_timer;

	auto audiovisual_step = [&](
		const augs::audio_renderer* audio_renderer,
		const augs::delta frame_delta,
		const double speed_multiplier,
		const config_lua_table& viewing_config
	) {
		const auto screen_size = logic_get_screen_size();
		const auto viewed_character = get_viewed_character();
		const auto& cosm = viewed_character.get_cosmos();
		
		//get_audiovisuals().reserve_caches_for_entities(viewed_character.get_cosmos().get_solvable().get_entity_pool().capacity());
		
		auto& interp = get_audiovisuals().get<interpolation_system>();

		{
			auto scope = measure_scope(get_audiovisuals().performance.interpolation);

			if (pending_new_state_sample) {
				interp.update_desired_transforms(cosm);
			}

			interp.integrate_interpolated_transforms(
				viewing_config.interpolation, 
				cosm, 
				frame_delta, 
				cosm.get_fixed_delta(),
				speed_multiplier
			);
		}

		auto nonzoomedout_area = get_nonzoomedout_visible_world_area(viewing_config);

		visit_current_setup([&]<typename S>(const S& setup) {
			if constexpr(is_one_of_v<S, client_setup, server_setup>) {
				const auto currently_viewed = setup.get_viewed_player_nonzoomedout_visible_world_area();

				if (currently_viewed != vec2::zero) {
					nonzoomedout_area = currently_viewed;
				}
			}
		});

		gameplay_camera.tick(
			screen_size,
			nonzoomedout_area,
			interp,
			frame_delta,
			viewing_config.camera,
			viewed_character,
			calc_pre_step_crosshair_displacement(viewing_config)
		);

		hud_messages.advance(viewing_config.hud_messages.value);

		reacquire_visible_entities(screen_size, viewed_character, viewing_config);

		const auto inv_tickrate = visit_current_setup([&](const auto& setup) {
			return setup.get_inv_tickrate();
		});

		visit_current_setup([&]<typename S>(const S& setup) {
			const auto now_sampled_cosmos = cosm.get_cosmos_id();

			auto resample = [&]() {
				audio_buffers.finish();
				audio_buffers.stop_all_sources();

				get_audiovisuals().get<sound_system>().clear();
				get_audiovisuals().get<particles_simulation_system>().clear();

				last_sampled_cosmos = now_sampled_cosmos;

				cosm.mark_as_resampled();

#if !IS_PRODUCTION_BUILD
				LOG("RESAMPLE due to a switched cosmos");
#endif
			};

			auto resample_if_different = [&]() {
				const bool requested_resample = cosm.resample_requested();

				if (requested_resample || last_sampled_cosmos != now_sampled_cosmos) {
					resample();
					gameplay_camera.dont_smooth_once = true;
				}
			};

			if constexpr(std::is_same_v<S, client_setup>) {
				const auto& referential = setup.get_arena_handle(client_arena_type::REFERENTIAL).get_cosmos();
				const auto& predicted = setup.get_arena_handle(client_arena_type::PREDICTED).get_cosmos();

				if (referential.resample_requested() || predicted.resample_requested()) {
					resample();

					referential.mark_as_resampled();
					predicted.mark_as_resampled();
				}
			}
			else {
				resample_if_different();
			}
		});

		auto audio_volume = viewing_config.audio_volume;

		if (viewing_config.audio.mute_main_menu_background) {
			if (has_main_menu() && !has_current_setup()) {
				audio_volume.sound_effects = 0.0f;
				audio_volume.music = 0.0f;
			}
		}

		get_audiovisuals().advance(audiovisual_advance_input {
			audio_buffers,
			audio_renderer,
			frame_delta,
			speed_multiplier,
			inv_tickrate,
			get_interpolation_ratio(),

			get_character_camera(viewing_config),
			get_queried_cone(viewing_config),
			all_visible,

			get_viewable_defs().particle_effects,
			cosm.get_logical_assets().plain_animations,

			streaming.loaded_sounds,

			audio_volume,
			viewing_config.sound,
			viewing_config.performance,

			streaming.images_in_atlas,
			get_write_buffer().particle_buffers,
			get_general_renderer().dedicated,
			pending_new_state_sample ? state_changed_timer.extract_delta() : std::optional<augs::delta>(),

			viewing_config.damage_indication,

			thread_pool
		});

		pending_new_state_sample = false;
	};

	auto setup_post_solve = [&](
		const const_logic_step step, 
		const augs::audio_renderer* audio_renderer,
		const config_lua_table& viewing_config,
		const audiovisual_post_solve_settings settings
	) {
		pending_new_state_sample = true;

		{
			const auto& defs = get_viewable_defs();

			get_audiovisuals().standard_post_solve(step, { 
				audio_renderer,
				defs.particle_effects, 
				streaming.loaded_sounds,
				viewing_config.audio_volume,
				viewing_config.sound,
				get_character_camera(viewing_config),
				viewing_config.performance,
				viewing_config.damage_indication,
				settings
			});
		}

		game_gui.standard_post_solve(
			step, 
			{ settings.prediction }
		);

		if (never_predictable_v.should_play(settings.prediction)) {
			hud_messages.standard_post_solve(
				step,
				viewing_config.faction_view,
				viewing_config.hud_messages.value
			);
		}
	};

	auto setup_post_cleanup = [&](const auto& cfg, const const_logic_step step) {
		if (cfg.debug.log_solvable_hashes) {
			const auto& cosm = step.get_cosmos();
			const auto ts = cosm.get_timestamp().step;
			const auto h = cosm.calculate_solvable_signi_hash<uint32_t>();

			LOG_NVPS(ts, h);
		}
	};

	auto advance_setup = [&](
		const augs::audio_renderer* audio_renderer,
		const augs::delta frame_delta,
		auto& setup,
		const input_pass_result& result
	) {
		const config_lua_table& viewing_config = result.viewing_config;

		setup.control(result.motions);
		setup.control(result.intents);

		setup.accept_game_gui_events(game_gui.get_and_clear_pending_events());
		
		auto setup_audiovisual_post_solve = [&](const const_logic_step step, const audiovisual_post_solve_settings settings = {}) {
			setup_post_solve(step, audio_renderer, viewing_config, settings);
		};

		{
			using S = remove_cref<decltype(setup)>;

			auto callbacks = solver_callbacks(
				setup_pre_solve,
				setup_audiovisual_post_solve,
				[&viewing_config, &setup_post_cleanup](const const_logic_step& step) { setup_post_cleanup(viewing_config, step); }
			);

			const auto nonzoomedout_zoom = get_camera_eye(viewing_config, no_edge_zoomout_v).zoom;
			const auto input_cfg = get_current_input_settings(viewing_config);

			if constexpr(std::is_same_v<S, client_setup>) {
				/* The client needs more goodies */

				setup.advance(
					{ 
						frame_delta,
						logic_get_screen_size(), 
						input_cfg, 
						nonzoomedout_zoom,
						viewing_config.simulation_receiver, 
						viewing_config.lag_compensation, 
						network_performance,
						network_stats,
						get_audiovisuals().get<interpolation_system>(),
						get_audiovisuals().get<past_infection_system>()
					},
					callbacks
				);

				if (setup.is_replaying() && setup.is_paused()) {
					pending_new_state_sample = true;
				}
			}
			else if constexpr(std::is_same_v<S, server_setup>) {
				setup.advance(
					{ 
						logic_get_screen_size(), 
						input_cfg, 
						nonzoomedout_zoom,
						get_detected_nat(),
						network_performance,
						server_stats
					},
					callbacks
				);
			}
			else {
#if BUILD_DEBUGGER_SETUP
				if constexpr(std::is_same_v<S, debugger_setup>) {
					if (setup.is_editing_mode()) {
						pending_new_state_sample = true;
					}
				}
#endif

				if constexpr(std::is_same_v<S, editor_setup>) {
					pending_new_state_sample = true;
				}

				setup.advance(
					{ 
						frame_delta, 
						logic_get_screen_size(), 
						input_cfg, 
						nonzoomedout_zoom 
					},
					callbacks
				);
			}

			thread_local augs::timer rich_presence_timer;
			const auto passed_secs = rich_presence_timer.get<std::chrono::seconds>();

			if (passed_secs > 2.0 || set_rich_presence_now) {
				set_rich_presence_now = false;
				rich_presence_timer.reset();

				thread_local steam_rich_presence_pairs pairs;
				pairs.clear();

				setup.get_steam_rich_presence_pairs(pairs);

				for (auto& p : pairs) {
					if (p.second.has_value()) {
						::steam_set_rich_presence(p.first.c_str(), p.second.value().c_str());
					}
					else {
						::steam_set_rich_presence(p.first.c_str(), nullptr);
					}
				}
			}
		}

		audiovisual_step(audio_renderer, frame_delta, setup.get_audiovisual_speed(), viewing_config);
	};

	auto advance_current_setup = [&](
		const augs::audio_renderer* audio_renderer,
		const augs::delta frame_delta,
		const input_pass_result& result
	) { 
		visit_current_setup(
			[&](auto& setup) {
				advance_setup(audio_renderer, frame_delta, setup, result);
			}
		);
	};

	{
		auto connect_to = [&](const auto& target) {
			if (!target.empty()) {
				change_with_save([&](config_lua_table& cfg) {
					cfg.client_connect = target;
				});
			}

			launch_setup(activity_type::CLIENT);
		};

		const auto steam_cli = steam_get_launch_command_line_string();

		if (steam_cli.length() > 0) {
			LOG("Detected Steam CLI (length: %x): %x", steam_cli.length(), steam_cli);
			connect_to(steam_cli);
		}
		else {
			LOG("No Steam CLI detected.");

			if (!params.debugger_target.empty()) {
				launch_debugger(lua, params.debugger_target);
			}
			else if (params.start_server) {
				launch_setup(activity_type::SERVER);
			}
			else if (params.should_connect) {
				connect_to(params.connect_address);
			}
			else {
				if (config.launch_at_startup == launch_type::LAST_ACTIVITY) {
					if (!config.skip_tutorial) {
						launch_setup(activity_type::TUTORIAL);
					}
					else {
						launch_setup(config.get_last_activity());
					}
				}
				else {
					launch_setup(activity_type::MAIN_MENU);
				}
			}
		}
	}

	/* 
		The main loop variables.
	*/

	augs::timer frame_timer;
	
	release_flags releases;

	auto make_create_game_gui_context = [&](const config_lua_table& viewing_config) {
		return [&]() {
			return game_gui.create_context(
				logic_get_screen_size(),
				common_input_state,
				get_game_gui_subject(),
				create_game_gui_deps(viewing_config)
			);
		};
	};

	auto make_create_menu_context = [&](const config_lua_table& viewing_config) {
		return [&](auto& gui) {
			return gui.create_context(
				logic_get_screen_size(),
				common_input_state,
				create_menu_context_deps(viewing_config)
			);
		};
	};

	auto close_next_imgui_window = [&]() {
		ImGuiContext& g = *GImGui;

		if (g.WindowsFocusOrder.size() > 0) {
			if (g.WindowsFocusOrder.back()) {
				augs::imgui::next_window_to_close = g.WindowsFocusOrder.back()->ID; 
			}
		}
	};

	auto let_imgui_hijack_mouse = [&](auto&& create_game_gui_context, auto&& create_menu_context) {
		if (!ImGui::GetIO().WantCaptureMouse) {
			return;
		}

		/*
			Since ImGUI has quite a different philosophy about input,
			we will need some ugly inter-op with our GUIs.

			If mouse enters any IMGUI element, rewrite ImGui's mouse position to common_input_state.

			This allows us to keep common_input_state up to date, 
			because mousemotions are eaten from the vector already due to ImGui wanting mouse.
		*/

		common_input_state.mouse.pos = ImGui::GetIO().MousePos;

		/* Neutralize hovers on all GUIs whose focus may have just been stolen. */

		game_gui.world.unhover_and_undrag(create_game_gui_context());

		if (has_main_menu()) {
			main_menu_gui.world.unhover_and_undrag(create_menu_context(main_menu_gui));
		}

		ingame_menu.world.unhover_and_undrag(create_menu_context(ingame_menu));

#if BUILD_DEBUGGER_SETUP
		on_specific_setup([&](debugger_setup& setup) {
			setup.unhover();
		});
#endif

		on_specific_setup([&](editor_setup& setup) {
			setup.unhover();
		});
	};

	auto advance_game_gui = [&](const auto context, const auto frame_delta) {
		auto scope = measure_scope(game_thread_performance.advance_game_gui);

		game_gui.advance(context, frame_delta);
		game_gui.rebuild_layouts(context);
		game_gui.build_tree_data(context);
	};

	/* 
		MousePos is initially set to negative infinity.
	*/

	ImGui::GetIO().MousePos = { 0, 0 };

	cached_visibility_data cached_visibility;
	debug_details_summaries debug_summaries;

	auto game_thread_result = work_result::SUCCESS;

	auto game_thread_worker = [&]() {
		auto prepare_next_game_frame = [&]() {
			auto frame = measure_scope(game_thread_performance.total);

			{
				/* The thread pool is always empty of tasks on the beginning of the game frame. */

				const auto requested_num_workers = config.performance.get_num_pool_workers();
				const auto current_num_workers = static_cast<int>(thread_pool.size());

				if (current_num_workers != requested_num_workers) {
					thread_pool.resize(requested_num_workers);
				}
			}

			/* Setup variables required by the lambdas */

			const auto screen_size = logic_get_screen_size();
			const auto frame_delta = frame_timer.extract_delta();
			const auto current_frame_num = current_frame.load();
			auto game_gui_mode = game_gui_mode_flag;

			auto& write_buffer = get_write_buffer();

			auto& game_gui_renderer = write_buffer.renderers.all[renderer_type::GAME_GUI];
			auto& post_game_gui_renderer = write_buffer.renderers.all[renderer_type::POST_GAME_GUI];
			auto& debug_details_renderer = write_buffer.renderers.all[renderer_type::DEBUG_DETAILS];

			/* Logic lambdas */

			auto get_current_frame_num = [&]() {
				return current_frame_num;
			};

			auto should_quit_due_to_signal = [&]() {
#if PLATFORM_UNIX
				if (signal_status != 0) {
					const auto sig = signal_status.load();

					LOG("%x received.", strsignal(sig));

					if(
						sig == SIGINT
						|| sig == SIGSTOP
						|| sig == SIGTERM
					) {
						LOG("Gracefully shutting down.");
						request_quit();
						
						return true;
					}
				}
#endif

				return false;
			};

			auto perform_input_pass = [&]() -> input_pass_result {
				/* 
					The centralized transformation of all window inputs.
					No window inputs will be acquired and/or used beyond the scope of this lambda,
					to the exception of remote packets, received by the client/server setups.
					
					This is necessary because we need some complicated interactions between multiple GUI contexts,
					primarily in deciding what events should be propagated further, down to the gameplay itself.
					It is the easiest if every possibility is considered in one place. 
					We have decided that some stronger decoupling here would benefit nobody.

					The lambda is called right away, like so: 
						result = [...](){...}().
					The result of the call, which is the collection of new game commands, will be passed further down the loop. 
				*/

				input_pass_result out;

				augs::local_entropy new_window_entropy;

				/* Generate release events if the previous frame so requested. */

				releases.append_releases(new_window_entropy, common_input_state);
				releases = {};

				concatenate(new_window_entropy, write_buffer.new_window_entropy);

				on_specific_setup([&](editor_setup& editor) {
					if (editor.warp_cursor_once) {
						editor.warp_cursor_once = false;

						common_input_state.mouse.pos = ImGui::GetIO().MousePos;
					}
				});

				if (get_viewed_character().dead()) {
					game_gui_mode = true;
				}

				const bool in_direct_gameplay =
					!game_gui_mode
					&& has_current_setup()
					&& !ingame_menu.show
				;

				/*
					Top-level events, higher than IMGUI.
				*/
				
				{
					auto simulated_input_state = common_input_state;

					erase_if(new_window_entropy, [&](const augs::event::change e) {
						using namespace augs::event;
						using namespace augs::event::keys;

						simulated_input_state.apply(e);

						if (e.msg == message::activate || e.msg == message::click_activate) {
							if (config.content_regeneration.rescan_assets_on_window_focus) {
								streaming.request_rescan();
							}

							write_buffer.should_recheck_current_context = true;
							map_catalogue_gui.request_rescan();
						}

						if (e.msg == message::deactivate) {
							releases.set_all();
						}

						if (e.is_exit_message()) {
							LOG("Window closing due to message: %x. Shutting down.", augs::enum_to_string(e.msg));
							request_quit();
							return true;
						}
						
						{
							const bool toggle_fullscreen = 
								e.was_pressed(key::F11)
#if PLATFORM_WINDOWS
								|| (e.was_pressed(key::ENTER) && common_input_state[augs::event::keys::key::LALT])
#endif
							;

							if (toggle_fullscreen) {
								bool& f = config.window.fullscreen;
								f = !f;
								return true;
							}
						}

						if (settings_gui.should_hijack_key()) {
							if (e.was_any_key_pressed()) {
								settings_gui.set_hijacked_key(e.get_key());
								return true;
							}
						}

						if (!ingame_menu.show) {
							if (visit_current_setup([&](auto& setup) {
								using T = remove_cref<decltype(setup)>;

								if constexpr(T::handles_window_input) {
									/* 
										Lets a setup fetch an input before IMGUI does,
										if for example IMGUI wants to capture keyboard input.	
									*/

									return setup.handle_input_before_imgui({
										simulated_input_state, e, window
									});
								}

								return false;
							})) {
								return true;
							}
						}

						return false;
					});
				}

				/*
					We "pause" the mouse cursor's position when we are in direct gameplay,
					so that when switching to GUI, the cursor appears exactly where it had disappeared.
					(it does not physically freeze the cursor, it just remembers the position)
				*/

				write_buffer.should_pause_cursor = in_direct_gameplay;

				const bool was_any_imgui_popup_opened = ImGui::IsPopupOpen(0u, ImGuiPopupFlags_AnyPopupId);

				do_imgui_pass(get_current_frame_num(), new_window_entropy, frame_delta, in_direct_gameplay);
				process_steam_callbacks();

				const auto viewing_config = get_setup_customized_config();
				out.viewing_config = viewing_config;

				configurables.apply(viewing_config);
				write_buffer.new_settings = viewing_config.window;
				write_buffer.swap_when = viewing_config.performance.swap_window_buffers_when;
				write_buffer.max_fps = viewing_config.window.max_fps;
				write_buffer.max_fps_method = viewing_config.window.max_fps_method;
				decide_on_cursor_clipping(in_direct_gameplay, viewing_config);

				releases.set_due_to_imgui(ImGui::GetIO());

				auto create_menu_context = make_create_menu_context(viewing_config);
				auto create_game_gui_context = make_create_game_gui_context(viewing_config);

				let_imgui_hijack_mouse(create_game_gui_context, create_menu_context);

				/*
					We also need inter-op between our own GUIs, 
					since we have more than just one.
				*/

				if (game_gui_mode && should_draw_game_gui() && game_gui.world.wants_to_capture_mouse(create_game_gui_context())) {
					if (current_setup) {
#if BUILD_DEBUGGER_SETUP
						if (auto* editor = std::get_if<debugger_setup>(&*current_setup)) {
							editor->unhover();
						}
#endif
					}
				}

				/* Maybe the game GUI was deactivated while the button was still hovered. */

				else if (!game_gui_mode && has_current_setup()) {
					game_gui.world.unhover_and_undrag(create_game_gui_context());
				}

				/* Distribution of all the remaining input happens here. */

				for (const auto& e : new_window_entropy) {
					using namespace augs::event;
					using namespace keys;
					
					/* Now is the time to actually track the input state. */
					common_input_state.apply(e);

					if (!was_any_imgui_popup_opened && e.was_pressed(key::ESC)) {
						if (has_current_setup()) {
							if (ingame_menu.show) {
								ingame_menu.show = false;
							}
							else if (!visit_current_setup([&](auto& setup) {
								switch (setup.escape()) {
									case setup_escape_result::LAUNCH_INGAME_MENU: ingame_menu.show = true; return true;
									case setup_escape_result::JUST_FETCH: return true;
									case setup_escape_result::GO_TO_MAIN_MENU: launch_setup(activity_type::MAIN_MENU); return true;
									case setup_escape_result::QUIT_PLAYTESTING: restore_background_setup(); return true;
									default: return false;
								}
							})) {
								/* Setup ignored the ESC button */
								ingame_menu.show = true;
							}

							releases.set_all();
						}
						else {
							close_next_imgui_window();
						}

						continue;
					}

					const auto key_change = ::to_intent_change(e.get_key_change());

					const bool was_pressed = key_change == intent_change::PRESSED;
					const bool was_released = key_change == intent_change::RELEASED;
					
					if (was_pressed || was_released) {
						const auto key = e.get_key();

						if (const auto it = mapped_or_nullptr(viewing_config.app_controls, key)) {
							if (was_pressed) {
								handle_app_intent(*it);
								continue;
							}
						}
					}

					{
						auto control_main_menu = [&]() {
							if (has_main_menu() && !has_current_setup()) {
								if (main_menu_gui.show) {
									main_menu_gui.control(create_menu_context(main_menu_gui), e, do_main_menu_option);
								}

								return true;
							}

							return false;
						};

						auto control_ingame_menu = [&]() {
							if (ingame_menu.show || was_released) {
								return ingame_menu.control(create_menu_context(ingame_menu), e, do_ingame_menu_option);
							}

							return false;
						};
						
						if (was_released) {
							control_main_menu();
							control_ingame_menu();
						}
						else {
							if (control_main_menu()) {
								continue;
							}

							if (control_ingame_menu()) {
								continue;
							}

							/* Prevent e.g. panning in editor when the ingame menu is on */
							if (ingame_menu.show) {
								continue;
							}
						}
					}

					{
						if (visit_current_setup([&](auto& setup) {
							using T = remove_cref<decltype(setup)>;

							if constexpr(T::handles_window_input) {
								if (!streaming.necessary_images_in_atlas.empty()) {
									/* Viewables reloading happens later so it might not be ready yet */

									const auto& general_gui_controls = viewing_config.general_gui_controls;

									return setup.handle_input_before_game({
										general_gui_controls, streaming.necessary_images_in_atlas, common_input_state, e, window
									});
								}
							}

							return false;
						})) {
							on_specific_setup([](client_setup& client) {
								client.reset_afk_timer();
							});

							on_specific_setup([](server_setup& client) {
								client.reset_afk_timer();
							});

							continue;
						}
					}

					const auto viewed_character = get_viewed_character();

					if (was_released || (has_current_setup() && !ingame_menu.show)) {
						const bool direct_gameplay = viewed_character.alive() && !game_gui_mode;
						const bool game_gui_effective = viewed_character.alive() && game_gui_mode;

						if (was_released || was_pressed) {
							const auto key = e.get_key();

							if (was_released || direct_gameplay || game_gui_effective) {
								if (const auto it = mapped_or_nullptr(viewing_config.general_gui_controls, key)) {
									if (was_pressed) {
										if (handle_general_gui_intent(*it)) {
											continue;
										}
									}
								}
								if (const auto it = mapped_or_nullptr(viewing_config.inventory_gui_controls, key)) {
									if (should_draw_game_gui()) {
										const auto input_cfg = get_current_input_settings(viewing_config);
										game_gui.control_hotbar_and_action_button(get_game_gui_subject(), { *it, *key_change }, input_cfg.game_gui);

										if (was_pressed) {
											continue;
										}
									}
								}
							}

							if (const auto it = mapped_or_nullptr(viewing_config.game_controls, key)) {
								if (e.uses_mouse() && game_gui_effective) {
									/* Leave it for the game gui */
								}
								else {
									out.intents.push_back({ *it, *key_change });

									if (was_pressed) {
										continue;
									}
								}
							}
						}

						if (direct_gameplay && e.msg == message::mousemotion) {
							raw_game_motion m;
							m.motion = game_motion_type::MOVE_CROSSHAIR;
							m.offset = e.data.mouse.rel;

							out.motions.emplace_back(m);
							continue;
						}

						if (was_released || should_draw_game_gui()) {
							if (get_game_gui_subject()) {
								if (game_gui.control_gui_world(create_game_gui_context(), e)) {
									continue;
								}
							}
						}
					}
				}

				/* 
					Notice that window inputs do not propagate
					beyond the closing of this scope.
				*/

				return out;
			};

			auto reload_needed_viewables = [&]() {
				visit_current_setup(
					[&](const auto& setup) {
						using T = remove_cref<decltype(setup)>;
						using S = viewables_loading_type;

						constexpr auto s = T::loading_strategy;

						if constexpr(s == S::LOAD_ALL || s == S::LOAD_ALL_ONLY_ONCE) {
							load_all(setup.get_viewable_defs());
						}
						else if constexpr(s == S::LOAD_ONLY_NEAR_CAMERA) {
							static_assert(always_false_v<T>, "Unimplemented");
						}
						else {
							static_assert(always_false_v<T>, "Unknown viewables loading strategy.");
						}
					}
				);
			};

			auto finalize_loading_viewables = [&](const bool measure_atlas_uploading) {
				streaming.finalize_load({
					audio_buffers,
					get_current_frame_num(),
					measure_atlas_uploading,
					get_general_renderer(),
					get_audiovisuals().get<sound_system>()
				});
			};

			auto do_advance_setup = [&](const augs::audio_renderer* const audio_renderer, auto& with_result) {
				/* 
					Advance the current setup's logic,
					and let the audiovisual_state sample the game world 
					that it chooses via get_viewed_cosmos.

					This also advances the audiovisual state, based on the cosmos returned by the setup.
				*/

				auto scope = measure_scope(game_thread_performance.advance_setup);
				advance_current_setup(audio_renderer, frame_delta, with_result);
			};

			auto create_viewing_game_gui_context = [&](augs::renderer& chosen_renderer, const config_lua_table& viewing_config) {
				return viewing_game_gui_context {
					make_create_game_gui_context(viewing_config)(),

					{
						get_audiovisuals().get<interpolation_system>(),
						get_audiovisuals().world_hover_highlighter,
						viewing_config.hotbar,
						viewing_config.drawing,
						viewing_config.inventory_gui_controls,
						get_camera_eye(viewing_config),
						get_drawer_for(chosen_renderer)
					}
				};
			};

			/* View lambdas */ 

			auto setup_the_first_fbo = [&](augs::renderer& chosen_renderer) {
				chosen_renderer.set_viewport({ vec2i{0, 0}, screen_size });

				const bool rendering_flash_afterimage = gameplay_camera.is_flash_afterimage_requested();

				if (rendering_flash_afterimage) {
					necessary_fbos.flash_afterimage->set_as_current(chosen_renderer);
				}
				else {
					augs::graphics::fbo::set_current_to_none(chosen_renderer);
				}

				chosen_renderer.clear_current_fbo();
			};

			auto draw_debug_lines = [&](augs::renderer& chosen_renderer, const config_lua_table& new_viewing_config) {
				if (DEBUG_DRAWING.enabled) {
					auto scope = measure_scope(game_thread_performance.debug_lines);

					const auto viewed_character = get_viewed_character();

					::draw_debug_lines(
						viewed_character.get_cosmos(),
						chosen_renderer,
						get_interpolation_ratio(),
						get_drawer_for(chosen_renderer).default_texture,
						new_viewing_config,
						get_camera_cone(new_viewing_config)
					);
				}
			};

			auto setup_standard_projection = [&](augs::renderer& chosen_renderer) {
				necessary_shaders.standard->set_projection(chosen_renderer, augs::orthographic_projection(vec2(screen_size)));
			};

			auto draw_game_gui = [&](augs::renderer& chosen_renderer, const config_lua_table& viewing_config) {
				auto scope = measure_scope(game_thread_performance.draw_game_gui);

				game_gui.world.draw(create_viewing_game_gui_context(chosen_renderer, viewing_config));
			};

			auto make_draw_setup_gui_input = [&](augs::renderer& chosen_renderer, const config_lua_table& new_viewing_config) {
				return draw_setup_gui_input {
					all_visible,
					get_camera_cone(new_viewing_config),
					get_blank_texture(),
					new_viewing_config,
					streaming.necessary_images_in_atlas,
					std::addressof(streaming.get_general_atlas()),
					std::addressof(streaming.avatars.texture),
					streaming.images_in_atlas,
					streaming.avatars.in_atlas,
					chosen_renderer,
					common_input_state.mouse.pos,
					screen_size,
					streaming.get_loaded_gui_fonts(),
					necessary_sounds,
					visit_current_setup([&](auto& setup) { return setup.find_player_metas(); }),
					game_gui_mode_flag,
					is_replaying_demo(),
					new_viewing_config.streamer_mode,
					new_viewing_config.streamer_mode_flags,
				};
			};

			auto draw_setup_custom_gui_over_imgui = [&](augs::renderer& chosen_renderer, const config_lua_table& new_viewing_config) {
				visit_current_setup([&]<typename S>(S& setup) {
					if constexpr(std::is_same_v<editor_setup, remove_cref<S>>) {
						setup.draw_custom_gui_over_imgui(make_draw_setup_gui_input(chosen_renderer, new_viewing_config));
						chosen_renderer.call_and_clear_triangles();
					}
				});
			};

			auto draw_mode_and_setup_custom_gui = [&](augs::renderer& chosen_renderer, const config_lua_table& new_viewing_config) {
				auto scope = measure_scope(game_thread_performance.draw_setup_custom_gui);

				visit_current_setup([&](auto& setup) {
					setup.draw_custom_gui(make_draw_setup_gui_input(chosen_renderer, new_viewing_config));
					chosen_renderer.call_and_clear_lines();
				});

				if (new_viewing_config.hud_messages.is_enabled) {
					hud_messages.draw(
						chosen_renderer,
						get_blank_texture(),
						streaming.get_loaded_gui_fonts().gui,
						screen_size,
						new_viewing_config.hud_messages.value,
						frame_delta
					);
				}
			};

			auto fallback_overlay_gray_color = [&](augs::renderer& chosen_renderer) {
				streaming.get_general_atlas().set_as_current(chosen_renderer);

				necessary_shaders.standard->set_as_current(chosen_renderer);
				setup_standard_projection(chosen_renderer);

				get_drawer_for(chosen_renderer).color_overlay(screen_size, darkgray);
			};

			auto draw_and_choose_menu_cursor = [&](augs::renderer& chosen_renderer, auto&& create_menu_context) {
				auto scope = measure_scope(game_thread_performance.menu_gui);

				auto get_drawer = [&]() {
					return get_drawer_for(chosen_renderer);
				};

				if (has_current_setup()) {
					if (ingame_menu.show) {
						const auto context = create_menu_context(ingame_menu);
						ingame_menu.advance(context, frame_delta);

						return ingame_menu.draw({ context, get_drawer() });
					}

					return assets::necessary_image_id::INVALID;
				}
				else {
					const auto context = create_menu_context(main_menu_gui);

					main_menu_gui.advance(context, frame_delta);

#if MENU_ART
					get_drawer().aabb(streaming.necessary_images_in_atlas[assets::necessary_image_id::ART_1], ltrb(0, 0, screen_size.x, screen_size.y), white);
#endif

					const auto cursor = main_menu_gui.draw({ context, get_drawer() });

					main_menu->draw_overlays(
						last_update_result,
						get_drawer(),
						streaming.necessary_images_in_atlas,
						streaming.get_loaded_gui_fonts().gui,
						screen_size
					);

					return cursor;
				}
			};

			auto draw_non_menu_cursor = [&](augs::renderer& chosen_renderer, const config_lua_table& viewing_config, const assets::necessary_image_id menu_chosen_cursor) {
				const bool should_draw_our_cursor = viewing_config.window.draws_own_cursor() && !window.is_mouse_pos_paused();
				const auto cursor_drawing_pos = common_input_state.mouse.pos;

				auto get_drawer = [&]() {
					return get_drawer_for(chosen_renderer);
				};

				if (ImGui::GetIO().WantCaptureMouse) {
					if (should_draw_our_cursor) {
						get_drawer().cursor(streaming.necessary_images_in_atlas, augs::imgui::get_cursor<assets::necessary_image_id>(), cursor_drawing_pos, white);
					}
				}
				else if (menu_chosen_cursor != assets::necessary_image_id::INVALID) {
					/* We must have drawn some menu */

					if (should_draw_our_cursor) {
						get_drawer().cursor(streaming.necessary_images_in_atlas, menu_chosen_cursor, cursor_drawing_pos, white);
					}
				}
				else if (game_gui_mode && should_draw_game_gui()) {
					if (get_viewed_character()) {
						const auto& character_gui = game_gui.get_character_gui(get_game_gui_subject());

						character_gui.draw_cursor_with_tooltip(create_viewing_game_gui_context(chosen_renderer, viewing_config), should_draw_our_cursor);
					}
				}
				else {
					if (should_draw_our_cursor) {
						get_drawer().cursor(streaming.necessary_images_in_atlas, assets::necessary_image_id::GUI_CURSOR, cursor_drawing_pos, white);
					}
				}
			};

			auto make_illuminated_rendering_input = [&](augs::renderer& chosen_renderer, const config_lua_table& viewing_config) {
				thread_local std::vector<additional_highlight> highlights;
				highlights.clear();

				visit_current_setup([&](const auto& setup) {
					using T = remove_cref<decltype(setup)>;

					if constexpr(T::has_additional_highlights) {
						setup.for_each_highlight([&](auto&&... args) {
							highlights.push_back({ std::forward<decltype(args)>(args)... });
						});
					}
				});

				thread_local std::vector<special_indicator> special_indicators;
				special_indicators.clear();
				special_indicator_meta indicator_meta;

				const auto viewed_character = get_viewed_character();

				if (viewed_character) {
					visit_current_setup([&](const auto& setup) {
						setup.on_mode_with_input(
							[&](const auto&... args) {
								::gather_special_indicators(
									args..., 
									viewed_character.get_official_faction(), 
									streaming.necessary_images_in_atlas, 
									special_indicators,
									indicator_meta,
									viewed_character
								);
							}
						);
					});
				}

				auto cone = get_camera_cone(viewing_config);
				cone.eye.transform.pos.discard_fract();

				return illuminated_rendering_input {
					{ viewed_character, cone },
					get_camera_requested_fov_expansion(),
					get_camera_edge_zoomout_mult(),
					get_queried_cone(viewing_config),
					calc_pre_step_crosshair_displacement(viewing_config),
					get_audiovisuals(),
					viewing_config.drawing,
					streaming.necessary_images_in_atlas,
					streaming.get_loaded_gui_fonts(),
					streaming.images_in_atlas,
					get_interpolation_ratio(),
					chosen_renderer,
					game_thread_performance,
					!has_current_setup() ? std::addressof(streaming.get_general_or_blank()) : std::addressof(streaming.get_general_atlas()),
					necessary_fbos,
					necessary_shaders,
					all_visible,
					viewing_config.performance,
					viewing_config.renderer,
					highlights,
					special_indicators,
					indicator_meta,
					write_buffer.particle_buffers,
					viewing_config.damage_indication,
					cached_visibility.light_requests,
					viewing_config.streamer_mode && viewing_config.streamer_mode_flags.inworld_hud,
					thread_pool
				};
			};

			auto perform_illuminated_rendering = [&](const illuminated_rendering_input& input) {
				auto scope = measure_scope(game_thread_performance.rendering_script);

				illuminated_rendering(input);
			};

			auto draw_call_imgui = [&](augs::renderer& chosen_renderer) {
				chosen_renderer.call_and_clear_triangles();

				auto scope = measure_scope(game_thread_performance.imgui);

				chosen_renderer.draw_call_imgui(
					imgui_atlas, 
					std::addressof(streaming.get_general_atlas()), 
					std::addressof(streaming.avatars.texture), 
					std::addressof(streaming.avatar_preview_tex),
					std::addressof(streaming.ad_hoc.texture)
				);
			};

			auto do_flash_afterimage = [&](augs::renderer& chosen_renderer) {
				const auto flash_mult = gameplay_camera.get_effective_flash_mult();
				const bool rendering_flash_afterimage = gameplay_camera.is_flash_afterimage_requested();

				const auto viewed_character = get_viewed_character();

				auto get_drawer = [&]() {
					return get_drawer_for(chosen_renderer);
				};

				::handle_flash_afterimage(
					chosen_renderer,
					necessary_shaders,
					necessary_fbos,
					streaming.get_general_atlas(),
					viewed_character,
					get_drawer,
					flash_mult,
					rendering_flash_afterimage,
					screen_size
				);
			};

			auto show_performance_details = [&](augs::renderer& chosen_renderer) {
				auto scope = measure_scope(game_thread_performance.debug_details);

				const auto viewed_character = get_viewed_character();

				debug_summaries.acquire(
					viewed_character.get_cosmos(),
					game_thread_performance,
					network_performance,
					network_stats,
					server_stats,
					streaming.performance,
					streaming.general_atlas_performance,
					render_thread_performance,
					get_audiovisuals().performance
				);

				::show_performance_details(
					get_drawer_for(chosen_renderer),
					streaming.get_loaded_gui_fonts().gui,
					screen_size,
					viewed_character,
					debug_summaries
				);
			};

			auto show_recent_logs = [&](augs::renderer& chosen_renderer) {
				::show_recent_logs(
					get_drawer_for(chosen_renderer),
					streaming.get_loaded_gui_fonts().gui,
					screen_size
				);
			};

			/* Flow */

			if (should_quit_due_to_signal()) {
				return;
			}

			ensure_float_flags_hold();

			if (setup_requires_cursor()) {
				game_gui_mode = true;
			}

			reload_needed_viewables();
			finalize_loading_viewables(config.debug.measure_atlas_uploading);

			const auto input_result = perform_input_pass();
			const auto& new_viewing_config = input_result.viewing_config;

			auto audio_renderer = std::optional<augs::audio_renderer>();

			if (const auto audio_buffer = audio_buffers.map_write_buffer()) {
				audio_renderer.emplace(augs::audio_renderer { *audio_buffer });
			}

			do_advance_setup(audio_renderer ? std::addressof(audio_renderer.value()) : nullptr, input_result);

			auto create_menu_context = make_create_menu_context(new_viewing_config);
			auto create_game_gui_context = make_create_game_gui_context(new_viewing_config);

			/* 
				What follows is strictly view part,
				without advancement of any kind.
				
				No state is altered beyond this point,
				except for usage of graphical resources and profilers.
			*/

			/*
				Canonical rendering order of the Hypersomnia Universe:
				
				1.  Draw the cosmos in the vicinity of the viewed character.
					Both the cosmos and the character are specified by the current setup (main menu is a setup, too).
				
				2.	Draw the debug lines over the game world, if so is appropriate.
				
				3.	Draw the game GUI, if so is appropriate.
					Game GUI involves things like inventory buttons, hotbar and health bars.

				4.  Draw the mode GUI.
					Mode GUI involves things like team selection, weapon shop, round time remaining etc.

				5.	Draw either the main menu buttons, or the in-game menu overlay accessed by ESC.
					These two are almost identical, except the layouts of the first (e.g. tweened buttons) 
					may also be influenced by a playing intro.

				6.	Draw IMGUI, which is the highest priority GUI. 
					This involves settings window, developer console and the like.

				7.	Draw the GUI cursor. It may be:
						- The cursor of the IMGUI, if it wants to capture the mouse.
						- Or, the cursor of the main menu or the in-game menu overlay, if either is currently active.
						- Or, the cursor of the game gui, with maybe tooltip, with maybe dragged item's ghost, if we're in-game in GUI mode.
			*/

			setup_the_first_fbo(get_general_renderer());

			const auto viewed_character = get_viewed_character();
			const auto& viewed_cosmos = viewed_character.get_cosmos();
			const bool non_zero_cosmos = !viewed_cosmos.completely_unset();

			auto enqueue_visibility_jobs = [&]() {
				const auto& interp = get_audiovisuals().get<interpolation_system>();
				auto& light_requests = cached_visibility.light_requests;
				light_requests.clear();

				::for_each_vis_request(
					[&](const visibility_request& request) {
						light_requests.emplace_back(request);
					},

					viewed_cosmos,
					all_visible,

					get_audiovisuals().get<light_system>().per_entity_cache,
					interp,
					get_queried_cone(new_viewing_config).get_visible_world_rect_aabb()
				);

				auto fog_of_war = new_viewing_config.drawing.fog_of_war;
				fog_of_war.size *= get_camera_requested_fov_expansion();

				const auto viewed_character_transform = viewed_character ? viewed_character.find_viewing_transform(interp) : std::optional<transformr>();

#if BUILD_STENCIL_BUFFER
				const bool fog_of_war_effective = 
					viewed_character_transform.has_value() 
					&& fog_of_war.is_enabled()
				;
#else
				const bool fog_of_war_effective = false;
#endif

				::enqueue_visibility_jobs(
					thread_pool,

					viewed_cosmos,
					get_general_renderer().dedicated,
					cached_visibility,

					fog_of_war_effective,
					viewed_character,
					viewed_character_transform ? *viewed_character_transform : transformr(),
					fog_of_war
				);
			};

			const auto illuminated_input = make_illuminated_rendering_input(get_general_renderer(), new_viewing_config);

			auto illuminated_rendering_job = [&]() {
				/* #1 */
				perform_illuminated_rendering(illuminated_input);
				/* #2 */
				draw_debug_lines(get_general_renderer(), new_viewing_config);

				setup_standard_projection(get_general_renderer());
			};

			auto game_gui_job = [&]() {
				advance_game_gui(create_game_gui_context(), frame_delta);
				draw_game_gui(game_gui_renderer, new_viewing_config);
			};

			auto menu_chosen_cursor = assets::necessary_image_id::INVALID;

			auto post_game_gui_job = [&]() {
				auto& chosen_renderer = post_game_gui_renderer;

				if (non_zero_cosmos) {
					/* #4 */
					draw_mode_and_setup_custom_gui(chosen_renderer, new_viewing_config);
				}
				else {
					fallback_overlay_gray_color(chosen_renderer);
				}

				/* #5 */
				menu_chosen_cursor = draw_and_choose_menu_cursor(chosen_renderer, create_menu_context);

				/* #6 */
				draw_call_imgui(chosen_renderer);

				draw_setup_custom_gui_over_imgui(chosen_renderer, new_viewing_config);
			};

			auto place_final_drawcalls_synchronously = [&]() {
				auto& chosen_renderer = post_game_gui_renderer;

				/* #7 */
				draw_non_menu_cursor(chosen_renderer, new_viewing_config, menu_chosen_cursor);

				do_flash_afterimage(chosen_renderer);
			};

			auto show_performance_details_job = [&]() {
				show_performance_details(debug_details_renderer);
			};

			auto show_recent_logs_job = [&]() {
				show_recent_logs(debug_details_renderer);
			};

			enqueue_visibility_jobs();

			if (new_viewing_config.session.show_performance) {
				thread_pool.enqueue(show_performance_details_job);
			}

			if (new_viewing_config.session.show_logs && !config.streamer_mode) {
				thread_pool.enqueue(show_recent_logs_job);
			}

			if (non_zero_cosmos) {
				thread_pool.enqueue(illuminated_rendering_job);

				/* #3 */
				if (should_draw_game_gui()) {
					thread_pool.enqueue(game_gui_job);
				}

				::enqueue_illuminated_rendering_jobs(thread_pool, illuminated_input);
			}

			thread_pool.enqueue(post_game_gui_job);

			thread_pool.submit();

			auto scope = measure_scope(game_thread_performance.main_help);

			thread_pool.help_until_no_tasks();
			thread_pool.wait_for_all_tasks_to_complete();

			/* 
				This task is dependent upon completion of two other tasks: 
				- game_gui_job
				- post_game_gui_job
			*/

			place_final_drawcalls_synchronously();
		};

		while (!should_quit) {
			auto extract_num_total_drawn_triangles = [&]() {
				return get_write_buffer().renderers.extract_num_total_triangles_drawn();
			};

			auto finalize_frame_and_swap = [&]() {
				for (auto& r : get_write_buffer().renderers.all) {
					r.call_and_clear_triangles();
				}

				visit_current_setup([&](auto& s) {
					return s.after_all_drawcalls(get_write_buffer());
				});

				game_thread_performance.num_triangles.measure(extract_num_total_drawn_triangles());

				buffer_swapper.wait_swap();

				{
					auto& write_buffer = get_write_buffer();

					write_buffer.renderers.next_frame();
					write_buffer.particle_buffers.clear();
				}
			};

			{
				auto scope = measure_scope(game_thread_performance.total);

#if IS_PRODUCTION_BUILD
				try {
					prepare_next_game_frame();
				}
				catch (const entity_creation_error& err) {
					LOG("Failed to create entity: %x", err.what());
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
				catch (const std::runtime_error& err) {
					LOG("Runtime error: %x", err.what());
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
				catch (const std::logic_error& err) {
					LOG("Logic error: %x", err.what());
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
				catch (const std::exception& err) {
					LOG("Exception: %x", err.what());
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
				catch (const std::ios_base::failure& err) {
					LOG("std::ios_base::failure: %x", err.what());
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
				catch (...) {
					LOG("Unknown error.");
					game_thread_result = work_result::FAILURE;
					request_quit();
				}
#else
				prepare_next_game_frame();
#endif
			}

			auto scope = measure_scope(game_thread_performance.main_wait);
			finalize_frame_and_swap();
		}
	};

	auto game_thread = std::thread(game_thread_worker);

	auto audio_thread_joiner = augs::scope_guard([&]() { audio_buffers.quit(); });
	auto game_thread_joiner = augs::scope_guard([&]() { game_thread.join(); });

	request_quit = [&]() {
		get_write_buffer().should_quit = true;
		should_quit = true;
	};

	renderer_backend_result rendering_result;

	auto game_main_thread_synced_op = [&]() {
		auto scope = measure_scope(game_thread_performance.synced_op);

		/* 
			IMGUI is our top GUI whose priority precedes everything else. 
			It will eat from the window input vector that is later passed to the game and other GUIs.	
		*/

		configurables.apply_main_thread(get_read_buffer().new_settings);
		configurables.sync_back_into(config);

		if (config.session.show_performance) {
			const auto viewed_character = get_viewed_character();

			viewed_character.get_cosmos().profiler.prepare_summary_info();
			game_thread_performance.prepare_summary_info();
			network_performance.prepare_summary_info();
			streaming.performance.prepare_summary_info();

			if (streaming.finished_generating_atlas()) {
				streaming.general_atlas_performance.prepare_summary_info();
			}

			render_thread_performance.prepare_summary_info();
			get_audiovisuals().performance.prepare_summary_info();
		}

		for (const auto& f : rendering_result.imgui_lists_to_delete) {
			IM_DELETE(f);
		}

		visit_current_setup([&](auto& s) {
			s.do_game_main_thread_synced_op(rendering_result);
		});
	};

	augs::timer this_frame_timer;

	for (;;) {
		auto scope = measure_scope(render_thread_performance.fps);

		auto swap_window_buffers = [&]() {
			auto scope = measure_scope(render_thread_performance.swap_window_buffers);
			window.swap_buffers();

			if (!until_first_swap_measured) {
				LOG("Time until first swap: %x ms", until_first_swap.extract<std::chrono::milliseconds>());
				until_first_swap_measured = true;

				program_log::get_current().mark_last_init_log();
			}
		};

		{
			auto& read_buffer = get_read_buffer();

			if (read_buffer.should_recheck_current_context) {
				augs::window::get_current().check_current_context();

				read_buffer.should_recheck_current_context = false;
			}

			const auto swap_when = read_buffer.swap_when;

			if (swap_when == swap_buffers_moment::AFTER_HELPING_LOGIC_THREAD) {
				swap_window_buffers();
			}

			{
				auto scope = measure_scope(render_thread_performance.renderer_commands);

				rendering_result.clear();

				for (auto& r : read_buffer.renderers.all) {
					renderer_backend.perform(
						rendering_result,
						r.commands.data(),
						r.commands.size(),
						r.dedicated
					);
				}

				current_frame.fetch_add(1, std::memory_order_relaxed);
			}

			if (swap_when == swap_buffers_moment::AFTER_GL_COMMANDS) {
				swap_window_buffers();
			}
		}

		{
			{
				auto& read_buffer = get_read_buffer();

				auto scope = measure_scope(render_thread_performance.local_entropy);

				auto& next_entropy = read_buffer.new_window_entropy;
				next_entropy.clear();

				window.collect_entropy(next_entropy);

				read_buffer.screen_size = window.get_screen_size();
			}

			{
				auto scope = measure_scope(render_thread_performance.render_help);
				thread_pool.sleep_until_tasks_posted();
				thread_pool.help_until_no_tasks();
			}

			{
				auto scope = measure_scope(render_thread_performance.render_wait);
				buffer_swapper.swap_buffers(game_main_thread_synced_op);

				const auto max_fps = get_read_buffer().max_fps;

				if (max_fps.is_enabled && max_fps.value >= 10) {
					const auto target_delay_ms = 1000.0 / max_fps.value;
					const auto method = get_read_buffer().max_fps_method;

					auto passed_ms = this_frame_timer.get<std::chrono::milliseconds>();

					while (passed_ms < target_delay_ms) {
						switch (method) {
							case augs::max_fps_type::SLEEP:
							{
								const auto to_sleep_ms = target_delay_ms - passed_ms;

								std::this_thread::sleep_for(std::chrono::duration<double>(to_sleep_ms / 1000.0));
							}
							case augs::max_fps_type::SLEEP_ZERO:
								yojimbo_sleep(0);
								break;
							case augs::max_fps_type::YIELD:
								std::this_thread::yield();
								break;
							case augs::max_fps_type::BUSY:
								break;
							default:
								break;
						}

						passed_ms = this_frame_timer.get<std::chrono::milliseconds>();
					}

					this_frame_timer.reset();
				}
			}
		}

		auto& read_buffer = get_read_buffer();

		if (window.is_active() && read_buffer.should_clip_cursor) {
			window.set_cursor_clipping(true);
			window.set_cursor_visible(false);
		}
		else {
			window.set_cursor_clipping(false);
			window.set_cursor_visible(true);
		}

		window.set_mouse_pos_paused(read_buffer.should_pause_cursor);

		if (read_buffer.should_quit) {
			break;
		}
	}

	return game_thread_result;
}
catch (const config_read_error& err) {
	LOG("Failed to read the initial config for the game!\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::imgui_init_error& err) {
	LOG("Failed init imgui:\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::audio_error& err) {
	LOG("Failed to establish the audio context:\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::window_error& err) {
	LOG("Failed to create an OpenGL window:\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::graphics::renderer_error& err) {
	LOG("Failed to initialize the renderer: %x", err.what());
	return work_result::FAILURE;
}
catch (const necessary_resource_loading_error& err) {
	LOG("Failed to load a resource necessary for the game to function!\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::lua_state_creation_error& err) {
	LOG("Failed to create a lua state for the game!\n%x", err.what());
	return work_result::FAILURE;
}
catch (const augs::unit_test_session_error& err) {
	LOG("Unit test session failure:\n%x\ncout:%x\ncerr:%x\nclog:%x\n", 
		err.what(), err.cout_content, err.cerr_content, err.clog_content
	);

	return work_result::FAILURE;
}
catch (const netcode_socket_raii_error& err) {
	LOG("Failed to create a socket for server browser:\n%x", err.what());
	return work_result::FAILURE;
}