#pragma once
#include "augs/math/vec2.h"
#include "augs/misc/imgui/standard_window_mixin.h"
#include "application/setups/server/server_instance_type.h"
#include "augs/misc/timing/timer.h"
#include "3rdparty/yojimbo/netcode.io/netcode.h"
#include "augs/network/netcode_sockets.h"
#include "augs/network/netcode_socket_raii.h"
#include "application/setups/client/client_connect_string.h"
#include "view/faction_view_settings.h"
#include "application/gui/server_list_entry.h"

#include <chrono>

struct nat_detection_settings;
struct browse_servers_gui_internal;
struct netcode_socket_t;
struct host_with_default_port;
struct resolve_address_result;

using official_addrs = std::vector<resolve_address_result>;

struct browse_servers_input {
	const host_with_default_port& server_list_provider;
	client_connect_string& client_connect;
	std::string& displayed_connecting_server_name;

	const std::vector<std::string>& official_arena_servers;
	const faction_view_settings& faction_view;
	const bool streamer_mode;
};

struct server_details_gui_state : public standard_window_mixin<server_details_gui_state> {
	using base = standard_window_mixin<server_details_gui_state>;
	using base::base;

	bool show_ips = false;

	bool perform(const server_list_entry&, const faction_view_settings&, const bool streamer_mode);
	void perform_online_players(const server_list_entry&, const faction_view_settings&, const bool streamer_mode);
};

class browse_servers_gui_state : public standard_window_mixin<browse_servers_gui_state> {
	netcode_socket_raii server_browser_socket;

	std::unique_ptr<browse_servers_gui_internal> data;

	void show_server_list(
		const std::string& label,
		const std::vector<server_list_entry*>&,
		const faction_view_settings&,
		const bool streamer_mode
	);

	std::optional<std::string> requested_connection;
	std::string displayed_connecting_server_name;

	net_time_t when_last_started_refreshing_server_list = 0;

	std::string error_message;

	std::vector<server_list_entry*> local_server_list;
	std::vector<server_list_entry*> official_server_list;
	std::vector<server_list_entry*> community_server_list;

	std::vector<server_list_entry> server_list;

	bool scroll_once_to_selected = false;
	server_list_entry selected_server;
	official_addrs official_server_addresses;

	server_details_gui_state server_details = std::string("Server details");

	bool only_responding = false;
	augs::maybe<int> at_least_players = augs::maybe<int>(1, false);
	augs::maybe<uint32_t> at_most_ping = augs::maybe<uint32_t>(100, false);

	int sort_by_column = 0;
	bool ascending = true;
	std::string loading_dots;
	uint64_t ping_sequence_counter = 0;

	void refresh_server_pings();

	server_list_entry* find_entry(const netcode_address_t&);
	server_list_entry* find_entry_by_internal_address(const netcode_address_t&, uint64_t ping_sequence);

	const resolve_address_result* find_resolved_official(const netcode_address_t&);

	bool handle_gameserver_response(const netcode_address_t& from, uint8_t* packet_buffer, std::size_t packet_bytes);

	void animate_dot_column();
	void handle_incoming_udp_packets(netcode_socket_t&);
	void send_pings_and_punch_requests(netcode_socket_t&);

	void refresh_custom_connect_strings();
public:

	using base = standard_window_mixin<browse_servers_gui_state>;
	browse_servers_gui_state(const std::string& title);
	~browse_servers_gui_state();

	bool perform(browse_servers_input);

	void sync_download_server_entry(browse_servers_input, const client_connect_string& in);

	void advance_ping_logic();

	void reping_all_servers();

	const server_list_entry* find_entry(const client_connect_string& in) const;
	const server_list_entry* find_best_server() const;
	const server_list_entry* find_best_ranked() const;
	void refresh_server_list(browse_servers_input);

	bool refreshed_at_least_once() const;

	void select_server(const server_list_entry&);

	bool refresh_in_progress() const;
};
