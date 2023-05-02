#pragma once
#include <sol/sol.hpp>

#include "augs/ensure.h"
#include "augs/log.h"
#include "augs/pad_bytes.h"

#include "augs/string/get_type_name.h"
#include "augs/templates/traits/is_variant.h"
#include "augs/templates/traits/is_monostate.h"
#include "augs/templates/traits/is_optional.h"
#include "augs/templates/traits/is_maybe.h"
#include "augs/templates/traits/is_pair.h"
#include "augs/misc/is_constant_size_string.h"
#include "augs/readwrite/lua_readwrite_errors.h"
#include "augs/templates/container_templates.h"
#include "augs/templates/introspect.h"
#include "augs/templates/for_each_type.h"
#include "augs/templates/identity_templates.h"

#include "augs/string/string_to_enum.h"

#include "augs/readwrite/custom_lua_representations.h"
#include "augs/misc/lua/lua_utils.h"

#include "augs/readwrite/lua_readwrite_declaration.h"
#include "augs/readwrite/lua_readwrite_overload_traits.h"
#include "augs/readwrite/lua_traits.h"
#include "augs/misc/enum/enum_boolset.h"
#include "augs/misc/enum/is_enum_boolset.h"

namespace augs {
	template <class... T>
	inline const char* get_variant_type_label(T&&...) {
		return "kind";
	}

	template <class... T>
	inline const char* get_variant_content_label(T&&...) {
		return "content";
	}

	template <class T>
	void general_from_lua_value(const sol::object& object, T& into) {
		if constexpr(std::is_pointer_v<T>) {
			into = nullptr;
		}
		else if constexpr(has_custom_to_lua_value_v<T>) {
			from_lua_value(object, into);
		}
		else if constexpr(is_constant_size_string_v<T>) {
			if (object.is<std::string>()) {
				into = object.as<std::string>();
			}
		}
		else if constexpr(std::is_same_v<T, std::string> || std::is_arithmetic_v<T>) {
			if (object.is<T>()) {
				into = object.as<T>();
			}
		}
		else if constexpr(std::is_enum_v<T>) {
			if constexpr(has_enum_to_string_v<T>) {
				if (object.is<std::string>()) {
					const auto stringized_enum = object.as<std::string>();

					if (
						const auto* enumized = mapped_or_nullptr(
							get_string_to_enum_map<T>(), 
							stringized_enum
						)
					) {
						into = *enumized;
					}
					else {
						LOG(
							"Failed to read \"%x\" into %x enum. Check if such option exists, or if spelling is correct.",
							stringized_enum,
							get_type_name<T>()
						);
					}
				}
			}
			else {
				if (object.is<int>()) {
					into = static_cast<T>(object.as<int>());
				}
			}
		}
		else {
			static_assert(always_false_v<T>, "Non-exhaustive general_from_lua_value");
		}
	}

	template <class Serialized>
	void verify_read_lua() {
		static_assert(
			!is_optional_v<Serialized> && !is_maybe_v<Serialized>,
			"std::optional and maybe can only be serialized as a member object."
		);

		static_assert(!std::is_const_v<Serialized>, "Trying to read into a const object.");
	}

	template <class Archive, class Serialized>
	void read_lua(const Archive& input_object, Serialized& into) {
		verify_read_lua<Serialized>();

		if constexpr(has_lua_read_overload_v<Serialized>) {
			static_assert(has_lua_write_overload_v<Serialized>, "Has read_object_lua overload, but no write_object_lua overload.");
			
			sol::table input_table = input_object;
			read_object_lua(input_table, into);
		}
		else {
			read_lua_no_overload(input_object, into);
		}
	}

