#include "augs/misc/key_and_mouse_intent.h"
#include "augs/misc/input_context.h"

bool key_and_mouse_intent::is_set() const {
	return intent != intent_type::COUNT;
}

bool key_and_mouse_intent::uses_mouse_motion() const {
	return
		intent == intent_type::MOVE_CROSSHAIR
		|| intent == intent_type::CROSSHAIR_PRIMARY_ACTION
		|| intent == intent_type::CROSSHAIR_SECONDARY_ACTION
	;
}

bool key_and_mouse_intent::operator==(const key_and_mouse_intent& b) const {
	return std::make_tuple(intent, mouse_rel, is_pressed) == std::make_tuple(b.intent, b.mouse_rel, b.is_pressed);
}

bool key_and_mouse_intent::operator!=(const key_and_mouse_intent& b) const {
	return !operator==(b);
}