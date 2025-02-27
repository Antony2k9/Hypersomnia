#include <future>

#include "application/gui/browse_servers_gui.h"
#include "augs/network/netcode_utils.h"
#include "augs/misc/imgui/imgui_control_wrappers.h"
#include "3rdparty/include_httplib.h"
#include "augs/templates/thread_templates.h"
#include "augs/readwrite/memory_stream.h"
#include "augs/readwrite/pointer_to_buffer.h"
#include "augs/readwrite/byte_readwrite.h"
#include "application/setups/client/client_connect_string.h"
#include "augs/log.h"
#include "application/setups/debugger/detail/maybe_different_colors.h"
#include "augs/misc/time_utils.h"
#include "augs/network/netcode_sockets.h"
#include "application/nat/nat_detection_settings.h"
#include "application/network/resolve_address.h"
#include "application/masterserver/masterserver_requests.h"
#include "application/masterserver/gameserver_command_readwrite.h"
#include "augs/misc/httplib_utils.h"
#include "application/network/resolve_address.h"

constexpr auto ping_retry_interval = 1;
constexpr auto reping_interval = 10;
constexpr auto server_entry_timeout = 5;
constexpr auto max_packets_per_frame_v = 64;

#define LOG_BROWSER 1

template <class... Args>
void BRW_LOG(Args&&... args) {
#if LOG_BROWSER
	LOG(std::forward<Args>(args)...);
#else
	((void)args, ...);
#endif
}

#if LOG_MASTERSERVER
#define BRW_LOG_NVPS LOG_NVPS
#else
#define BRW_LOG_NVPS BRW_LOG
#endif

static uint32_t order_category(
	const server_list_entry& entry
) {
	const auto& a = entry.heartbeat;

	/*
		Order:

		0. Local servers
		1. Servers with players with no NAT (based on ping)
		2. Servers with players with NAT
		3. Empty servers with no NAT
		4. Empty servers with NAT
		5. Full servers with no NAT
		6. Full servers with NAT
	*/

	if (a.is_full()) {
		/* 
			Still consider it as there's always a chance it gets freed in the meantime
		*/

		if (a.nat.type == nat_type::PUBLIC_INTERNET) {
			return 1000;
		}

		return 10000;
	}

	if (entry.progress.found_on_internal_network) {
		return 0;
	}

	if (a.num_online > 0 		&& a.nat.type == nat_type::PUBLIC_INTERNET) {
		/* Non-empty public, best category */
		return 1; 
	}
	else if (a.num_online > 0) {
		/* Non-empty NAT */
		return 1 + static_cast<uint32_t>(a.nat.type); /* 2-5 */
	}
	else if (a.num_online == 0 && a.nat.type == nat_type::PUBLIC_INTERNET) {
		/* Empty public */
		return 10 + static_cast<uint32_t>(a.nat.type); /* 10-14 */
	}
	else {
		/* Empty NAT */
		return 100 + static_cast<uint32_t>(a.nat.type); /* 100-104 */
	}
}

static bool compare_servers(
	const server_list_entry& a,
	const server_list_entry& b
) {
	{
		const auto ca = order_category(a);
		const auto cb = order_category(b);
		
		if (ca != cb) {
			/* Favor better category */
			return ca < cb;
		}
	}

	if (a.progress.ping != b.progress.ping) {
		/* Favor closer one */
		return a.progress.ping < b.progress.ping;
	}

	/* Favor more recent one */
	return a.time_hosted > b.time_hosted;
}

bool server_list_entry::is_set() const {
	return heartbeat.server_name.size() > 0;
}

struct browse_servers_gui_internal {
	std::future<std::optional<httplib::Result>> future_response;
	netcode_socket_t socket;

	std::future<official_addrs> future_official_addresses;

	bool refresh_op_in_progress() const {
		return future_official_addresses.valid() || future_response.valid();
	}
};

browse_servers_gui_state::~browse_servers_gui_state() = default;

browse_servers_gui_state::browse_servers_gui_state(const std::string& title) 
	: base(title), data(std::make_unique<browse_servers_gui_internal>()) 
{

}

double yojimbo_time();

static std::vector<server_list_entry> to_server_list(std::optional<httplib::Result> result, std::string& error_message) {
    using namespace httplib_utils;

    if (result == std::nullopt || result.value() == nullptr) {
        error_message = "Couldn't connect to the server list host.";
        return {};
    }

    const auto& response = result.value();
    const auto status = response->status;

    LOG("Server list response status: %x", status);

    if (!successful(status)) {
        const auto couldnt_download = std::string("Couldn't download the server list.\n");

        error_message = couldnt_download + "HTTP response: " + std::to_string(status);
        return {};
    }

    const auto& bytes = response->body;

    LOG("Server list response bytes: %x", bytes.size());

    auto stream = augs::make_ptr_read_stream(bytes.data(), bytes.size());

	std::vector<server_list_entry> new_server_list;

    try {
        while (stream.has_unread_bytes()) {
            server_list_entry entry;

            augs::read_bytes(stream, entry.address);
            augs::read_bytes(stream, entry.time_hosted);
            augs::read_bytes(stream, entry.heartbeat);

            new_server_list.emplace_back(std::move(entry));
        }
    }
    catch (const augs::stream_read_error& err) {
        error_message = "There was a problem deserializing the server list:\n" + std::string(err.what()) + "\n\nTry restarting the game and updating your client!";
        new_server_list.clear();
    }

    return new_server_list;
}

