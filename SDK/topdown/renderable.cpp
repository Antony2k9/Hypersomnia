#include <algorithm>
#include "entity_system/entity.h"

#include "../topdown/components/physics_component.h"
#include "renderable.h"

sprite::sprite(texture_baker::texture* tex, graphics::pixel_32 color) : tex(tex), color(color) {
	size = tex->get_size();
}

void renderable::make_rect(vec2<double> pos, vec2<float> size, float angle, vec2<float> v[4]) {
	vec2<float> origin = pos + size / 2.0;
	v[0] = pos;
	v[1] = pos + vec2<float>(size.x, 0.f);
	v[2] = pos + size;
	v[3] = pos + vec2<float>(0.f, size.y);

	v[0].rotate(angle, origin);
	v[1].rotate(angle, origin);
	v[2].rotate(angle, origin);
	v[3].rotate(angle, origin);

	v[0] -= size / 2.0;
	v[1] -= size / 2.0;
	v[2] -= size / 2.0;
	v[3] -= size / 2.0;
}

void sprite::draw(buffer& triangles, const components::transform& transform) {
	vec2<float> v[4];
	make_rect(transform.current.pos, size, transform.current.rotation, v);

	triangle t1, t2;
	t1.vertices[0].color = t2.vertices[0].color = color;
	t1.vertices[1].color = t2.vertices[1].color = color;
	t1.vertices[2].color = t2.vertices[2].color = color;

	t1.vertices[0].texcoord = t2.vertices[0].texcoord = vec2<float>(0.f, 0.f);
	t2.vertices[1].texcoord = vec2<float>(1.f, 0.f);
	t1.vertices[1].texcoord = t2.vertices[2].texcoord = vec2<float>(1.f, 1.f);
	t1.vertices[2].texcoord = vec2<float>(0.f, 1.f);

	for (int i = 0; i < 3; ++i) {
		tex->get_uv(t1.vertices[i].texcoord);
		tex->get_uv(t2.vertices[i].texcoord);
	}

	t1.vertices[0].position = t2.vertices[0].position = v[0];
	t2.vertices[1].position = v[1];
	t1.vertices[1].position = t2.vertices[2].position = v[2];
	t1.vertices[2].position = v[3];

	triangles.emplace_back(t1);
	triangles.emplace_back(t2);
}

b2Body* sprite::create_body(entity_system::entity& subject, b2World& b2world, b2BodyType type) {
	b2BodyDef def;
	def.type = type;
	def.angle = 0;
	def.userData = (void*) &subject;

	b2PolygonShape shape;
	shape.SetAsBox(size.w / 2.0 * PIXELS_TO_METERS, size.h / 2.0 * PIXELS_TO_METERS);
	
	//b2Vec2 v[4] = {
	//	vec2<float>(0.f, 0.f),
	//	vec2<float>(size.w*PIXELS_TO_METERS, 0.f),
	//	vec2<float>(size.w*PIXELS_TO_METERS, size.h*PIXELS_TO_METERS),
	//	vec2<float>(0.f, size.h*PIXELS_TO_METERS)
	//};

	//shape.Set(v, 4);
	
	b2FixtureDef fixdef;
	fixdef.shape = &shape;
	fixdef.density = 1.0;
	fixdef.friction = 1.0;

	b2Body* body = b2world.CreateBody(&def);
	body->CreateFixture(&fixdef);
	auto& transform = subject.get<components::transform>();

	body->SetTransform(transform.current.pos*PIXELS_TO_METERS, transform.current.rotation);

	return body;
}

rects::xywh sprite::get_aabb(const components::transform& transform) {
	vec2<float> v[4];
	make_rect(transform.current.pos, size, transform.current.rotation, v);

	typedef const vec2<float>& vc;
	
	rects::xywh out;
	auto x_pred = [](vc a, vc b){ return a.x < b.x; };
	auto y_pred = [](vc a, vc b){ return a.y < b.y; };

	vec2<float> lower = vec2<float>(
		std::min_element(v, v + 4, x_pred)->x,
		std::min_element(v, v + 4, y_pred)->y
		);

	vec2<float> upper = vec2<float>(
		std::max_element(v, v + 4, x_pred)->x,
		std::max_element(v, v + 4, y_pred)->y
		);

	return rects::ltrb(lower.x, lower.y, upper.x, upper.y);
}

void polygon::draw(buffer& triangles, const components::transform& transform) {
	/* perform triangulation */
}