	template <class Archive, class Serialized>
	void read_lua_no_overload(const Archive& input_object, Serialized& into) {
		verify_read_lua<Serialized>();

		if constexpr(is_variant_v<Serialized>) {
			sol::table input_table = input_object;
			ensure(input_table.is<sol::table>());

			const auto variant_type_label = get_variant_type_label(into);
			std::string variant_type = input_table[variant_type_label];

			if (variant_type == "std_monostate") {
				return;
			}

			const auto variant_content_label = get_variant_content_label(into);
			sol::object variant_content = input_table[variant_content_label];

			for_each_type_in_list<Serialized>(
				[variant_content, variant_type, &into](const auto& resolved){
					if constexpr(!is_monostate_v<decltype(resolved)>) {
						const auto this_type_name = get_type_name_strip_namespace(resolved);

						if (this_type_name == variant_type) {
							using T = remove_cref<decltype(resolved)>;
							T object{};
							read_lua(variant_content, object);
							into.template emplace<T>(std::move(object));
						}
					}
				}
			);
		}
		else if constexpr(representable_as_lua_value_v<Serialized>) {
			general_from_lua_value(input_object, into);
		}
		else {
			sol::table input_table = input_object;
			ensure(input_table.is<sol::table>() && "This must be read from a table, not a value.");
			
			if constexpr(is_container_v<Serialized>) {
				using Container = Serialized;
			
				if constexpr(can_clear_v<Container>) {
					into.clear();
				}
				else {
					into = {};
				}

				/*
					If container is associative and the keys are representable as lua values,
					read container table as:
					
					{
						ab = "abc",
						cd = "cde"
					}

					otherwise, read container table as a sequence of key-value pairs (like vector):

					{
						{ "ab", "abc" },
						{ "cd", "cde" }
					}
				*/

				if constexpr(is_associative_v<Container> && key_representable_as_lua_value_v<Container>) {
					for (auto key_value_pair : input_table) {
						typename Container::key_type key{};
						typename Container::mapped_type mapped{};

						general_from_lua_value(key_value_pair.first, key);
						read_lua(key_value_pair.second, mapped);

						into.emplace(std::move(key), std::move(mapped));
					}
				}
				else if constexpr(is_enum_array_v<Container>) {
					augs::for_each_enum_except_bounds([&](typename Container::enum_type e) {
						auto maybe_entry = input_table[augs::enum_to_string(e)];

						if (maybe_entry.valid()) {
							read_lua(maybe_entry, into[e]);
						}
					});
				}
				else if constexpr(is_enum_boolset_v<Container>) {
					using E = typename Container::enum_type;

					into.reset();

					int counter = 1;

					while (true) {
						sol::object maybe_element = input_table[counter];

						if (maybe_element.valid()) {
							E e;
							general_from_lua_value<E>(maybe_element, e);
							into[e] = true;
						}
						else {
							break;
						}

						++counter;
					}
				}
				else if constexpr(is_std_array_v<Container>) {
					for (std::size_t i = 0; i < into.size(); ++i) {
						read_lua(input_table[static_cast<int>(i + 1)], into[i]);
					}
				}
				else {
					int counter = 1;

					const auto max_size = into.max_size();

					while (true) {
						if (into.size() >= max_size) {
							break;
						}

						sol::object maybe_element = input_table[counter];

						if (maybe_element.valid()) {
							if constexpr(is_associative_v<Container>) {
								typename Container::key_type key{};
								typename Container::mapped_type mapped{};

								ensure(maybe_element.is<sol::table>());
								
								sol::table key_value_table = maybe_element;

								ensure(key_value_table[1].valid());
								ensure(key_value_table[2].valid());

								read_lua(key_value_table[1], key);
								read_lua(key_value_table[2], mapped);

								into.emplace(std::move(key), std::move(mapped));
							}
							else {
								typename Container::value_type val;

								read_lua(input_table[counter], val);

								if constexpr(can_emplace_back_v<Container>) {
									into.emplace_back(std::move(val));
								}
								else {
									into.emplace(std::move(val));
								}
							}
						}
						else {
							break;
						}

						++counter;
					}
				}
			}
			else if constexpr(is_pair_v<Serialized>) {
				sol::object maybe_first = input_table[1];
				sol::object maybe_second = input_table[2];

				if (maybe_first.valid() && maybe_second.valid()) {
					read_lua(maybe_first, into.first);
					read_lua(maybe_second, into.second);
				}
			}
			else {
				introspect(
					[&input_table](const auto& label, auto& field) {
						using T = remove_cref<decltype(field)>;

						if constexpr(is_optional_v<T>) {
							sol::object maybe_field = input_table[label];
							
							if (maybe_field.valid()) {
								if (field == std::nullopt) {
									field.emplace();
								}

								read_lua(maybe_field, *field);
							}
						}
						else if constexpr(is_maybe_v<T>) {
							if constexpr(T::serialize_disabled) {
								if (sol::object enabled_field = input_table[std::string("enabled_") + label];
									enabled_field.valid()
								) {
									read_lua(enabled_field, field.value);
									field.is_enabled = true;
								}

								if (sol::object disabled_field = input_table[std::string("disabled_") + label];
									disabled_field.valid()
								) {
									read_lua(disabled_field, field.value);
									field.is_enabled = false;
								}
							}
							else {
								if (sol::object disabled_field = input_table[label];
									disabled_field.valid()
								) {
									read_lua(disabled_field, field.value);
									field.is_enabled = false;
								}
							}
						}
						else if constexpr(!is_padding_field_v<T>) {
							sol::object maybe_field = input_table[label];

							const bool field_specified = maybe_field.valid();
							
							if (field_specified) {
								read_lua(maybe_field, field);
							}
						}
					},
					into
				);
			}
		}
	}

	template <class T>
	decltype(auto) general_to_lua_value(const T& field) {
		if constexpr(std::is_pointer_v<T>) {
			return general_to_lua_value(*field);
		}
		else if constexpr(has_custom_to_lua_value_v<T>) {
			return to_lua_value(field);
		}
		else if constexpr(is_constant_size_string_v<T>) {
			return field.operator std::string();
		}
		else if constexpr(std::is_same_v<T, std::string> || std::is_arithmetic_v<T>) {
			return field;
		}
		else if constexpr(std::is_enum_v<T>) {
			if constexpr(has_enum_to_string_v<T>) {
				return enum_to_string(field);
			}
			else {
				return static_cast<int>(field);
			}
		}
		else {
			static_assert(always_false_v<T>, "Non-exhaustive to_lua_representation");
		}
	}
	
