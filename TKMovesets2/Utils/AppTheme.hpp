#pragma once
#include <imgui.h>
#include <initializer_list>

// Shared application palette, sampled from the original editor reference.
namespace AppTheme {
inline constexpr ImU32 Background=IM_COL32(15,15,15,255);
inline constexpr ImU32 Input=IM_COL32(30,47,73,255);
inline constexpr ImU32 Reference=IM_COL32(56,82,124,255);
inline constexpr ImU32 Accent=IM_COL32(77,131,219,255);
inline void Apply() {
    ImGui::StyleColorsDark();
    auto& c=ImGui::GetStyle().Colors;
    const auto set=[&](ImGuiCol index,ImU32 value){c[index]=ImGui::ColorConvertU32ToFloat4(value);};
    set(ImGuiCol_Text,IM_COL32(245,245,245,255));
    set(ImGuiCol_TextDisabled,IM_COL32(145,145,150,255));
    for(auto i:{ImGuiCol_WindowBg,ImGuiCol_ChildBg,ImGuiCol_DockingEmptyBg})set(i,Background);
    set(ImGuiCol_PopupBg,IM_COL32(20,20,20,255));
    set(ImGuiCol_MenuBarBg,IM_COL32(36,36,36,255));
    set(ImGuiCol_Border,IM_COL32(90,90,96,255));
    set(ImGuiCol_BorderShadow,0);
    for(auto i:{ImGuiCol_FrameBg,ImGuiCol_Button,ImGuiCol_Tab,ImGuiCol_TitleBg,ImGuiCol_TitleBgCollapsed})set(i,Input);
    for(auto i:{ImGuiCol_FrameBgHovered,ImGuiCol_ButtonHovered,ImGuiCol_TabSelected,ImGuiCol_TabDimmedSelected})set(i,Reference);
    for(auto i:{ImGuiCol_FrameBgActive,ImGuiCol_ButtonActive,ImGuiCol_HeaderActive,ImGuiCol_TabHovered,ImGuiCol_TitleBgActive,ImGuiCol_SliderGrabActive,ImGuiCol_NavCursor})set(i,Accent);
    set(ImGuiCol_Header,IM_COL32(30,57,87,255));
    set(ImGuiCol_HeaderHovered,IM_COL32(56,82,124,200));
    set(ImGuiCol_TabDimmed,IM_COL32(24,35,51,255));
    set(ImGuiCol_TabSelectedOverline,0);set(ImGuiCol_TabDimmedSelectedOverline,0);
    set(ImGuiCol_CheckMark,Accent);set(ImGuiCol_CheckboxSelectedBg,Reference);
    set(ImGuiCol_SliderGrab,Reference);set(ImGuiCol_InputTextCursor,IM_COL32(245,245,245,255));
    set(ImGuiCol_ScrollbarBg,Background);
    set(ImGuiCol_ScrollbarGrab,IM_COL32(72,72,76,255));
    set(ImGuiCol_ScrollbarGrabHovered,IM_COL32(100,100,106,255));
    set(ImGuiCol_ScrollbarGrabActive,IM_COL32(125,125,132,255));
    for(auto i:{ImGuiCol_Separator,ImGuiCol_TableBorderStrong})set(i,IM_COL32(80,80,86,255));
    set(ImGuiCol_TableBorderLight,IM_COL32(55,55,61,255));
    set(ImGuiCol_SeparatorHovered,Reference);set(ImGuiCol_SeparatorActive,Accent);
    set(ImGuiCol_TableHeaderBg,IM_COL32(48,48,50,255));
    set(ImGuiCol_TableRowBg,0);set(ImGuiCol_TableRowBgAlt,IM_COL32(255,255,255,15));
    set(ImGuiCol_ResizeGrip,IM_COL32(56,82,124,100));
    set(ImGuiCol_ResizeGripHovered,Reference);set(ImGuiCol_ResizeGripActive,Accent);
    set(ImGuiCol_TextSelectedBg,IM_COL32(77,131,219,100));
    set(ImGuiCol_DockingPreview,IM_COL32(77,131,219,120));
    set(ImGuiCol_TextLink,IM_COL32(133,177,241,255));
    set(ImGuiCol_TreeLines,IM_COL32(80,80,86,255));
    set(ImGuiCol_ModalWindowDimBg,IM_COL32(0,0,0,150));
    set(ImGuiCol_NavWindowingDimBg,IM_COL32(0,0,0,100));
}

// Only dock-tab colours vary by record type. Zero retains the default move blue.
inline ImU32 RecordTab(unsigned table) {
    switch(table) {
    case 12:return 0;
    case 0:return IM_COL32(143,55,62,255);             // reactions: red
    case 6:case 7:case 8:return IM_COL32(100,65,131,255); // cancels: purple
    case 1:return IM_COL32(139,105,49,255);           // requirements: ochre
    case 2:return IM_COL32(146,77,49,255);            // hit conditions: copper
    case 3:return IM_COL32(94,103,143,255);           // projectiles: slate violet
    case 4:case 5:return IM_COL32(57,113,114,255);     // pushbacks: teal
    case 9:case 10:case 11:return IM_COL32(71,116,78,255); // properties: green
    case 13:case 19:return IM_COL32(127,77,111,255);   // voice/dialogue: rose
    case 14:case 15:case 16:return IM_COL32(49,109,139,255); // inputs: cyan
    case 17:case 18:return IM_COL32(129,104,69,255);   // cameras: bronze
    default:return 0;
    }
}
inline int PushTabAccent(ImU32 accent) {
    if(!accent)return 0;
    const auto base=ImGui::ColorConvertU32ToFloat4(accent);
    const auto shade=[&](float factor){return ImVec4(base.x*factor,base.y*factor,base.z*factor,1);};
    ImGui::PushStyleColor(ImGuiCol_Tab,shade(.65f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,base);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,shade(1.2f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmed,shade(.5f));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected,shade(.85f));
    return 5;
}
inline ImGuiID DockSpace(ImGuiID id,ImVec2 size) {
    // Dock tab strips inherit title colours by default. Their empty area is
    // black in the reference; only the application's title bar is bright blue.
    ImGui::PushStyleColor(ImGuiCol_TitleBg,Background);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,Background);
    const auto result=ImGui::DockSpace(id,size);
    ImGui::PopStyleColor(2);
    return result;
}
}
