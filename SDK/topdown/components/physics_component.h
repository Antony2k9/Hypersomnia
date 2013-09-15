#pragma once
#include <Box2D\Box2D.h>

#include "entity_system/component.h"
#define METERS_TO_PIXELS 50.0
#define PIXELS_TO_METERS 1.0/METERS_TO_PIXELS
#define METERS_TO_PIXELSf 50.f
#define PIXELS_TO_METERSf 1.0f/METERS_TO_PIXELSf

namespace components {
	struct physics : public augmentations::entity_system::component {
		b2Body* body;
		physics(b2Body* body = nullptr) : body(body) {}
	};
}
