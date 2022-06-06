#pragma once
#include "augs/math/vec2.h"
#include "augs/math/snapping_grid.h"
#include "augs/templates/identity_templates.h"

template <class C, class F, class M, class... K>
void on_each_independent_transform(
	C& cosm,
	const M& subjects,
	F&& callback,
	K... keys	
) {
	subjects.for_each(
		[&](const auto& i) {
			const auto typed_handle = cosm[i];

			typed_handle.access_independent_transform(
				[&](auto& tr) { callback(tr, typed_handle); },
				keys...
			);
		}
	);
}

template <class T>
void fix_pixel_imperfections(T& in) {
	if constexpr(std::is_same_v<T, transformr>) {
		in.rotation = augs::normalize_degrees(in.rotation);

		if (augs::to_near_90_multiple(in.rotation)) {
			in.pos.round_fract();
		}
	}
	else {
		static_assert(always_false_v<T>, "Unknown transform type");
	}
}

inline void fix_pixel_imperfections(transformr& in, const si_scaling si) {
	auto degrees = augs::normalize_degrees(RAD_TO_DEG<real32> * in.rotation);

	if (augs::to_near_90_multiple(degrees)) {
		in.pos = si.get_meters(si.get_pixels(in.pos).round_fract());
		in.rotation = degrees * DEG_TO_RAD<real32>;
	}
}

