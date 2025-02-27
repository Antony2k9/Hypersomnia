#pragma once
#include "augs/filesystem/path.h"
#include "augs/app_type.h"
#include "augs/network/network_types.h"

struct cmd_line_params {
	std::string complete_command_line;
	std::string parsed_as;

	std::string live_log_path;

	std::optional<augs::path_type> calling_cwd;
	augs::path_type appdata_dir;

	augs::path_type exe_path;
	augs::path_type appimage_path;
	augs::path_type debugger_target;
	augs::path_type editor_target;
	augs::path_type consistency_report;
	bool unit_tests_only = false;
	bool help_only = false;
	bool version_only = false;
	bool version_line_only = false;
	bool start_server = false;
	app_type type = app_type::GAME_CLIENT;
	bool upgraded_successfully = false;
	bool should_connect = false;
	bool only_check_update_availability_and_quit = false;
	bool keep_cwd = false;
	bool sync_external_arenas = false;
	bool sync_external_arenas_and_quit = false;
	std::optional<int> autoupdate_delay;

	augs::path_type apply_config;

	int test_fp_consistency = -1;
	std::string connect_address;

	bool no_router = false;

	bool suppress_server_webhook = false;

	bool no_update_on_launch = false;
	bool update_once_now = false;
	bool daily_autoupdate = false;

	std::optional<port_type> first_udp_command_port;
	std::optional<port_type> server_list_port;
	std::optional<port_type> server_port;

	bool is_updater = false;
	augs::path_type verified_archive;
	augs::path_type verified_signature;

	cmd_line_params(const int argc, const char* const * const argv) {
		exe_path = argv[0];
		complete_command_line = exe_path.string();
		parsed_as = std::string();

		for (int i = 1; i < argc; ++i) {
			const auto a = std::string(argv[i]);

			complete_command_line += " " + a;
			parsed_as += typesafe_sprintf("%x=%x ", i, a);
		}

		for (int i = 1; i < argc; ++i) {
			auto get_next = [&i, argc, argv]() {
				if (i + 1 < argc) {
					return argv[++i];
				}

				return "";
			};

			const auto a = std::string(argv[i]);

			if (begins_with(a, "-psn")) {
				/* MacOS process identifier. Ignore. */
				continue;
			}
			else if (a == "--keep-cwd") {
				keep_cwd = true;
			}
			else if (a == "--appimage-path") {
				appimage_path = get_next();
				exe_path = appimage_path;
			}
			else if (a == "--unit-tests-only") {
				unit_tests_only = true;
				keep_cwd = true;
			}
			else if (a == "--help" || a == "-h") {
				help_only = true;
			}
			else if (a == "--version-line") {
				version_line_only = true;
			}
			else if (a == "--version" || a == "-v") {
				version_only = true;
			}
			else if (a == "--is-update-available") {
				only_check_update_availability_and_quit = true;
			}
			else if (a == "--server") {
				start_server = true;
			}
			else if (a == "--upgraded-successfully") {
				upgraded_successfully = true;
			}
			else if (a == "--dedicated-server") {
				type = app_type::DEDICATED_SERVER;
			}
			else if (a == "--delayed-autoupdate") {
				daily_autoupdate = true;
				autoupdate_delay = std::atoi(get_next());
			}
			else if (a == "--server-port") {
				server_port = std::atoi(get_next());
			}
			else if (a == "--no-router") {
				no_router = true;
			}
			else if (a == "--no-update-on-launch") {
				no_update_on_launch = true;
			}
			else if (a == "--daily-autoupdate") {
				daily_autoupdate = true;
			}
			else if (a == "--update-once-now") {
				update_once_now = true;
			}
			else if (a == "--suppress-server-webhook") {
				suppress_server_webhook = true;
			}
			else if (a == "--masterserver") {
				type = app_type::MASTERSERVER;
			}
			else if (a == "--test-fp-consistency") {
				test_fp_consistency = std::atoi(get_next());
				keep_cwd = true;
			}
			else if (a == "--nat-punch-port") {
				first_udp_command_port = std::atoi(get_next());
			}
			else if (a == "--server-list-port") {
				server_list_port = std::atoi(get_next());
			}
			else if (a == "--consistency-report") {
				consistency_report = get_next();
			}
			else if (a == "--verify-updater") {
				is_updater = true;
				verified_archive = get_next();
			}
			else if (a == "--verify") {
				verified_archive = get_next();
			}
			else if (a == "--apply-config") {
				apply_config = get_next();
			}
			else if (a == "--edit") {
				editor_target = get_next();
			}
			else if (a == "--signature") {
				verified_signature = get_next();
			}
			else if (a == "--connect") {
				should_connect = true;
				connect_address = get_next();
			}
			else if (a == "--live-log") {
				live_log_path = get_next();
			}
			else if (a == "--sync-external-arenas") {
				sync_external_arenas = true;
			}
			else if (a == "--sync-external-arenas-and-quit") {
				sync_external_arenas_and_quit = true;
			}
			else if (a == "--appdata-dir") {
				appdata_dir = get_next();
			}
			else if (a == "--calling-cwd") {
				calling_cwd = get_next();
			}
			else {

			}
		}
	}

	bool is_cli_tool() const {
		return type == app_type::MASTERSERVER || type == app_type::DEDICATED_SERVER;
	}
};