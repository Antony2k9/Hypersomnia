#pragma once
#include "test_scenes/test_id_to_pool_id.h"

enum class test_scene_particle_effect_id {
	// GEN INTROSPECTOR enum class test_scene_particle_effect_id
	WANDERING_SMOKE,
	ENGINE_PARTICLES,
	MUZZLE_SMOKE,
	EXHAUSTED_SMOKE,
	FOOTSTEP_SMOKE,
	CAST_CHARGING,
	HEALTH_DAMAGE_SPARKLES,
	CAST_SPARKLES,
	EXPLODING_RING_SMOKE,
	EXPLODING_RING_SPARKLES,
	PIXEL_MUZZLE_LEAVE_EXPLOSION,
	PISTOL_MUZZLE_LEAVE_EXPLOSION,
	FIRE_MUZZLE_LEAVE_EXPLOSION,
	ELECTRIC_PROJECTILE_DESTRUCTION,
	STEEL_PROJECTILE_DESTRUCTION,
	PISTOL_PROJECTILE_DESTRUCTION,
	ELECTRIC_PROJECTILE_TRACE,
	SHELL_FIRE,
	ROUND_ROTATING_BLOOD_STREAM,
	THUNDER_REMNANTS,
	SKULL_ROCKET_TRACE,
	SKULL_ROCKET_DESTRUCTION,
	SKULL_ROCKET_MUZZLE_LEAVE_EXPLOSION,
	PICKUP_SPARKLES,
	STEAM_BURST,
	HASTE_FOOTSTEP,
	GLASS_DAMAGE,
	METAL_DAMAGE,
	WOOD_DAMAGE,
	ELECTRIC_RICOCHET,
	STEEL_RICOCHET,
	AQUARIUM_BUBBLES,
	FLOWER_BUBBLES,
	FISH_BUBBLE,
	STANDARD_LEARNT_SPELL,
	DETACHED_HEAD_FIRE,
	STANDARD_KNIFE_IMPACT,
	STANDARD_KNIFE_CLASH,
	STANDARD_KNIFE_ATTACK,

	FURY_THROWER_TRACE,
	FURY_THROWER_ATTACK,

	POSEIDON_THROWER_TRACE,
	POSEIDON_THROWER_ATTACK,

	STANDARD_KNIFE_PRIMARY_SMOKE,
	STANDARD_KNIFE_SECONDARY_SMOKE,

	COUNT
	// END GEN INTROSPECTOR
};


inline auto to_particle_effect_id(const test_scene_particle_effect_id id) {
	return to_pool_id<assets::particle_effect_id>(id);
}
