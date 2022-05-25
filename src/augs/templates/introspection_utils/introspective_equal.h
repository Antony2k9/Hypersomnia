#pragma once
#include "augs/templates/introspect.h"
#include "augs/templates/traits/is_optional.h"
#include "augs/templates/traits/is_comparable.h"
#include "augs/templates/traits/is_tuple.h"
#include "augs/templates/algorithm_templates.h"
#include "augs/templates/traits/is_pair.h"

namespace augs {
	template <class A, class B>
	bool introspective_equal(const A& a, const B& b);

	template <class A, class B>
	bool equal_or_introspective_equal(
		const A& a,
		const B& b
	) {
		if constexpr(is_optional_v<A>) {
			static_assert(is_optional_v<B>);

			if (a.has_value() != b.has_value()) {
				return false;
			}
			else if (a && b) {
				return equal_or_introspective_equal(*a, *b);
			}
			else {
				ensure(!a && !b);
				return true;
			}
		}
		else if constexpr(is_tuple_v<A> || is_pair_v<A>) {
			return introspective_equal(a, b);
		}
		else if constexpr(has_begin_and_end_v<A> && can_access_size_v<A>) {
			return ranges_equal(a, b, [](const auto& aa, const auto& bb) { return equal_or_introspective_equal(aa, bb); });
		}
		else if constexpr(is_comparable_v<A, B>) {
			return a == b;
		}
		else {
			return introspective_equal(a, b);
		}
	}

	template <class A, class B>
	bool introspective_equal(const A& a, const B& b) {
		static_assert(has_introspect_v<A> && has_introspect_v<B>, "Comparison requested on type(s) without introspectors!");

		bool are_equal = true;

		introspect(
			[&are_equal](auto, const auto& aa, const auto& bb) {
				are_equal = are_equal && equal_or_introspective_equal(aa, bb);
			}, a, b
		);

		return are_equal;
	}
}
