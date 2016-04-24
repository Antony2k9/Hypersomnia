#pragma once

//#include "asset.h"
//
namespace augs {
	class texture;
}

#include "math/vec2.h"

namespace assets {
	enum texture_id {
		INVALID_TEXTURE,
		BLANK,

		CRATE,
		CAR_INSIDE,
		CAR_FRONT,

		TRUCK_INSIDE,
		TRUCK_FRONT,

		MOTORCYCLE_FRONT,
		MOTORCYCLE_INSIDE,

		TEST_CROSSHAIR,
		TEST_PLAYER,

		TORSO_MOVING_FIRST,
		TORSO_MOVING_LAST = TORSO_MOVING_FIRST + 5,

		SMOKE_PARTICLE_FIRST,
		SMOKE_PARTICLE_LAST = SMOKE_PARTICLE_FIRST + 6,

		PIXEL_THUNDER_FIRST,
		PIXEL_THUNDER_LAST = PIXEL_THUNDER_FIRST + 5,

		ASSAULT_RIFLE,
		SUBMACHINE,
		PISTOL,
		SHOTGUN,
		URBAN_CYAN_MACHETE,

		TEST_BACKGROUND,
		TEST_BACKGROUND2,
		TEST_SPRITE,

		SAMPLE_MAGAZINE,
		SAMPLE_SUPPRESSOR,
		ROUND_TRACE,
		PINK_CHARGE,
		PINK_SHELL,
		CYAN_CHARGE,
		CYAN_SHELL,
		GREEN_CHARGE,
		GREEN_SHELL,
		BACKPACK,
		RIGHT_HAND_ICON,
		LEFT_HAND_ICON,
		DROP_HAND_ICON,

		GUI_CURSOR,
		GUI_CURSOR_ADD,
		GUI_CURSOR_MINUS,
		GUI_CURSOR_ERROR,
		
		HUD_CIRCULAR_BAR_MEDIUM,

		CONTAINER_OPEN_ICON,
		CONTAINER_CLOSED_ICON,

		ATTACHMENT_CIRCLE_FILLED,
		ATTACHMENT_CIRCLE_BORDER,
		PRIMARY_HAND_ICON,
		SECONDARY_HAND_ICON,
		SHOULDER_SLOT_ICON,
		ARMOR_SLOT_ICON,
		CHAMBER_SLOT_ICON,
		DETACHABLE_MAGAZINE_ICON,
		GUN_BARREL_SLOT_ICON,

		CUSTOM = 10000
	};
	
	vec2i get_size(texture_id);
}

augs::texture& operator*(const assets::texture_id& id);
bool operator!(const assets::texture_id& id);