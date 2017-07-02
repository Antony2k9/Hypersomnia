#pragma once

enum class menu_button_type {
	CONNECT_TO_OFFICIAL_UNIVERSE,
	BROWSE_UNOFFICIAL_UNIVERSES,
	HOST_UNIVERSE,
	CONNECT_TO_UNIVERSE,
	LOCAL_UNIVERSE,
	EDITOR,
	SETTINGS,
	CREATORS,
	QUIT,

	COUNT
};

enum class local_submenu_button_type {
	SINGLEPLAYER,
	SCENE_DIRECTOR,
	DETERMINISM_TEST,
	BACK,

	COUNT
};