void browse_servers_gui_state::sync_download_server_entry(
	const browse_servers_input in,
	const client_connect_string& server
) {
	LOG("Calling sync_download_server_entry.");

	/*
		For now this will just refresh the whole list
		so find_entry later returns a valid entry.
	*/

	(void)server;

	/* Todo: make async */
	auto lbd = 
		[address = in.server_list_provider]() -> std::optional<httplib::Result> {
			/* 
				Right now it's the same as the one that downloads the whole list,
				but we'll need to support downloading just one entry.
			*/

			const auto resolved = resolve_address(address);
			LOG(resolved.report());

			if (resolved.result != resolve_result_type::OK) {
				return std::nullopt;
			}

			auto resolved_addr = resolved.addr;
			const auto intended_port = resolved_addr.port;
			resolved_addr.port = 0;

			const auto address_str = ::ToString(resolved_addr);
			const auto timeout = 5;

			LOG("Connecting to server list at: %x:%x", address_str, intended_port);

			httplib::Client cli(address_str.c_str(), intended_port);
			cli.set_write_timeout(timeout);
			cli.set_read_timeout(timeout);
			cli.set_follow_location(true);

			return cli.Get("/server_list_binary");
		}
	;

	server_list = ::to_server_list(lbd(), error_message);
	refresh_custom_connect_strings();
}

bool browse_servers_gui_state::refreshed_at_least_once() const {
	return when_last_started_refreshing_server_list != 0;
}

bool browse_servers_gui_state::refresh_in_progress() const {
	return data->refresh_op_in_progress();
}

void browse_servers_gui_state::refresh_server_list(const browse_servers_input in) {
	using namespace httplib;

	if (refresh_in_progress()) {
		return;
	}

	official_server_addresses.clear();
	error_message.clear();
	server_list.clear();
	auto prev_addr = selected_server.address;
	selected_server = {};
	selected_server.address = prev_addr;

	LOG("Launching future_official_addresses");

	data->future_official_addresses = launch_async(
		[addresses=in.official_arena_servers]() {
			official_addrs results;

			LOG("Resolving %x official arena hosts for the server list.", addresses.size());

			for (const auto& a : addresses) {
				host_with_default_port in;

				in.address = a;
				in.default_port = 0;

				const auto result = resolve_address(in);
				LOG(result.report());

				if (result.result == resolve_result_type::OK) {
					results.emplace_back(result);
				}
			}

			return results;
		}
	);

	LOG("Launching future_response");

	data->future_response = launch_async(
		[address = in.server_list_provider]() -> std::optional<httplib::Result> {
			const auto resolved = resolve_address(address);
			LOG(resolved.report());

			if (resolved.result != resolve_result_type::OK) {
				return std::nullopt;
			}

			auto resolved_addr = resolved.addr;
			const auto intended_port = resolved_addr.port;
			resolved_addr.port = 0;

			const auto address_str = ::ToString(resolved_addr);
			const auto timeout = 5;

			LOG("Connecting to server list at: %x:%x", address_str, intended_port);

			httplib::Client cli(address_str.c_str(), intended_port);
			cli.set_write_timeout(timeout);
			cli.set_read_timeout(timeout);
			cli.set_follow_location(true);

			return cli.Get("/server_list_binary");
		}
	);

	LOG("refresh_server_list returns.");

	when_last_started_refreshing_server_list = yojimbo_time();
}

void browse_servers_gui_state::refresh_server_pings() {
	for (auto& s : server_list) {
		s.progress = {};
	}
}

std::string server_list_entry::get_connect_string() const {
	if (!custom_connect_string.empty()) {
		return custom_connect_string;
	}

	return ::ToString(get_connect_address());
}

netcode_address_t server_list_entry::get_connect_address() const {
	if (progress.found_on_internal_network && heartbeat.internal_network_address.has_value()) {
		return *heartbeat.internal_network_address;
	}

	return address;
}

bool server_list_entry::is_behind_nat() const {
	return heartbeat.is_behind_nat();
}

server_list_entry* browse_servers_gui_state::find_entry(const netcode_address_t& address) {
	for (auto& s : server_list) {
		if (s.address == address) {
			return &s;
		}
	}

	return nullptr;
}

