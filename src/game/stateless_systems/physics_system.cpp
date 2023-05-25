#include "3rdparty/Box2D/Box2D.h"

#include "game/cosmos/cosmos.h"
#include "game/cosmos/logic_step.h"
#include "game/cosmos/entity_handle.h"
#include "game/cosmos/data_living_one_step.h"
#include "game/cosmos/for_each_entity.h"

#include "game/stateless_systems/physics_system.h"
#include "game/stateless_systems/portal_system.h"

#define OVER_BODIES 0

void physics_system::post_and_clear_accumulated_collision_messages(const logic_step step) {
	auto& cosm = step.get_cosmos();
	auto& physics = cosm.get_solvable_inferred({}).physics;
	
	step.post_messages(physics.accumulated_messages);
	physics.accumulated_messages.clear();
}

void physics_system::step_and_set_new_transforms(const logic_step step) {
	auto& cosm = step.get_cosmos();
	auto& physics = cosm.get_solvable_inferred({}).physics;

	auto& performance = cosm.profiler;

	{
		auto scope = measure_scope(performance.physics_step);

		const auto delta = step.get_delta();

		const int32 velocityIterations = 8;
		const int32 positionIterations = 3;

		physics.b2world->Step(
			static_cast<float32>(delta.in_seconds()),
			velocityIterations,
			positionIterations
		);

		post_and_clear_accumulated_collision_messages(step);
	}

	auto scope = measure_scope(performance.physics_readback);

#if OVER_BODIES
	for (b2Body* b = physics.b2world->GetBodyList(); b != nullptr; b = b->GetNext()) {
		if (b->GetType() == b2_staticBody) continue;
		entity_handle entity = cosm[b->GetUserData()];

		physics.recurential_friction_handler(step, b, b->m_ownerFrictionGround);
		entity.get<components::rigid_body>().update_after_step(*b);	
	}
#else
	cosm.for_each_having<components::rigid_body>(
		[&](const auto& handle){
			const auto rigid_body = handle.template get<components::rigid_body>();

			auto& body = *rigid_body.find_cache()->body.get();
			rigid_body.update_after_step(body);

			physics.recurential_friction_handler(step, &body, body.m_ownerFrictionGround);

			special_physics& special = rigid_body.get_special();
			special.teleport_progress -= special.teleport_progress_falloff_speed;

			if (special.inside_portal.is_set()) {
				if (special.teleport_progress >= 1.0f) {
					portal_system().finalize_portal_exit(
						step,
						handle,
						true
					);
				}
			}
		}
	);
#endif
}

