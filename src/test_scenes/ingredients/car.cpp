#include "test_scenes/ingredients/ingredients.h"
#include "augs/drawing/polygon.h"
#include "game/cosmos/cosmos.h"
#include "game/stateless_systems/particles_existence_system.h"
#include "game/stateless_systems/sound_existence_system.h"

#include "game/components/crosshair_component.h"
#include "game/components/sprite_component.h"
#include "game/components/movement_component.h"
#include "game/components/rigid_body_component.h"
#include "game/components/car_component.h"
#include "game/components/rigid_body_component.h"
#include "game/components/fixtures_component.h"
#include "game/detail/view_input/particle_effect_input.h"
#include "game/detail/view_input/sound_effect_input.h"
#include "game/components/polygon_component.h"
#include "game/components/shape_polygon_component.h"
#include "game/cosmos/cosmos.h"

#include "game/assets/all_logical_assets.h"

#include "game/enums/filters.h"

namespace test_flavours {
	void populate_car_flavours(const populate_flavours_input in) {
		(void)in;
#if TODO_CARS
#endif
	}
}

#if TODO_CARS
namespace prefabs {
	entity_handle create_car(const logic_step step, const transformr& spawn_transform) {
		auto& world = step.get_cosmos();
		const auto& metas = step.get_logical_assets();

		auto front_id = create_test_scene_entity(world, test_scene_flavour::TRUCK_FRONT).get_id();
		auto interior_id = create_test_scene_entity(world, test_scene_flavour::TRUCK_INTERIOR).get_id();
		auto left_wheel_id = create_test_scene_entity(world, test_scene_flavour::TRUCK_LEFT_WHEEL).get_id();

		const auto si = world.get_si();

		const vec2 front_size = metas.at(to_image_id(test_scene_image_id::TRUCK_FRONT)).get_size();
		const vec2 interior_size = metas.at(to_image_id(test_scene_image_id::TRUCK_INSIDE)).get_size();

		{
			auto& car = front += components::car();

			components::rigid_body physics_invariant(si, spawn_transform);
			car.interior = interior;
			car.left_wheel_trigger = left_wheel;
			car.input_acceleration.set(2500, 4500) /= 3;
			//car.acceleration_length = 4500 / 5.0;
			car.acceleration_length = 4500 / 6.2f;
			car.speed_for_pitch_unit = 2000.f;


			physics_invariant.damping.linear = 0.4f;
			physics_invariant.damping.angular = 2.f;

			components::fixtures fixtures_invariant;

			fixtures_invariant.filter = filters[predefined_filter_type::WALL];
			fixtures_invariant.density = 0.6f;
			fixtures_invariant.material = to_physical_material_id(test_scene_physical_material_id::METAL);

			front += fixtures_invariant;
			front += physics_invariant;
			front.get<components::fixtures>().set_owner_body(front);
			//rigid_body.air_resistance = 0.2f;
		}
		
		{
			auto sprite = interior.get<invariants::sprite>();
			
			vec2 offset((front_size.x / 2 + sprite.get_size(/*metas*/).x / 2) * -1, 0);

			components::fixtures fixtures_invariant;

			fixtures_invariant.filter = filters[predefined_filter_type::GROUND];
			fixtures_invariant.density = 0.6f;
			fixtures_invariant.offsets_for_created_shapes[colliders_offset_type::SHAPE_OFFSET].pos = offset;
			fixtures_invariant.friction_ground = true;
			fixtures_invariant.material = to_physical_material_id(test_scene_physical_material_id::METAL);

			interior  += fixtures_invariant;

			interior.get<components::fixtures>().set_owner_body(front);
		}

		{
			auto sprite = left_wheel.get<invariants::sprite>();

			vec2 offset((front_size.x / 2 + sprite.get_size(/*metas*/).x / 2 + 20) * -1, 0);

			components::fixtures fixtures_invariant;

			fixtures_invariant.filter = filters[predefined_filter_type::TRIGGER];
			fixtures_invariant.density = 0.6f;
			fixtures_invariant.disable_standard_collision_resolution = true;
			fixtures_invariant.offsets_for_created_shapes[colliders_offset_type::SHAPE_OFFSET].pos = offset;
			fixtures_invariant.material = to_physical_material_id(test_scene_physical_material_id::METAL);

			left_wheel  += fixtures_invariant;
			left_wheel.get<components::fixtures>().set_owner_body(front);
		}

		{
			for (int i = 0; i < 4; ++i) {
				transformr this_engine_transform;
				const auto engine_physical = create_test_scene_entity(world, test_scene_flavour::TRUCK_ENGINE_BODY);

				{

					auto sprite = engine_physical.get<invariants::sprite>();

					transformr offset;

					if (i == 0) {
						offset.pos.set((front_size.x / 2 + interior_size.x + sprite.get_size(/*metas*/).x / 2 - 5.f) * -1, (-interior_size.y / 2 + sprite.get_size(/*metas*/).y / 2));
					}
					if (i == 1) {
						offset.pos.set((front_size.x / 2 + interior_size.x + sprite.get_size(/*metas*/).x / 2 - 5.f) * -1, -(-interior_size.y / 2 + sprite.get_size(/*metas*/).y / 2));
					}
					if (i == 2) {
						offset.pos.set(-100, (interior_size.y / 2 + sprite.get_size(/*metas*/).x / 2) * -1);
						offset.rotation = -90;
					}
					if (i == 3) {
						offset.pos.set(-100, (interior_size.y / 2 + sprite.get_size(/*metas*/).x / 2) *  1);
						offset.rotation = 90;
					}
					
					components::fixtures fixtures_invariant;

					fixtures_invariant.filter = filters[predefined_filter_type::LYING_ITEM];
					fixtures_invariant.density = 1.0f;
					fixtures_invariant.sensor = true;
					fixtures_invariant.offsets_for_created_shapes[colliders_offset_type::SHAPE_OFFSET] = offset;
					fixtures_invariant.material = to_physical_material_id(test_scene_physical_material_id::METAL);

					engine_physical  += fixtures_invariant;

					engine_physical.get<components::fixtures>().set_owner_body(front);
					engine_physical.construct_entity(step);

					this_engine_transform = engine_physical.get_logic_transform();
				}

				const vec2 engine_size = metas.at(to_image_id(test_scene_image_id::TRUCK_ENGINE)).get_size();

				{
					particles_existence_input input;
					
					input.effect.id = to_particle_effect_id(test_scene_particle_effect_id::ENGINE_PARTICLES);
					input.effect.modifier.colorize = cyan;
					input.effect.modifier.scale_amounts = 6.7f;
					input.effect.modifier.scale_lifetimes = 0.45f;
					input.delete_entity_after_effect_lifetime = false;

					auto place_of_birth = this_engine_transform;

					if (i == 0 || i == 1) {
						place_of_birth.rotation += 180;
					}

					const auto engine_particles = input.start_particle_effect_entity(
						step,
						place_of_birth,
						front
					);

					auto& existence = engine_particles.get<components::particles_existence>();
					existence.distribute_within_segment_of_length = engine_size.y;

					engine_particles.construct_entity(step);

					if (i == 0) {
						front.get<components::car>().acceleration_engine[i].physical = engine_physical;
						front.get<components::car>().acceleration_engine[i].particles = engine_particles;
					}
					if (i == 1) {
						front.get<components::car>().acceleration_engine[i].physical = engine_physical;
						front.get<components::car>().acceleration_engine[i].particles = engine_particles;
					}
					if (i == 2) {
						front.get<components::car>().left_engine.physical = engine_physical;
						front.get<components::car>().left_engine.particles = engine_particles;
					}
					if (i == 3) {
						front.get<components::car>().right_engine.physical = engine_physical;
						front.get<components::car>().right_engine.particles = engine_particles;
					}

					components::particles_existence::deactivate(engine_particles);
				}
			}

			{
				sound_existence_input in;
				in.effect.id = to_sound_id(test_scene_sound_id::ENGINE);
				in.delete_entity_after_effect_lifetime = false;

				const auto engine_sound = in.create_sound_effect_entity(step, spawn_transform, front);
				engine_sound.construct_entity(step);

				front.get<components::car>().engine_sound = engine_sound;
				components::sound_existence::deactivate(engine_sound);
			}
		}

		front.construct_entity(step);
		left_wheel.construct_entity(step);
		interior.construct_entity(step);

		return front;
	}
}
#endif