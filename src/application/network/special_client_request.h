#pragma once

enum class special_client_request {
	RESYNC_ARENA,
	RESET_AFK_TIMER,

	WAIT_IM_DOWNLOADING_EXTERNALLY,
	RESYNC_ARENA_AFTER_FILES_DOWNLOADED,

	COUNT
};