server_list_entry* browse_servers_gui_state::find_entry_by_internal_address(const netcode_address_t& address, const uint64_t ping_sequence) {
	for (auto& s : server_list) {
		if (s.progress.ping_sequence == ping_sequence) {
			if (s.heartbeat.internal_network_address == address) {
				return &s;
			}
		}
	}

	return nullptr;
}

bool is_internal(const netcode_address_t& address) {
	if (address.type == NETCODE_ADDRESS_IPV4) {
		auto ip = address.data.ipv4;

		if (ip[0] == 127) {
			return true;
		}

		if (ip[0] == 10) {
			return true;
		}

		if (ip[0] == 172) {
			if (ip[1] >= 16 && ip[1] <= 31) {
				return true;
			}
		}

		if (ip[0] == 192) {
			if (ip[1] == 168) {
				return true;
			}
		}
	}

	// TODO
	return false;
}

bool browse_servers_gui_state::handle_gameserver_response(const netcode_address_t& from, uint8_t* packet_buffer, std::size_t packet_bytes) {
	const auto current_time = yojimbo_time();

	if (const auto maybe_sequence = read_ping_response(packet_buffer, packet_bytes)) {
		const auto sequence = *maybe_sequence;

		if (const auto entry = find_entry(from)) {
			BRW_LOG("Received a pingback (seq=%x) from an EXTERNAL game server at: %x", sequence, ::ToString(from));

			auto& progress = entry->progress;

			if (sequence == progress.ping_sequence) {
				BRW_LOG("Properly measured ping to %x: %x", ::ToString(from), progress.ping);

				progress.set_ping_from(current_time);
				progress.state = server_entry_state::PING_MEASURED;
			}
		}
		else if (const auto entry = find_entry_by_internal_address(from, sequence)) {
			auto& progress = entry->progress;

			BRW_LOG("Received a pingback (seq=%x) from an INTERNAL game server at: %x", sequence, ::ToString(from));

			progress.set_ping_from(current_time);
			progress.state = server_entry_state::PING_MEASURED;
			progress.found_on_internal_network = true;
		}
		else {
			BRW_LOG("WARNING! Received a pingback (seq=%x) from an UNKNOWN game server at: %x", sequence, ::ToString(from));
		}

		return true;
	}

	return false;
}

void browse_servers_gui_state::handle_incoming_udp_packets(netcode_socket_t& socket) {
	auto packet_handler = [&](const auto& from, uint8_t* buffer, const int bytes_received) {
		if (handle_gameserver_response(from, buffer, bytes_received)) {
			return;
		}
	};

	::receive_netcode_packets(socket, packet_handler);
}

void browse_servers_gui_state::animate_dot_column() {
	const auto current_time = yojimbo_time();

	const auto num_dots = uint64_t(current_time * 3) % 3 + 1;
	loading_dots = std::string(num_dots, '.');
}

void browse_servers_gui_state::advance_ping_logic() {
	animate_dot_column();

	auto& udp_socket = server_browser_socket.socket;

	handle_incoming_udp_packets(udp_socket);
	send_pings_and_punch_requests(udp_socket);
}

void browse_servers_gui_state::send_pings_and_punch_requests(netcode_socket_t& socket) {
	const auto current_time = yojimbo_time();

	auto interval_passed = [current_time](const auto last, const auto interval) {
		return current_time - last >= interval;
	};

	int packets_left = max_packets_per_frame_v;

	if (server_list.empty()) {
		return;
	}

	for (auto& s : server_list) {
		if (packets_left <= 0) {
			break;
		}

		auto& p = s.progress;

		using S = server_entry_state;

		auto request_ping = [&]() {
			p.when_sent_last_ping = current_time;
			p.ping_sequence = ping_sequence_counter++;

			ping_this_server(socket, s.address, p.ping_sequence);
			--packets_left;

			const auto& internal_address = s.heartbeat.internal_network_address;
			const bool maybe_reachable_internally = 
				internal_address.has_value()
				&& ::is_internal(*internal_address)
			;

			if (maybe_reachable_internally) {
				ping_this_server(socket, *internal_address, p.ping_sequence);
				--packets_left;
			}
		};

		if (p.state == S::AWAITING_RESPONSE) {
			auto& when_first_ping = p.when_sent_first_ping;
			auto& when_last_ping = p.when_sent_last_ping;

			if (try_fire_interval(ping_retry_interval, when_last_ping, current_time)) {
				if (when_first_ping < 0.0) {
					when_first_ping = current_time;
				}

				request_ping();
			}

			if (when_first_ping >= 0.0 && interval_passed(server_entry_timeout, when_first_ping)) {
				p.state = server_entry_state::GIVEN_UP;
				continue;
			}
		}

		if (p.state == S::PING_MEASURED) {
			auto& when_last_ping = p.when_sent_last_ping;

			auto reping_if_its_time = [&]() {
				if (!show) {
					/* Never reping if the window is hidden. */
					return;
				}

				if (interval_passed(reping_interval, when_last_ping)) {
					request_ping();
				}
			};

			reping_if_its_time();
		}
	}
}