	template <class T, class K>
	void write_table_or_field(sol::table& output_table, const T& from, K&& key) {
		if constexpr(representable_as_lua_value_v<T>) {
			output_table[std::forward<K>(key)] = general_to_lua_value(from);
		}
		else {
			auto new_table = output_table.create();
			write_lua(new_table, from);
			output_table[std::forward<K>(key)] = new_table;
		}
	}

	template <class Archive, class Serialized>
	void verify_write_lua() {
		static_assert(
			!representable_as_lua_value_v<Serialized>, 
			"Directly representable, but no key (label) provided! Use write_representable_field to directly serialize this object."
		);

		static_assert(
			!is_optional_v<Serialized> && !is_maybe_v<Serialized>,
			"std::optional can only be serialized as a member object."
		);
	}

	template <class Archive, class Serialized>
	void write_lua(Archive& output_table, const Serialized& from) {
		verify_write_lua<Archive, Serialized>();

		if constexpr(has_lua_write_overload_v<Serialized>) {
			static_assert(has_lua_read_overload_v<Serialized>, "Has write_object_lua overload, but no read_object_lua overload.");

			write_object_lua(output_table, from);
		}
		else {
			write_lua_no_overload(output_table, from);
		}
	}
	template <class Archive, class Serialized>
	void write_lua_no_overload(Archive& output_table, const Serialized& from) {
		verify_write_lua<Archive, Serialized>();

		if constexpr(is_variant_v<Serialized>) {
			const auto variant_type_label = get_variant_type_label(from);
			const auto variant_content_label = get_variant_content_label(from);

			std::visit(
				[&](const auto& resolved) mutable {
					if constexpr(is_monostate_v<decltype(resolved)>) {
						output_table[variant_type_label] = "std_monostate";
					}
					else {
						const auto this_type_name = get_type_name_strip_namespace(resolved);
						output_table[variant_type_label] = this_type_name;
						write_table_or_field(output_table, resolved, variant_content_label);
					}
				}, 
				from
			);
		}
		else if constexpr(is_container_v<Serialized>) {
			using Container = Serialized;

			/*
				If container is associative and the keys are representable as lua values,
				write container table as:
				
				{
					ab = "abc",
					cd = "cde"
				}

				otherwise, write container table as a sequence of key-value pairs (like vector):

				{
					{ "ab", "abc" },
					{ "cd", "cde" }
				}
			*/

			if constexpr(is_associative_v<Container> && key_representable_as_lua_value_v<Container>) {
				for (auto&& element : from) {
					write_table_or_field(output_table, element.second, general_to_lua_value(element.first));
				}
			}
			else if constexpr(is_enum_array_v<Container>) {
				augs::for_each_enum_except_bounds([&](typename Container::enum_type e) {
					write_table_or_field(output_table, from[e], general_to_lua_value(e));
				});
			}
			else if constexpr(is_enum_boolset_v<Container>) {
				using E = typename Container::enum_type;

				int counter = 1;

				augs::for_each_enum_except_bounds([&](const E e) {
					if (from[e]) {
						const auto entry_name = augs::enum_to_string(e);
						output_table[counter] = entry_name;
						++counter;
					}
				});
			}
			else if constexpr(is_std_array_v<Container>) {
				for (std::size_t i = 0; i < from.size(); ++i) {
					write_table_or_field(output_table, from[i], static_cast<int>(i + 1));
				}
			}
			else {
				int counter = 1;

				for (auto&& element : from) {
					if constexpr(is_associative_v<Container>) {
						auto key_value_pair_table = output_table.create();

						write_table_or_field(key_value_pair_table, element.first, 1);
						write_table_or_field(key_value_pair_table, element.second, 2);

						output_table[counter] = key_value_pair_table;
					}
					else {
						write_table_or_field(output_table, element, counter);
					}

					++counter;
				}
			}
		}
		else if constexpr(is_pair_v<Serialized>) {
			write_table_or_field(output_table, from.first, 1);
			write_table_or_field(output_table, from.second, 2);
		}
		else {
			introspect(
				[&output_table](const auto& label, const auto& field) {
					using T = remove_cref<decltype(field)>;

					if constexpr(is_optional_v<T>) {
						if (field) {
							write_table_or_field(output_table, field.value(), label);
						}
					}
					else if constexpr(is_maybe_v<T>) {
						if constexpr(T::serialize_disabled) {
							if (field) {
								write_table_or_field(output_table, field.value, std::string("enabled_") + label);
							}
							else {
								write_table_or_field(output_table, field.value, std::string("disabled_") + label);
							}
						}
						else {
							if (field) {
								write_table_or_field(output_table, field.value, label);
							}
						}
					}
					else if constexpr(!is_padding_field_v<T>) {
						write_table_or_field(output_table, field, label);
					}
				},
				from
			);
		}
	}
}