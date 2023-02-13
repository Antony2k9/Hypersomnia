#include "augs/templates/history.hpp"
#include "augs/misc/imgui/imgui_utils.h"
#include "augs/misc/imgui/imgui_control_wrappers.h"
#include "augs/misc/imgui/imgui_controls.h"
#include "augs/misc/imgui/imgui_game_image.h"
#include "application/setups/editor/editor_filesystem.h"

#include "application/setups/editor/gui/editor_filesystem_gui.h"
#include "application/setups/editor/editor_setup.hpp"
#include "application/setups/editor/gui/widgets/inspectable_with_icon.h"
#include "application/setups/editor/gui/widgets/icon_button.h"
#include "application/setups/editor/gui/widgets/filesystem_node_widget.h"
#include "application/setups/editor/detail/simple_two_tabs.h"
#include "application/setups/editor/resources/resource_traits.h"
#include "test_scenes/test_scene_flavours.h"
#include "application/setups/editor/nodes/editor_node_defaults.h"
#include "view/rendering_scripts/for_each_iconed_entity.h"

editor_filesystem_gui::editor_filesystem_gui(const std::string& name) : base(name) {
	setup_special_filesystem(project_special_root);
	setup_special_filesystem(official_special_root);
}

void editor_filesystem_gui::perform(const editor_project_files_input in) {
	using namespace augs::imgui;
	using namespace editor_widgets;

	(void)in;
	
	entity_to_highlight.unset();

	auto window = make_scoped_window();

	if (dragged_resource.is_set()) {
		const bool payload_still_exists = [&]() {
			const auto payload = ImGui::GetDragDropPayload();
			return payload && payload->IsDataType("dragged_resource");

		}();

		const bool mouse_over_scene = !mouse_over_any_window();

		if (!payload_still_exists) {
			if (previewed_created_node.is_set()) {
				in.setup.finish_moving_selection();
			}

			dragged_resource.unset();
			previewed_created_node.unset();
		}
		else {
			if (mouse_over_scene) {
				auto instantiate = [&]<typename T>(const T& typed_resource, const auto resource_id) {
					const auto resource_name = typed_resource.get_display_name();
					const auto new_name = in.setup.get_free_node_name_for(resource_name);

					using node_type = typename T::node_type;

					node_type new_node;
					new_node.resource_id = resource_id;
					new_node.unique_name = new_name;
					new_node.editable.pos = in.setup.get_world_cursor_pos();

					::setup_node_defaults(new_node, typed_resource);

					world_position_started_dragging = new_node.editable.pos;

					create_node_command<node_type> command;

					command.built_description = typesafe_sprintf("Created %x", new_name);
					command.created_node = std::move(new_node);

					const auto place_over_node = in.setup.get_topmost_inspected_node();
					entity_to_highlight = in.setup.to_entity_id(place_over_node);

					if (const auto parent_layer = in.setup.find_best_layer_for_new_node()) {
						command.layer_id = parent_layer->layer_id;
						command.index_in_layer = parent_layer->index_in_layer;
					}
					else {
						command.create_layer = create_layer_command();
						command.create_layer->created_layer.unique_name = in.setup.get_free_layer_name();
					}

					const auto& executed = in.setup.post_new_command(std::move(command));

					in.setup.start_moving_selection();
					in.setup.make_last_command_a_child();
					in.setup.show_absolute_mover_pos_once();

					previewed_created_node = executed.get_node_id();

					in.setup.scroll_once_to(previewed_created_node);
				};

				if (!previewed_created_node.is_set()) {
					in.setup.on_resource(
						dragged_resource,
						[&]<typename T>(const T& typed_resource, const auto resource_id) {
							if constexpr(can_be_instantiated_v<T>) {
								instantiate(typed_resource, resource_id);
							}
						}
					);
				}
			}
			else {
				if (previewed_created_node.is_set()) {
					// LOG("Interrupting drag.");
					in.setup.finish_moving_selection();
					in.setup.undo_quiet();
					in.setup.undo_quiet();

					in.setup.rebuild_scene();
					previewed_created_node.unset();
				}
			}
		}
	}
	else {
		previewed_created_node.unset();
	}

	if (!window) {
		return;
	}

	acquire_keyboard_once();

	//rebuild_special_filesystem(in);

	thread_local ImGuiTextFilter filter;
	filter_with_hint(filter, "##FilesFilter", "Search resources...");

	int id_counter = 0;

	if (filter.IsActive()) {
		in.files_root.reset_filter_flags();
		get_viewed_special_root().reset_filter_flags();

		auto& f = filter;

		auto filter_callback = [&f](auto& entry) {
			const auto path_in_project = entry.get_path_in_project();

			if (f.PassFilter(path_in_project.string().c_str())) {
				entry.mark_passed_filter();
			}
		};

		in.files_root.for_each_entry_recursive(filter_callback);
		get_viewed_special_root().for_each_entry_recursive(filter_callback);
	}

	simple_two_tabs(
		current_tab,
		editor_resources_tab_type::PROJECT,
		editor_resources_tab_type::OFFICIAL,
		"Project",
		"Official"
	);

	auto child = scoped_child(showing_official() ? "nodes official view" : "nodes project view");

	const bool with_closed_folders = filter.IsActive();

	editor_filesystem_node* currently_viewed_root = &in.files_root;

	auto node_callback = [&](editor_filesystem_node& node) {
		auto id_scope = scoped_id(id_counter++);

		const bool filter_active = filter.IsActive();

		if (filter_active) {
			if (!node.passed_filter) {
				return;
			}
		}

		if (node.is_child_of_root()) {
			/* Ignore some editor-specific files in the project folder */

			if (node.name == "editor_view.json") {
				return;
			}
		}

		const bool pressed = filesystem_node_widget(
			in.setup,
			node,
			editor_icon_info_in(in),
			dragged_resource
		);

		if (pressed) {
			if (node.is_folder()) {
				if (!filter_active) {
					node.toggle_open();
				}
			}
			else {
				if (in.setup.exists(node.associated_resource)) {
					if (ImGui::GetIO().KeyShift && !in.setup.wants_multiple_selection()) {
						auto last_inspected = in.setup.get_last_inspected_any();
						auto last_resource = std::get_if<editor_resource_id>(&last_inspected);

						const auto shift_clicked_resource_id = node.associated_resource;

						if (in.setup.inspects_any<editor_resource_id>() && last_resource && *last_resource != shift_clicked_resource_id) {
							int state = 0;

							in.setup.clear_inspector();

							auto shift_click_callback = [&state, &last_resource, &shift_clicked_resource_id, &in](editor_filesystem_node& it_node) {
								if (state == 2) {
									return;
								}

								const editor_resource_id id = it_node.associated_resource;

								if (id == shift_clicked_resource_id || id == *last_resource) {
									++state;
								}

								if (state && id.is_set()) {
									in.setup.inspect_add_quiet(id);
								}
							};

							currently_viewed_root->in_ui_order(shift_click_callback, with_closed_folders);

							in.setup.after_quietly_adding_inspected();
							in.setup.quiet_set_last_inspected_any(*last_resource);
						}
					}
					else {
						in.setup.inspect(node.associated_resource);
					}
				}
			}
		}
	};

	in.files_root.in_ui_order(node_callback, with_closed_folders);

	ImGui::Separator();

	if (!showing_official()) {
		const bool special_resource_inspected = false;

		if (icon_button("##NewResource", in.necessary_images[assets::necessary_image_id::EDITOR_ICON_ADD], [](){}, "New special resource", !showing_official())) {

		}

		ImGui::SameLine();

		if (icon_button("##Duplicate", in.necessary_images[assets::necessary_image_id::EDITOR_ICON_CLONE], [](){}, "Duplicate selection", special_resource_inspected)) {

		}

		ImGui::SameLine();

		{
			const auto remove_bgs = std::array<rgba, 3> {
				rgba(0, 0, 0, 0),
				rgba(80, 20, 20, 255),
				rgba(150, 40, 40, 255)
			};

			const auto remove_tint = rgba(220, 80, 80, 255);

			if (icon_button("##Remove", in.necessary_images[assets::necessary_image_id::EDITOR_ICON_REMOVE], [](){}, "Remove selection", !showing_official() && special_resource_inspected, remove_tint, remove_bgs)) {

			}
		}

		ImGui::SameLine();
	}

	text_disabled("(Special resources)");

	currently_viewed_root = &get_viewed_special_root();
	currently_viewed_root->in_ui_order(node_callback, with_closed_folders, true);
}

