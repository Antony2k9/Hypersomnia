#pragma once
#include <string>
#include "augs/network/port_type.h"
#include "application/gui/connect_address_type.h"
#include "augs/filesystem/path_declaration.h"
#include "application/network/address_and_port.h"

struct client_start_input {
	// GEN INTROSPECTOR struct client_start_input
	port_type default_port = 8412;
	connect_address_type chosen_address_type = connect_address_type::OFFICIAL;

	std::string custom_address = "127.0.0.1";
	std::string preferred_official_address = "";

	augs::path_type replay_demo;

	std::string displayed_connecting_server_name;
	// END GEN INTROSPECTOR

	address_and_port get_address_and_port() const;
	void set_custom(const std::string& target);
};
