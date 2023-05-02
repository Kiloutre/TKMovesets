#include "imgui_extras.hpp"
#include "EditorCancelExtra.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorCancelExtra::EditorCancelExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_CancelExtradata;
	InitForm(windowTitleBase, t_id, editor);
}