void editor_filesystem_gui::clear_drag_drop() {
	ImGui::ClearDragDrop();
	augs::imgui::release_mouse_buttons();

	previewed_created_node.unset();
	dragged_resource.unset();
}

void editor_filesystem_gui::setup_special_filesystem(editor_filesystem_node& root) {
	root.clear();
	root.subfolders.resize(10);
	root.should_sort = false;

	auto i = 0;

	auto& lights_folder = root.subfolders[i++];
	auto& particles_folder = root.subfolders[i++];
	auto& wandering_pixels_folder = root.subfolders[i++];
	auto& prefabs_folder = root.subfolders[i++];

	auto& firearms_folder = root.subfolders[i++];
	auto& ammunition_folder = root.subfolders[i++];
	auto& melee_weapons_folder = root.subfolders[i++];
	auto& explosives_folder = root.subfolders[i++];

	auto& point_markers_folder = root.subfolders[i++];
	auto& area_markers_folder = root.subfolders[i++];

	lights_folder.name = "Lights";
	particles_folder.name = "Particles";
	wandering_pixels_folder.name = "Wandering pixels";
	prefabs_folder.name = "Prefabs";

	firearms_folder.name = "Firearms";
	ammunition_folder.name = "Ammunition";
	melee_weapons_folder.name = "Melee weapons";
	explosives_folder.name = "Explosives";

	point_markers_folder.name = "Point markers";
	area_markers_folder.name = "Area markers";

	for (auto& s : root.subfolders) {
		s.type = editor_filesystem_node_type::FOLDER;
	}

	root.adding_children_finished();
}