constexpr auto num_columns = 6;

void browse_servers_gui_state::select_server(const server_list_entry& s) {
	selected_server = s;
	//server_details.open();
	scroll_once_to_selected = true;
}

void browse_servers_gui_state::show_server_list(
	const std::string& label,
	const std::vector<server_list_entry*>& server_list,
	const faction_view_settings& faction_view,
	const bool streamer_mode
) {
	using namespace augs::imgui;

	const auto& style = ImGui::GetStyle();

	if (server_list.empty()) {
		ImGui::NextColumn();
		text_disabled(typesafe_sprintf("No %x servers match the applied filters.", label));
		next_columns(num_columns - 1);
		return;
	}

	for (const auto& sp : server_list) {
		auto id = scoped_id(index_in(server_list, sp));
		const auto& s = *sp;
		const auto& progress = s.progress;
		const auto& d = s.heartbeat;

		const bool given_up = progress.state == server_entry_state::GIVEN_UP;
		const auto color = rgba(given_up ? style.Colors[ImGuiCol_TextDisabled] : style.Colors[ImGuiCol_Text]);

		auto do_text = [&](const auto& t) {
			text_color(t, color);
		};

		const bool is_selected = selected_server.address == s.address;

		if (progress.responding()) {
			const auto ping = progress.ping;

			if (ping > 999) {
				do_text("999>");
			}
			else {
				do_text(typesafe_sprintf("%x", ping));
			}
		}
		else {
			if (given_up) {
				do_text("?");
			}
			else {
				do_text(loading_dots);
			}
		}

		ImGui::NextColumn();

		{
			auto col_scope = scoped_style_color(ImGuiCol_Text, color);

			auto displayed_name = d.server_name;

			if (streamer_mode && s.is_community_server) {
				displayed_name = "Community server";
			}

			if (is_selected) {
				if (scroll_once_to_selected) {
					scroll_once_to_selected = false;

					ImGui::SetScrollHereY(0.0f);
				}
			}

			if (ImGui::Selectable(displayed_name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
				selected_server = s;
			}
		}

		if (ImGui::IsItemHovered()) {
			auto tool = scoped_tooltip();

			if (s.heartbeat.num_online == 0) {
				text_disabled("No players online.");
			}
			else {
				text_color(typesafe_sprintf("%x players online:", s.heartbeat.num_online), green);

				auto do_faction_summary = [&](auto faction, auto& entries) {
					if (entries.empty()) {
						return;
					}

					ImGui::Separator();

					const auto labcol = faction_view.colors[faction].current_player_text;

					for (auto& e : entries) {
						auto nickname = e.nickname;

						if (streamer_mode) {
							nickname = "Player";
						}

						text_color(nickname, labcol);
					}
				};

				do_faction_summary(faction_type::RESISTANCE, s.heartbeat.players_resistance);
				do_faction_summary(faction_type::METROPOLIS, s.heartbeat.players_metropolis);
				do_faction_summary(faction_type::SPECTATOR, s.heartbeat.players_spectating);
			}
		}
		
		if (ImGui::IsItemClicked()) {
			if (ImGui::IsMouseDoubleClicked(0)) {
				LOG("Double-clicked server list entry: %x (%x). Connecting.", d.server_name, ToString(s.address));

				displayed_connecting_server_name = s.heartbeat.server_name;

				requested_connection = s.get_connect_string();
			}
		}

		if (ImGui::IsItemClicked(1)) {
			selected_server = s;
			server_details.open();
		}

		ImGui::NextColumn();

		if (d.game_mode.size() > 0) {
			auto displayed_mode = std::string(d.game_mode);

			if (streamer_mode && s.is_community_server) {
				displayed_mode = "Game mode";
			}

			do_text(displayed_mode);
		}
		else {
			do_text("<unknown>");
		}

		ImGui::NextColumn();

		do_text(typesafe_sprintf("%x/%x", d.num_online, d.max_online));

		ImGui::NextColumn();

		{
			auto displayed_arena = std::string(d.current_arena);

			if (streamer_mode && s.is_community_server) {
				displayed_arena = "Community arena";
			}

			do_text(displayed_arena);
		}

		ImGui::NextColumn();

		const auto secs_ago = augs::date_time::secs_since_epoch() - s.time_hosted;
		text_disabled(augs::date_time::format_how_long_ago(true, secs_ago));

		ImGui::NextColumn();
	}

	scroll_once_to_selected = false;
}

const resolve_address_result* browse_servers_gui_state::find_resolved_official(const netcode_address_t& n) {
	for (const auto& o : official_server_addresses) {
		auto compared_for_officiality = o.addr;
		compared_for_officiality.port = 0;

		auto candidate = n;
		candidate.port = 0;

		if (candidate == compared_for_officiality) {
			return &o;
		}
	}

	return nullptr;
}

void browse_servers_gui_state::refresh_custom_connect_strings() {
	for (auto& s : server_list) {
		if (const auto resolved = find_resolved_official(s.address)) {
			if (s.address.port == DEFAULT_GAME_PORT_V) {
				s.custom_connect_string = resolved->host;
			}
			else {
				s.custom_connect_string = typesafe_sprintf("%x:%x", resolved->host, s.address.port);
			}
		}
		else {
			s.custom_connect_string.clear();
		}
	}
}

bool browse_servers_gui_state::perform(const browse_servers_input in) {
	using namespace httplib_utils;

	if (valid_and_is_ready(data->future_response)) {
		server_list = ::to_server_list(data->future_response.get(), error_message);
		refresh_custom_connect_strings();
	}

	if (valid_and_is_ready(data->future_official_addresses)) {
		official_server_addresses = data->future_official_addresses.get();
		refresh_custom_connect_strings();
	}

	if (!show) {
		return false;
	}

	using namespace augs::imgui;

	centered_size_mult = vec2(0.8f, 0.7f);

	auto imgui_window = make_scoped_window(ImGuiWindowFlags_NoSavedSettings);

	if (!imgui_window) {
		return false;
	}

	thread_local ImGuiTextFilter filter;

	filter_with_hint(filter, "##HierarchyFilter", "Filter server names/arenas...");
	local_server_list.clear();
	official_server_list.clear();
	community_server_list.clear();

	bool has_local_servers = false;
	bool has_official_servers = false;
	bool has_community_servers = false;

	requested_connection = std::nullopt;

	for (auto& s : server_list) {
		const auto& name = s.heartbeat.server_name;
		const auto& arena = s.heartbeat.current_arena;
		const auto effective_name = std::string(name) + " " + std::string(arena);

		auto push_if_passes = [&](auto& target_list) {
			if (only_responding) {
				if (!s.progress.responding()) {
					return;
				}
			}

			if (at_least_players.is_enabled) {
				if (s.heartbeat.num_online < at_least_players.value) {
					return;
				}
			}

			if (at_most_ping.is_enabled) {
				if (s.progress.ping > at_most_ping.value) {
					return;
				}
			}

			if (!filter.PassFilter(effective_name.c_str())) {
				return;
			}

			target_list.push_back(&s);
		};

		const bool is_internal = s.progress.found_on_internal_network;

		if (is_internal) {
			has_local_servers = true;
			s.is_community_server = false;
			push_if_passes(local_server_list);
		}
		else {
			if (const bool is_official = !s.custom_connect_string.empty()) {
				has_official_servers = true;
				s.is_community_server = false;
				push_if_passes(official_server_list);
			}
			else {
				has_community_servers = true;
				s.is_community_server = true;
				push_if_passes(community_server_list);
			}
		}
	}

	{
		using T = const server_list_entry*;

		auto by_ping = [](const T& a, const T& b) {
			return a->progress.ping < b->progress.ping;
		};

		auto by_name = [](const T& a, const T& b) {
			return a->heartbeat.server_name < b->heartbeat.server_name;
		};

		auto by_mode = [](const T& a, const T& b) {
			return a->heartbeat.game_mode < b->heartbeat.game_mode;
		};

		auto by_players = [](const T& a, const T& b) {
			return a->heartbeat.num_online < b->heartbeat.num_online;
		};

		auto by_arena = [](const T& a, const T& b) {
			return a->heartbeat.current_arena < b->heartbeat.current_arena;
		};

		auto by_appeared = [](const T& a, const T& b) {
			return a->time_hosted > b->time_hosted;
		};

		auto make_comparator = [&](auto op1, auto op2, auto op3) {
			bool asc = ascending;

			return [asc, op1, op2, op3](const T& a, const T& b) {
				const auto a_less_b = op1(a, b);
				const auto b_less_a = op1(b, a);

				if (!a_less_b && !b_less_a) {
					const auto _2_a_less_b = op2(a, b);
					const auto _2_b_less_a = op2(b, a);

					if (!_2_a_less_b && !_2_b_less_a) {
						return op3(a, b);
					}

					return _2_a_less_b;
				}
				
				return asc ? a_less_b : b_less_a;
			};
		};

		auto sort_entries = [&]() {
			const auto c = sort_by_column;

			auto sort_by = [&](auto... cmps) {
				sort_range(local_server_list, make_comparator(cmps...));
				sort_range(official_server_list, make_comparator(cmps...));
				sort_range(community_server_list, make_comparator(cmps...));
			};

			switch (c) {
				case 0: 
					return sort_by(by_ping, by_appeared, by_name);

				case 1: 
					return sort_by(by_name, by_ping, by_appeared);

				case 2: 
					return sort_by(by_mode, by_ping, by_appeared);

				case 3: 
					return sort_by(by_players, by_ping, by_appeared);

				case 4: 
					return sort_by(by_arena, by_ping, by_appeared);

				case 5: 
					return sort_by(by_appeared, by_ping, by_players);
					
				default:
					return sort_by(by_ping, by_appeared, by_name);
			}
		};

		sort_entries();
	}

	ImGui::Separator();

	if (when_last_started_refreshing_server_list == 0) {
		refresh_server_list(in);
	}

	auto do_list_view = [&](bool disable_content_view) {
		auto child = scoped_child("list view", ImVec2(0, 2 * -(ImGui::GetFrameHeightWithSpacing() + 4)));

		if (disable_content_view) {
			return;
		}

		ImGui::Columns(num_columns);
		ImGui::SetColumnWidth(0, ImGui::CalcTextSize("9999999").x);
		ImGui::SetColumnWidth(1, ImGui::CalcTextSize("9").x * max_server_name_length_v - 4);
		ImGui::SetColumnWidth(2, ImGui::CalcTextSize("Capture the fla(FFA)").x);
		ImGui::SetColumnWidth(3, ImGui::CalcTextSize("Players  9").x);
		ImGui::SetColumnWidth(4, ImGui::CalcTextSize("9").x * (max_arena_name_length_v / 2));
		ImGui::SetColumnWidth(5, ImGui::CalcTextSize("9").x * 25);

		int current_col = 0;

		auto do_column = [&](std::string label, std::optional<rgba> col = std::nullopt) {
			const bool is_current = current_col == sort_by_column;

			if (is_current) {
				label += ascending ? "▲" : "▼";
			}

			const auto& style = ImGui::GetStyle();

			auto final_color = rgba(is_current ? style.Colors[ImGuiCol_Text] : style.Colors[ImGuiCol_TextDisabled]);

			if (col.has_value()) {
				final_color = *col;
			}

			auto col_scope = scoped_style_color(ImGuiCol_Text, final_color);

			if (ImGui::Selectable(label.c_str())) {
				if (is_current) {
					ascending = !ascending;
				}
				else 
				{
					sort_by_column = current_col;
					ascending = true;
				}
			}

			++current_col;
			ImGui::NextColumn();
		};

		auto do_column_labels = [&](const auto& with_label, const auto& col) {
			do_column("Ping");
			do_column(with_label, col);
			do_column("Game mode");
			do_column("Players");
			do_column("Arena");
			do_column("First appeared");

			ImGui::Separator();
		};

		const auto num_locals = local_server_list.size();
		const auto num_officials = official_server_list.size();
		const auto num_communities = community_server_list.size();

		const auto local_servers_label = typesafe_sprintf("Local servers (%x)", num_locals);
		const auto official_servers_label = typesafe_sprintf("Official servers (%x)", num_officials);
		const auto community_servers_label = typesafe_sprintf("Community servers (%x)", num_communities);

		auto separate_with_label_only = [&](const auto& label, const auto& color) {
			text_disabled("\n\n");

			ImGui::Separator();
			ImGui::NextColumn();
			text_color(label, color);
			next_columns(num_columns - 1);
			ImGui::Separator();
		};

		if (has_local_servers) {
			do_column_labels(local_servers_label, green);
			show_server_list("local", local_server_list, in.faction_view, in.streamer_mode);
		}

		if (!has_local_servers) {
			do_column_labels(official_servers_label, yellow);
		}
		else {
			separate_with_label_only(official_servers_label, yellow);
		}

		if (has_official_servers) {
			show_server_list("official", official_server_list, in.faction_view, in.streamer_mode);
		}
		else {
			ImGui::NextColumn();
			text_disabled("No official servers are online at the time.");
			next_columns(num_columns - 1);
		}

		separate_with_label_only(community_servers_label, orange);

		if (has_community_servers) {
			show_server_list("community", community_server_list, in.faction_view, in.streamer_mode);
		}
		else {
			ImGui::NextColumn();
			text_disabled("No community servers are online at the time.");
			next_columns(num_columns - 1);
		}
	};

	{
		bool disable_content_view = false;

		if (error_message.size() > 0) {
			text_color(error_message, red);
			disable_content_view = true;
		}

		if (data->future_response.valid()) {
			text_disabled("Downloading the server list...");
			disable_content_view = true;
		}

		do_list_view(disable_content_view);
	}

	{
		auto child = scoped_child("buttons view");

		ImGui::Separator();

		checkbox("Only responding", only_responding);
		ImGui::SameLine();

		checkbox("At least", at_least_players.is_enabled);

		{
			auto& val = at_least_players.value;
			auto scoped = maybe_disabled_cols({}, !at_least_players.is_enabled);
			auto width = scoped_item_width(ImGui::CalcTextSize("9").x * 4);

			ImGui::SameLine();
			auto input = std::to_string(val);
			input_text(val == 1 ? "player###numbox" : "players###numbox", input);
			val = std::clamp(std::atoi(input.c_str()), 1, int(max_incoming_connections_v));
		}


		ImGui::SameLine();

		checkbox("At most", at_most_ping.is_enabled);
		
		{
			auto& val = at_most_ping.value;
			auto scoped = maybe_disabled_cols({}, !at_most_ping.is_enabled);
			auto width = scoped_item_width(ImGui::CalcTextSize("9").x * 5);

			ImGui::SameLine();
			auto input = std::to_string(val);
			input_text(val == 1 ? "ping###numbox" : "ping###numbox", input);
			val = std::clamp(std::atoi(input.c_str()), 0, int(999));
		}

		{
			auto scope = maybe_disabled_cols({}, !selected_server.is_set());

			if (ImGui::Button("Connect")) {
				auto& s = selected_server;

				LOG("Chosen server list entry: %x (%x). Connecting.", ToString(s.address), s.heartbeat.server_name);

				displayed_connecting_server_name = s.heartbeat.server_name;

				requested_connection = s.get_connect_string();
			}

			ImGui::SameLine();

			if (ImGui::Button("Server details")) {
				server_details.open();
			}

			ImGui::SameLine();
		}

		ImGui::SameLine();

		{
			auto scope = maybe_disabled_cols({}, refresh_in_progress());

			if (ImGui::Button("Refresh ping")) {
				refresh_server_pings();
			}

			ImGui::SameLine();

			if (ImGui::Button("Refresh list")) {
				refresh_server_list(in);
			}
		}
	}
	
	if (server_details.show) {
		if (!selected_server.is_set()) {
			for (auto& s : server_list) {
				if (s.address == selected_server.address) {
					selected_server = s;
				}
			}
		}
	}

	if (server_details.perform(selected_server, in.faction_view, in.streamer_mode)) {
		refresh_server_list(in);
	}

	if (requested_connection.has_value()) {
		in.client_connect = *requested_connection;
		in.displayed_connecting_server_name = displayed_connecting_server_name;

		return true;
	}

	return false;
}

const server_list_entry* browse_servers_gui_state::find_best_server() const {
	if (server_list.empty()) {
		return nullptr;
	}

	return &minimum_of(server_list, compare_servers);
}

const server_list_entry* browse_servers_gui_state::find_best_ranked() const {
	auto filtered = server_list;

	erase_if(
		filtered,
		[&](auto& f) {
			const bool good = f.is_official_server(); //&& f.heartbeat.is_ranked_server;

			return !good;
		}
	);

	if (filtered.empty()) {
		return nullptr;
	}

	return &minimum_of(filtered, compare_servers);
}

const server_list_entry* browse_servers_gui_state::find_entry(const client_connect_string& in) const {
	if (const auto connected_address = ::find_netcode_addr(in)) {
		LOG("Finding the server entry by: %x", in);
		LOG("Number of servers in the browser: %x", server_list.size());

		for (auto& s : server_list) {
			if (s.address == *connected_address) {
				return &s;
			}
		}
	}
	else {
		LOG("find_entry: Not an IP address: %x.", in);
	}

	return nullptr;
}

void server_details_gui_state::perform_online_players(
	const server_list_entry& entry,
	const faction_view_settings& faction_view,
	const bool streamer_mode
) {
	using namespace augs::imgui;

	auto heartbeat = entry.heartbeat;

	//input_text("Server version", heartbeat.server_version, ImGuiInputTextFlags_ReadOnly);
	if (heartbeat.num_online == 0) {
		text_disabled("No players online.");
		return;
	}

#if 0
	ImGui::Separator();
	text("Faction scores");
	ImGui::Separator();

	auto do_faction_score = [&](const auto faction, const auto score) {
		const auto labcol = faction_view.colors[faction].current_player_text;

		text_color(typesafe_sprintf("%x: %x", format_enum(faction), score), labcol);
	};

	do_faction_score(faction_type::RESISTANCE, heartbeat.score_resistance);
	do_faction_score(faction_type::METROPOLIS, heartbeat.score_metropolis);
#endif

	ImGui::Separator();

	auto longest_nname = std::size_t(0);

	auto get_nickname = [&](const auto& e) -> std::string {
		if (streamer_mode) {
			return "Player";
		}

		return e.nickname;
	};

	for (auto& e : heartbeat.players_resistance) { 
		longest_nname = std::max(longest_nname, get_nickname(e).length());
	}
	for (auto& e : heartbeat.players_metropolis) { 
		longest_nname = std::max(longest_nname, get_nickname(e).length());
	}
	for (auto& e : heartbeat.players_spectating) { 
		longest_nname = std::max(longest_nname, get_nickname(e).length());
	}

	text_color(typesafe_sprintf("Players online: %x", heartbeat.num_online), green);

	ImGui::Columns(3);
	//ImGui::SetColumnWidth(0, ImGui::CalcTextSize("9999999").x);
	ImGui::SetColumnWidth(0, ImGui::CalcTextSize("9").x * (longest_nname + 5));
	ImGui::SetColumnWidth(1, ImGui::CalcTextSize("Score99").x);
	ImGui::SetColumnWidth(2, ImGui::CalcTextSize("Deaths90").x);

#if 0
	text_disabled("Nickname");
	ImGui::NextColumn();

	//text_disabled("Score");
	ImGui::NextColumn();

	//text_disabled("Deaths");
	ImGui::NextColumn();
#endif

	bool once = true;

	auto do_faction = [&](const auto faction, const auto& entries, const auto score) {
		(void)score;
		if (entries.empty()) {
			return;
		}

		const auto labcol = faction_view.colors[faction].current_player_text;
		const auto col = faction_view.colors[faction].current_player_text;
		(void)labcol;

		ImGui::Separator();

		if (faction == faction_type::SPECTATOR) {
			text_disabled("Spectators");

			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();

			ImGui::Separator();
		}
		else {
			text_disabled(typesafe_sprintf("%x", format_enum(faction)));

			ImGui::NextColumn();
			if (once) {
				text_disabled("Score");
			}
			//text_disabled(typesafe_sprintf("%x", score));

			ImGui::NextColumn();
			if (once) {
				text_disabled("Deaths");
			}
			ImGui::NextColumn();

			ImGui::Separator();

			once = false;
		}


		for (auto& e : entries) {
			text_color(get_nickname(e), col);
			ImGui::NextColumn();

			if (faction != faction_type::SPECTATOR) {
				text(std::to_string(e.score));
			}

			ImGui::NextColumn();

			if (faction != faction_type::SPECTATOR) {
				text(std::to_string(e.deaths));
			}

			ImGui::NextColumn();
		}
	};

	do_faction(faction_type::RESISTANCE, heartbeat.players_resistance, heartbeat.score_resistance);
	do_faction(faction_type::METROPOLIS, heartbeat.players_metropolis, heartbeat.score_metropolis);
	do_faction(faction_type::SPECTATOR, heartbeat.players_spectating, 0);
}

bool server_details_gui_state::perform(
	const server_list_entry& entry,
	const faction_view_settings& faction_view,
	const bool streamer_mode
) {
	using namespace augs::imgui;
	//ImGui::SetNextWindowSize(ImVec2(350,560), ImGuiCond_FirstUseEver);
	auto window = make_scoped_window(ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

	if (!window) {
		return false;
	}

	if (!entry.is_set()) {
		text_disabled("No server selected.");
		return false;
	}

	if (ImGui::Button("Refresh")) {
		return true;
	}

	ImGui::Separator();

	auto heartbeat = entry.heartbeat;
	const auto& internal_addr = heartbeat.internal_network_address;

	auto external_address = ::ToString(entry.address);
	auto internal_address = internal_addr ? ::ToString(*internal_addr) : std::string("Unknown yet");

	acquire_keyboard_once();

#if IS_PRODUCTION_BUILD
	const bool censor_ips = true;
#else
	const bool censor_ips = false;
#endif

	checkbox("Show IPs", show_ips);

	if (censor_ips && !show_ips) {
		if (ImGui::Button("Copy External IP address to clipboard")) {
			ImGui::SetClipboardText(external_address.c_str());
		}

		if (ImGui::Button("Copy Internal IP address to clipboard")) {
			ImGui::SetClipboardText(internal_address.c_str());
		}
	}
	else {
		input_text("External IP address", external_address, ImGuiInputTextFlags_ReadOnly);
		input_text("Internal IP address", internal_address, ImGuiInputTextFlags_ReadOnly);
	}

	auto name = heartbeat.server_name;
	auto arena = heartbeat.current_arena;
	auto version = heartbeat.server_version;

	if (streamer_mode && entry.is_community_server) {
		name = "Community server";
		arena = "Community arena";
	}

	input_text("Server name", name, ImGuiInputTextFlags_ReadOnly);

	input_text("Arena", arena, ImGuiInputTextFlags_ReadOnly);

	auto nat = nat_type_to_string(heartbeat.nat.type); 

	input_text("NAT type", nat, ImGuiInputTextFlags_ReadOnly);
	auto delta = std::to_string(heartbeat.nat.port_delta);

	if (heartbeat.nat.type != nat_type::PUBLIC_INTERNET) {
		input_text("Port delta", delta, ImGuiInputTextFlags_ReadOnly);
	}

	input_text("Server version", version, ImGuiInputTextFlags_ReadOnly);

	perform_online_players(entry, faction_view, streamer_mode);
	return false;
}

void browse_servers_gui_state::reping_all_servers() {
	for (auto& s : server_list) {
		s.progress = {};
	}
}
