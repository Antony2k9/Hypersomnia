#pragma once
#include <variant>
#include "application/setups/main_menu_setup.h"
#include "application/setups/test_scene_setup.h"
#if BUILD_DEBUGGER_SETUP
#include "application/setups/debugger/debugger_setup.h"
#endif
#include "application/setups/client/client_setup.h"
#include "application/setups/server/server_setup.h"
#include "application/setups/editor/editor_setup.h"
#include "application/setups/project_selector/project_selector_setup.h"

using setup_variant = std::variant<
	test_scene_setup,
#if BUILD_DEBUGGER_SETUP
	debugger_setup,
#endif
	editor_setup,
	project_selector_setup
#if BUILD_NETWORKING
	, client_setup
	, server_setup
#endif
>;