void editor_filesystem_gui::rebuild_special_filesystem(editor_setup& setup) {
	rebuild_special_filesystem(project_special_root, false, setup);
}

void editor_filesystem_gui::rebuild_official_special_filesystem(editor_setup& setup) {
	rebuild_special_filesystem(official_special_root, true, setup);
}

void editor_filesystem_gui::rebuild_special_filesystem(editor_filesystem_node& root, bool official, editor_setup& setup) {
	auto i = 0;

	const auto& resources = official ? setup.get_official_resources() : setup.get_project().resources;

	auto& lights_folder = root.subfolders[i++];
	auto& particles_folder = root.subfolders[i++];
	auto& wandering_pixels_folder = root.subfolders[i++];
	auto& prefabs_folder = root.subfolders[i++];

	auto& firearms_folder = root.subfolders[i++];
	auto& ammunition_folder = root.subfolders[i++];
	auto& melee_weapons_folder = root.subfolders[i++];
	auto& explosives_folder = root.subfolders[i++];

	auto& point_markers_folder = root.subfolders[i++];
	auto& area_markers_folder = root.subfolders[i++];

	(void)prefabs_folder;
	(void)wandering_pixels_folder;

	auto handle = [&]<typename P>(editor_filesystem_node& parent, P& pool, const auto icon_id) {
		using resource_type = typename P::value_type;

		parent.necessary_atlas_icon = icon_id;
		parent.after_text = typesafe_sprintf("(%x)", pool.size());

		parent.files.resize(pool.size());
		std::size_t i = 0;

		auto resource_handler = [&](const auto raw_id, const resource_type& typed_resource) {
			editor_resource_id resource_id;

			resource_id.is_official = official;
			resource_id.raw = raw_id;
			resource_id.type_id.set<resource_type>();

			auto& new_node = parent.files[i++];
			new_node.associated_resource = resource_id;
			new_node.type = editor_filesystem_node_type::OTHER_RESOURCE;

			const auto& initial_intercosm = setup.get_initial_scene();

			if constexpr(is_one_of_v<resource_type, editor_point_marker_resource, editor_area_marker_resource>) {
				new_node.name = typed_resource.get_display_name();
			}
			else {
				std::visit(
					[&](const auto& tag) {
						const auto& flavour = initial_intercosm.world.get_flavour(to_entity_flavour_id(tag));
						auto name = flavour.get_name();

						const auto id = str_ops(name).to_lowercase().replace_all(" ", "_").subject;
						new_node.name = id;

						if (auto sprite = flavour.template find<invariants::sprite>()) {
							new_node.custom_thumbnail_path = initial_intercosm.viewables.image_definitions[sprite->image_id].get_source_path().resolve({});
						}

						if (auto animation = flavour.template find<invariants::animation>()) {
							if (animation->id.is_set()) {
								auto path_s = new_node.custom_thumbnail_path.string();
								ensure(ends_with(path_s, "_1.png"));

								path_s.erase(path_s.end() - std::strlen("_1.png"), path_s.end());
								path_s += "_*.png";

								new_node.custom_thumbnail_path = path_s;
								// LOG_NVPS(new_node.custom_thumbnail_path);
							}
						}

						if (auto gun = flavour.template find<invariants::gun>()) {
							if (gun->shoot_animation.is_set()) {
								//const auto image_id = initial_intercosm.world.get_logical_assets().plain_animations[gun->shoot_animation].frames[0].image_id;
								//auto path_s = initial_intercosm.viewables.image_definitions[image_id].get_source_path().resolve({}).string();
								//LOG_NVPS(path_s);

								auto path_s = new_node.custom_thumbnail_path;
								path_s.replace_extension("");
								path_s += "_shot_*.png";

								/* auto path_basic = path_s.string(); */
								/* cut_trailing_number(path_basic); */

								/* if (path_basic.back() == '_') { */
								/* 	path_basic.pop_back(); */
								/* } */

								/* path_basic += "_shot_*.png"; */

								new_node.custom_thumbnail_path = path_s;
								// LOG_NVPS(new_node.custom_thumbnail_path);
							}
						}
					},
					*typed_resource.official_tag
				);
			}
		};

		pool.for_each_id_and_object(resource_handler);
		parent.adding_children_finished();
	};

	// TODO: redirect to proper pools!
	handle(particles_folder, resources.get_pool_for<editor_particles_resource>(), assets::necessary_image_id::EDITOR_ICON_PARTICLE_SOURCE);
	handle(wandering_pixels_folder, resources.get_pool_for<editor_wandering_pixels_resource>(), assets::necessary_image_id::EDITOR_ICON_WANDERING_PIXELS);
	//handle(prefabs_folder, resources.get_pool_for<editor_light_resource>(), assets::necessary_image_id::EDITOR_ICON_BOMBSITE_A);

	handle(lights_folder, resources.get_pool_for<editor_light_resource>(), assets::necessary_image_id::EDITOR_ICON_LIGHT);

	handle(firearms_folder, resources.get_pool_for<editor_firearm_resource>(), assets::necessary_image_id::CHAMBER_SLOT_ICON);
	handle(ammunition_folder, resources.get_pool_for<editor_ammunition_resource>(), assets::necessary_image_id::DETACHABLE_MAGAZINE_SLOT_ICON);
	handle(melee_weapons_folder, resources.get_pool_for<editor_melee_resource>(), assets::necessary_image_id::SHOULDER_SLOT_ICON);
	handle(explosives_folder, resources.get_pool_for<editor_explosive_resource>(), assets::necessary_image_id::BOMB_INDICATOR);

	handle(point_markers_folder, resources.get_pool_for<editor_point_marker_resource>(), assets::necessary_image_id::DANGER_INDICATOR);
	handle(area_markers_folder, resources.get_pool_for<editor_area_marker_resource>(), assets::necessary_image_id::EDITOR_ICON_BOMBSITE_A);

	root.set_parents(0);
}
