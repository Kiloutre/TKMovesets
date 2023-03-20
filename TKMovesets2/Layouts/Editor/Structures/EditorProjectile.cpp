#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorProjectile.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorProjectile::EditorProjectile(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Projectile;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorProjectile::OnFieldLabelClick(EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "cancel_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
	}
	else if (name == "hit_condition_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_HitCondition, id);
	}
}
