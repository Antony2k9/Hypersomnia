#include "application/intercosm.h"

#include "application/setups/editor/editor_command_input.h"
#include "application/setups/editor/editor_folder.h"

#include "application/setups/editor/gui/editor_entity_selector.h"
#include "application/setups/editor/gui/editor_entity_mover.h"

#include "application/setups/editor/gui/editor_fae_gui.h"

all_viewables_defs& editor_command_input::get_viewable_defs() const {
	return folder.work->viewables;
}

const all_logical_assets& editor_command_input::get_logical_assets() const {
	return get_cosmos().get_logical_assets();
}

cosmos& editor_command_input::get_cosmos() const {
	return folder.work->world;
}

void editor_command_input::interrupt_tweakers() const {
	fae_gui.interrupt_tweakers();
}

void editor_command_input::purge_selections() const {
	folder.view.selected_entities.clear();
	selector.clear();
	mover.escape();
}

void editor_command_input::clear_selection_of(const entity_id id) const {
	erase_element(folder.view.selected_entities, id);

	selector.clear_selection_of(id);
}
