#include "AppTheme.hpp"
#include "EmbeddedInterface.hpp"
#include <sstream>
#include "EditorFormList.hpp"
#include "RetailNames.hpp"
#include "RetailCommands.hpp"
#include "RetailFieldCodecs.hpp"
#include "EditorVisuals_t8Retail.hpp"
#include "MovesetFile.hpp"
#include "Games.hpp"
#include "Compression.hpp"
#include "Localization.hpp"
#include "constants.h"
#include "imgui_extras.hpp"
#include <imgui_internal.h>
#include "FileIO.hpp"
#include "AnimationPreview.hpp"
#include <optional>
#include <nlohmann/json.hpp>
#include <fstream>
#include <charconv>
#include <format>
#include <cctype>
#include <filesystem>
#include <thread>
#include <mutex>
#include <map>
#include <shobjidl.h>
#include <shellapi.h>

namespace R=RetailT8Snapshot;
namespace {
// Localized text with std::format placeholders ("{}"), resolved at call time.
template<class... A> std::string Fmt(const char* key,A&&... a){return std::vformat(_(key),std::make_format_args(a...));}
uint64_t RetailInputValue(const EditorInput* field){
    if(field->flags&EditorInput_Float){float value=std::strtof(field->buffer,nullptr);uint32_t bits;memcpy(&bits,&value,4);return bits;}
    return std::strtoull(field->buffer,nullptr,(field->flags&EditorInput_Hex)?16:10);
}
// EditorVisuals_t7 TEditorRequirements/TEditorExtraproperties value text.
std::string RetailValueText(uint32_t value){
    const auto signedValue=static_cast<int32_t>(value);if(signedValue>=0 && signedValue<=15)return std::to_string(signedValue);
    float asFloat;memcpy(&asFloat,&value,4);return std::format("{} / {} / 0x{:x} / {:f}f",signedValue,value,value,asFloat);
}
// EditorVisuals_t7 TEditorExtraproperties starting frame text (retail keeps the 0x8000/0x4000 flags, checked on Jin).
std::string RetailPropertyFrameText(uint32_t frame){
    if(frame==9999)return _("edition.extraproperty.every_frame");
    if(frame>=32769)return frame==32769?std::string(_("edition.extraproperty.instant")):std::format("{} (+{})",_("edition.extraproperty.instant"),frame-32769);
    if(frame>=0x4000){char buffer[128];snprintf(buffer,sizeof(buffer),_("edition.extraproperty.every_nth_frame"),int(frame-0x4000));return buffer;}
    return std::to_string(frame);
}
std::wstring BrowsePanm(){
    std::wstring result;const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return result;
    IFileOpenDialog* dialog=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
        COMDLG_FILTERSPEC filters[]{{L"Animations (Tekken 8 .panm, Tekken 7 0x64 / 0xC8)",L"*.panm;*.bin;*.anm"},{L"All files",L"*.*"}};dialog->SetFileTypes(2,filters);dialog->SetOptions(FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR);
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();}}
        dialog->Release();
    }if(SUCCEEDED(initialized))CoUninitialize();return result;
}
std::wstring SuggestedPanmName(std::string name){
    for(auto& c:name)if(static_cast<unsigned char>(c)<0x20 || std::string_view("<>:\"/\\|?*").find(c)!=std::string_view::npos)c='_';
    return Helpers::to_unicode(name.empty()?std::string("animation"):name)+L".panm";
}
std::wstring BrowseSavePanm(const std::wstring& suggested){
    std::wstring result;const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return result;
    IFileSaveDialog* dialog=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
        COMDLG_FILTERSPEC filter{L"Tekken 8 animation",L"*.panm"};dialog->SetFileTypes(1,&filter);dialog->SetDefaultExtension(L"panm");dialog->SetFileName(suggested.c_str());
        dialog->SetOptions(FOS_FORCEFILESYSTEM|FOS_OVERWRITEPROMPT|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR);
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();}}
        dialog->Release();
    }if(SUCCEEDED(initialized))CoUninitialize();return result;
}
// Hover text of a retail field: edition.t8.<table>.<field>.description. Hitbox fields share one key per
// field kind (hitbox_3_start -> hitbox_start, hitbox_3_offset_a_1 -> hitbox_offset_a).
std::string RetailFieldDescription(const char* table,std::string field){
    if(field.starts_with("hitbox_")){auto second=field.find('_',7);if(second!=std::string::npos)field="hitbox"+field.substr(second);}
    if(field.starts_with("hitbox_offset_a_"))field="hitbox_offset_a";
    if(field.starts_with("hitbox_offset_b_"))field="hitbox_offset_b";
    const auto key=std::format("edition.t8.{}.{}.description",table,field);
    return Localization::HasText(key)?std::string(_(key.c_str())):std::string();
}
// Parameter meanings per requirement / property id, generated from the Tekken8 cnt handler review by
// tools/generate_retail_param_labels.py (TKMInterface/editor_t8-retail_{requirement,property}_params.txt).
struct RetailParamInfo {std::string name,param,confidence,top,meaning;};
class RetailParamLabels {
public:
    static const RetailParamLabels& Get(){static RetailParamLabels labels;return labels;}
    const RetailParamInfo* Requirement(uint32_t id) const {auto found=m_requirements.find(id);return found==m_requirements.end()?nullptr:&found->second;}
    const RetailParamInfo* Property(uint32_t id) const {auto found=m_properties.find(id);return found==m_properties.end()?nullptr:&found->second;}
private:
    std::map<uint32_t,RetailParamInfo> m_requirements,m_properties;
    RetailParamLabels(){Load(m_requirements,"requirement");Load(m_properties,"property");}
    // The interface data sits next to the executable; headless commands may run from another directory.
    static std::filesystem::path DataPath(const std::string& file) {
        std::filesystem::path relative=std::filesystem::path(INTERFACE_DATA_DIR)/file;std::error_code error;
        if(std::filesystem::exists(relative,error))return relative;
        wchar_t executable[32768];const auto length=GetModuleFileNameW(nullptr,executable,_countof(executable));
        if(length && length<_countof(executable))return std::filesystem::path(executable).parent_path()/INTERFACE_DATA_DIR/file;
        return relative;
    }
    static void Load(std::map<uint32_t,RetailParamInfo>& into,const char* kind) {
        // TKMInterface file on disk if present, else the copy embedded in the exe
        std::istringstream file(EmbeddedInterface::Read(std::format("editor_t8-retail_{}_params.txt",kind)).value_or(std::string()));std::string line;
        while(std::getline(file,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            if(line.empty() || line[0]=='#')continue;
            std::vector<std::string> columns;size_t start=0;
            while(true){auto tab=line.find('\t',start);columns.push_back(line.substr(start,tab==std::string::npos?std::string::npos:tab-start));if(tab==std::string::npos)break;start=tab+1;}
            if(columns.size()<6)continue;
            uint32_t id=0;auto parsed=std::from_chars(columns[0].data(),columns[0].data()+columns[0].size(),id);
            if(parsed.ec!=std::errc{})continue;
            into[id]={columns[1],columns[2],columns[3],columns[4],columns[5]};
        }
    }
};
// Port of EditorVisuals_t7 TEditorMove_Animations: every .panm under editor_library, grouped by folder
// (Export move PANMs / Extract animations write one folder per document). Loaded on a thread, shared by
// all record forms; frame counts come from the PANM structure.
class RetailAnimationLibrary {
public:
    struct File {std::string name,lower,frames,sizeKb;std::wstring path;};
    struct Folder {std::string name;std::vector<File> files;};
    using Folders=std::shared_ptr<const std::vector<Folder>>;
    static RetailAnimationLibrary& Get(){static RetailAnimationLibrary library;return library;}
    ~RetailAnimationLibrary(){if(m_thread.joinable())m_thread.join();}
    void EnsureLoaded(){std::lock_guard lock(m_mutex);if(m_loaded || m_loading)return;Start();}
    void Refresh(){std::lock_guard lock(m_mutex);if(m_loading)return;Start();}
    bool Loading(){std::lock_guard lock(m_mutex);return m_loading;}
    Folders Get_(){std::lock_guard lock(m_mutex);return m_folders;}
private:
    std::mutex m_mutex;std::thread m_thread;bool m_loading=false,m_loaded=false;Folders m_folders=std::make_shared<const std::vector<Folder>>();
    void Start(){if(m_thread.joinable())m_thread.join();m_loading=true;m_thread=std::thread([this]{Load();});}
    void Load(){
        auto result=std::make_shared<std::vector<Folder>>();
        try{
            const auto base=std::filesystem::absolute(EDITOR_LIB_DIRECTORY);std::error_code error;
            if(std::filesystem::is_directory(base,error))for(const auto& directory:std::filesystem::directory_iterator(base,error)){
                if(!directory.is_directory(error))continue;
                Folder folder;folder.name=Helpers::to_utf8(directory.path().filename().wstring());
                for(const auto& entry:std::filesystem::recursive_directory_iterator(directory.path(),std::filesystem::directory_options::skip_permission_denied,error)){
                    if(!entry.is_regular_file(error))continue;const auto& path=entry.path();
                    if(path.extension()!=L".panm")continue;
                    File file;file.path=path.wstring();file.name=Helpers::to_utf8(path.stem().wstring());file.lower=EditorVisualsT8Retail::NormalizeSearch(file.name);
                    file.sizeKb=std::format("{:.1f}",double(entry.file_size(error))/1000.0);
                    try{file.frames=std::to_string(EditorT8Retail::PanmFrames(EditorT8Retail::ReadPanmFile(path)));}catch(const std::exception&){file.frames="?";}
                    folder.files.push_back(std::move(file));
                }
                if(folder.files.empty())continue;
                std::sort(folder.files.begin(),folder.files.end(),[](const File& a,const File& b){return a.lower<b.lower;});
                folder.name+=std::format(" - {}",folder.files.size());result->push_back(std::move(folder));
            }
            std::sort(result->begin(),result->end(),[](const Folder& a,const Folder& b){return a.name<b.name;});
        }catch(const std::exception&){}
        std::lock_guard lock(m_mutex);m_folders=std::move(result);m_loading=false;m_loaded=true;
    }
};
// Changed-field marking: a field whose text differs from its last applied value (savedText, recorded when the
// form opens and after every successful Apply) is drawn with FORM_INPUT_CHANGED. Invalid fields stay red.
void MarkFieldsSaved(const InputMap& fields){for(const auto& [name,field]:fields)if(field && field->buffer)field->savedText=field->buffer;}
bool FieldChanged(const EditorInput* field){return field->buffer && (!field->savedText || *field->savedText!=field->buffer);}
// Swaps the field colour for the changed tint while the shared EditorForm::RenderInput draws it.
class ChangedFieldTint {
    EditorInput* m_field;int m_color;bool m_active;
public:
    explicit ChangedFieldTint(EditorInput* field,bool newRow=false):m_field(field),m_color(field->color),m_active(!field->errored && (newRow || FieldChanged(field))){if(m_active)field->color=FORM_INPUT_CHANGED;}
    ~ChangedFieldTint(){if(m_active)m_field->color=m_color;}
    ChangedFieldTint(const ChangedFieldTint&)=delete;ChangedFieldTint& operator=(const ChangedFieldTint&)=delete;
};
class RetailRecordForm final : public EditorForm {
    size_t m_table;
    EditorT8Retail* m_retail;
    std::array<std::array<char,256>,2> m_names{};
    std::array<std::string,2> m_savedNames;
    double m_lastTyped=0;
    // m_lastTyped of the edit state whose automatic (live editing) Apply failed: retried only after the next edit.
    double m_failedTyped=-1;
    char m_animationSearch[128]{};
    std::vector<std::pair<std::string,uint32_t>> m_animationChoices;
    bool m_openAnimationPicker=false;
    // Returns true when the file was bound to an animation already in the document (the popup can close).
    bool ImportPanmBytes(std::vector<Byte> bytes,const std::wstring& filename){
        if(auto donor=m_retail->FindBoundAnimation(bytes)){
            auto found=std::find_if(m_animationChoices.begin(),m_animationChoices.end(),[&](const auto& item){return item.second==*donor;});
            if(found==m_animationChoices.end())throw std::runtime_error(_("edition.t8.animation_identity"));
            m_pendingPanm.clear();m_pendingName.clear();snprintf(m_names[1].data(),m_names[1].size(),"%s",found->first.c_str());
            unsavedChanges=true;m_lastTyped=ImGui::GetTime();m_operationError.clear();return true;
        }
        m_pendingNote.clear();
        try{EditorT8Retail::ValidatePanm(bytes);}catch(const std::exception&){m_retail->RewritePanm(bytes);m_pendingNote=_("edition.t8.panm_rewritten");}
        m_pendingPanm=std::move(bytes);m_pendingName=Helpers::to_utf8(std::filesystem::path(filename).stem().wstring());
        snprintf(m_names[1].data(),m_names[1].size(),"new: %s",m_pendingName.c_str());
        unsavedChanges=true;m_lastTyped=ImGui::GetTime();m_operationError=m_pendingNote;return false;
    }
    void RenderLibraryTab(const std::string& query){
        auto& library=RetailAnimationLibrary::Get();
        if(ImGui::SmallButton(_("edition.t8.animation_library.refresh")))library.Refresh();
        ImGui::SameLine();
        if(ImGui::SmallButton(_("edition.t8.animation_library.open_folder"))){const auto path=std::filesystem::absolute(EDITOR_LIB_DIRECTORY);std::error_code error;std::filesystem::create_directories(path,error);ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWDEFAULT);}
        if(library.Loading()){ImGui::SameLine();ImGui::TextDisabled("%s",_("edition.t8.animation_library.loading"));}
        auto folders=library.Get_();
        if(folders->empty() && !library.Loading())ImGui::TextWrapped("%s",_("edition.t8.animation_library.empty"));
        const char* frames=_("edition.t8.animation_library.frames");const char* size=_("edition.animation_list.size");const char* import=_("edition.t8.animation_library.import");
        for(size_t f=0;f<folders->size();++f){
            const auto& folder=(*folders)[f];
            std::vector<const RetailAnimationLibrary::File*> shown;
            for(const auto& file:folder.files)if(query.empty() || file.lower.find(query)!=std::string::npos)shown.push_back(&file);
            if(shown.empty())continue;
            ImGui::PushID(int(f));
            if(!query.empty())ImGui::SetNextItemOpen(true);
            if(ImGui::TreeNode(folder.name.c_str())){
                if(ImGui::BeginTable("##library",4,ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_RowBg|ImGuiTableFlags_Borders|ImGuiTableFlags_Resizable,ImVec2(ImGui::GetContentRegionAvail().x,0))){
                    ImGui::TableSetupColumn(_("edition.animation_list.anim_name"),ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn(frames);ImGui::TableSetupColumn(size);ImGui::TableSetupColumn("##import");ImGui::TableHeadersRow();
                    ImGuiListClipper clip;clip.Begin(int(shown.size()));
                    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){
                        const auto& file=*shown[i];ImGui::PushID(i);ImGui::TableNextRow();
                        ImGui::TableNextColumn();ImGui::TextUnformatted(file.name.c_str());if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Helpers::to_utf8(file.path).c_str());
                        ImGui::TableNextColumn();ImGui::TextUnformatted(file.frames.c_str());
                        ImGui::TableNextColumn();ImGui::Text("%s KB",file.sizeKb.c_str());
                        ImGui::TableNextColumn();
                        if(ImGui::SmallButton(import))try{if(ImportPanmBytes(EditorT8Retail::ReadPanmFile(file.path),std::filesystem::path(file.path).filename().wstring()))ImGui::CloseCurrentPopup();}
                        catch(const std::exception& e){m_operationError=e.what();}
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    // Custom PANM picked in the animation library, imported on Apply as a new animation of this move
    // (EditorT8Retail::ImportPanm), named after its file.
    std::vector<Byte> m_pendingPanm;
    std::string m_pendingName;
    std::string m_pendingNote;
    // 3D preview of the move's animation (moves table only): the pending PANM once one is picked, else the applied one.
    AnimationPreview m_preview;
    AnimationPreview::Input PreviewInput() {
        auto number=[&](const std::string& name)->int64_t{
            auto found=m_fieldIdentifierMap.find(name);
            if(found==m_fieldIdentifierMap.end() || found->second->errored)return 0;
            return strtoll(found->second->buffer,nullptr,0);
        };
        AnimationPreview::Input input;
        uint64_t token=0;
        if(structureId<m_retail->Count(12)){auto record=m_retail->Record(12,structureId);uint64_t value=0,key=0;memcpy(&value,record.data()+0x20,8);memcpy(&key,record.data()+0x28,8);token=value*0x9E3779B97F4A7C15ULL^key;}
        if(!m_pendingPanm.empty())token^=(uint64_t(uintptr_t(m_pendingPanm.data()))<<1)^(uint64_t(m_pendingPanm.size())<<40)^1;
        input.token=token|1;
        input.load=[this]{return m_pendingPanm.empty()?m_retail->MoveAnimation(structureId):m_pendingPanm;};
        // Overall active window, then the hitbox slots (each its own range; most lie inside the window).
        const auto first=number("active_start"),last=number("active_end");
        if(first>0 && last>=first)input.active.emplace_back(int(first),int(last));
        for(int i=0;i<8;++i){const auto a=number("hitbox_"+std::to_string(i)+"_start"),b=number("hitbox_"+std::to_string(i)+"_last");if(a>0 && b>=a && !(a>=first && b<=last))input.active.emplace_back(int(a),int(b));}
        if(structureId<m_retail->Count(12))input.displacements=m_retail->MoveDisplacements(structureId,&input.conditional);
        // Opponent mannequin: the reactions of the move's likely hit, and any move's animation (idle, reactions, pose).
        if(structureId<m_retail->Count(12)){input.opponent.reactions=m_retail->MoveHitReactions(structureId,false);input.opponent.airReactions=m_retail->MoveHitReactions(structureId,true);}
        input.opponent.move=[retail=m_retail](int id)->AnimationPreview::Input::Animation{
            if(id<0)return {};
            uint32_t row=0;
            try{row=retail->ResolveMove(uint32_t(id));}catch(const std::exception&){return {};}
            const auto record=retail->Record(12,row);uint64_t value=0,key=0;memcpy(&value,record.data()+0x20,8);memcpy(&key,record.data()+0x28,8);
            return {(value*0x9E3779B97F4A7C15ULL^key)|1,[retail,row]{return retail->MoveAnimation(row);}};
        };
        // Hitbox slots as typed, inside the move's active window (0 = no hitboxes).
        auto real=[&](const std::string& name)->float{
            auto found=m_fieldIdentifierMap.find(name);
            if(found==m_fieldIdentifierMap.end() || found->second->errored)return 0;
            return strtof(found->second->buffer,nullptr);
        };
        if(first>0)for(int i=0;i<8;++i){
            const auto prefix="hitbox_"+std::to_string(i)+"_";
            const auto location=uint32_t(number(prefix+"location"));
            if(!location)continue;
            const int a=int((std::max)(number(prefix+"start"),first)),b=int((std::min)(number(prefix+"last"),last>=first?last:INT64_MAX));
            if(b>=a)input.hitboxes.push_back({a,b,location,real(prefix+"radius"),real(prefix+"vertical_height")});
        }
        return input;
    }
    // Fields drawn with a decoder combo (RetailFieldCodecs.hpp) before their raw input, by field name: every committed
    // transaction replaces the form's EditorInput objects with clones, so their addresses are not stable.
    enum class Codec { HitLevel, HitboxLocation, Vulnerability };
    std::map<std::string,Codec> m_codecs;
    EditorVisualsT8Retail& Owner(){return *static_cast<EditorVisualsT8Retail*>(m_baseWindow);}
    void WriteDecoded(int listIdx,EditorInput* field,uint32_t value) {
        if(field->flags&EditorInput_Hex)snprintf(field->buffer,field->bufsize,"0x%X",value);else snprintf(field->buffer,field->bufsize,"%u",value);
        unsavedChanges=true;m_lastTyped=ImGui::GetTime();
        field->errored=m_editor->ValidateField(windowType,field)==false;
        if(!field->errored)OnUpdate(listIdx,field);
    }
    void RenderInput(int listIdx,EditorInput* field) override {
        ChangedFieldTint tint(field);
        auto codec=m_codecs.find(field->name.str());
        if(codec==m_codecs.end() || field->errored){EditorForm::RenderInput(listIdx,field);return;}
        namespace C=RetailFieldCodecs;
        const auto current=uint32_t(RetailInputValue(field));
        const auto preview=codec->second==Codec::HitLevel?C::HitLevelText(current):codec->second==Codec::Vulnerability?C::VulnerabilityText(current):C::HitboxLocationText(current,false);
        float comboWidth=ImGui::GetContentRegionAvail().x*0.6f;
        ImGuiComboFlags comboFlags=0;
        if(codec->second==Codec::HitboxLocation){
            // Bone names: no arrow, and as wide as the text needs while leaving the raw value 4 hex digits.
            comboFlags=ImGuiComboFlags_NoArrowButton;
            const float fit=ImGui::CalcTextSize(preview.c_str()).x+ImGui::GetStyle().FramePadding.x*2;
            comboWidth=(std::max)(comboWidth,(std::min)(fit,ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize("0x000").x-ImGui::GetStyle().FramePadding.x*2-ImGui::GetStyle().ItemSpacing.x));
        }
        ImGui::PushID(field);ImGui::SetNextItemWidth(comboWidth);
        const ImGuiID comboId=ImGui::GetID("##codec");
        if(ImGui::BeginCombo("##codec",preview.c_str(),comboFlags)){
            if(codec->second==Codec::HitLevel){
                for(const auto& choice:C::HitLevels){
                    const auto label=std::format("{} (0x{:X}){}",choice.label,choice.value,choice.confirmed?"":" ?");
                    if(ImGui::Selectable(label.c_str(),(current&C::HitLevelMask)==choice.value))WriteDecoded(listIdx,field,(current&~C::HitLevelMask)|choice.value);
                }
            }else if(codec->second==Codec::Vulnerability){
                for(const auto& choice:C::Vulnerabilities){
                    const auto label=std::format("{} (0x{:X})",choice.label,choice.value);
                    if(ImGui::Selectable(label.c_str(),(current&0xFFFF)==choice.value))WriteDecoded(listIdx,field,(current&0xFFFF0000u)|choice.value);
                }
            }else{
                if(ImGui::Selectable("None (0x0)",current==0))WriteDecoded(listIdx,field,0);
                for(const auto value:RetailLimbs::CommonLocations){
                    const auto label=std::format("{} (0x{:X})",C::HitboxLocationText(value),value);
                    if(ImGui::Selectable(label.c_str(),current==value))WriteDecoded(listIdx,field,value);
                }
            }
            ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)){
            if(codec->second==Codec::HitboxLocation)ImGui::SetTooltip("%s\n\n%s",C::HitboxLocationText(current).c_str(),_("edition.t8.codec.hitbox_location"));
            else ImGui::SetTooltip("%s",_(codec->second==Codec::HitLevel?"edition.t8.codec.hit_level":"edition.t8.codec.vulnerability"));
        }
        // Name the combo for Dear ImGui Test Engine after its ItemAdd (BeginCombo records no label): "codec:<field>".
        if(ImGui::GetCurrentContext()->TestEngineHookItems)ImGuiTestEngineHook_ItemInfo(ImGui::GetCurrentContext(),comboId,("codec:"+field->name.str()).c_str(),0);
        ImGui::PopID();
        ImGui::SameLine();
        EditorForm::RenderInput(listIdx,field);
    }
    void OnUpdate(int,EditorInput*) override {m_lastTyped=ImGui::GetTime();}
    void OnApply() override {MarkFieldsSaved(m_fieldIdentifierMap);}
    void PostRender() override {
        // A failing Apply leaves unsavedChanges set: do not retry it every frame (each attempt clones the forms and
        // copies the document), only once the form has been edited again.
        if(Owner().LiveEditing() && unsavedChanges && IsFormValid() && !ImGui::IsAnyItemActive() && ImGui::GetTime()-m_lastTyped>0.25 && m_lastTyped!=m_failedTyped){
            TryApply();if(unsavedChanges)m_failedTyped=m_lastTyped;
        }
    }
    void OpenDuplicatedForm(uint32_t id,int) override {Owner().OpenRecord(m_table,id);}
    void DeleteStructure() override {
        Owner().CheckStructuralEdit(this);
        RecordEdit edit(structureId,{}, {int(structureId)},m_retail->Count(m_table));
        EditForm([&]{m_retail->DeleteStructures(windowType,structureId,1);Owner().StructureChanged(this,m_table,edit);popen=false;unsavedChanges=false;justAppliedChanges=true;return true;});
    }
    void PasteText(std::string_view text) override {
        EditorForm::PasteText(text);
        for(auto& [name,field]:m_fieldIdentifierMap)if(field->imguiInputFlags&ImGuiInputTextFlags_ReadOnly)field->nextValue.reset();
        m_failedTyped=-1; // a paste is a new edit: live editing may apply again
    }
    void RenderExtraContextMenuItems() override {
        if(ImGui::Selectable(_("edition.t8.find_references")))static_cast<EditorVisualsT8Retail*>(m_baseWindow)->FindReferences(m_table,structureId);
    }
    void Apply() override {
        EditForm([&] {
            FlushPendingFields(); if (!IsFormValid()) return false;
            m_retail->SaveRecord(m_table,structureId,m_fieldIdentifierMap);
            if(m_table==12){
                if(!m_pendingPanm.empty()){
                    m_retail->ImportPanm(structureId,m_pendingPanm,m_pendingName);
                    auto record=m_retail->Record(12,structureId);uint64_t value,key;uint32_t length;memcpy(&value,record.data()+0x20,8);memcpy(&key,record.data()+0x28,8);memcpy(&length,record.data()+0x120,4);
                    const auto animationName=Owner().NameForHash(*EditorT8Retail::Decode(value,key));
                    snprintf(m_names[1].data(),m_names[1].size(),"%s",animationName.c_str());
                    // The new animation is a choice of this moveset from now on.
                    if(std::none_of(m_animationChoices.begin(),m_animationChoices.end(),[&](const auto& item){return item.first==animationName;})){
                        m_animationChoices.emplace_back(animationName,structureId);std::sort(m_animationChoices.begin(),m_animationChoices.end());}
                    if(auto found=m_fieldIdentifierMap.find("animation_length");found!=m_fieldIdentifierMap.end())snprintf(found->second->buffer,found->second->bufsize,"%u",length);
                }
                else if(m_names[1].data()!=m_savedNames[1])m_retail->SetAnimation(structureId,Owner().ResolveAnimation(m_names[1].data()));
                if(m_names[0].data()!=m_savedNames[0])m_retail->RenameMove(structureId,m_names[0].data());
                auto record=m_retail->Record(12,structureId);
                for(int i=0;i<2;++i){uint64_t value,key;memcpy(&value,record.data()+i*0x20,8);memcpy(&key,record.data()+i*0x20+8,8);
                    auto field=m_fieldIdentifierMap.at(i?"animation_hash":"name_hash");snprintf(field->buffer,field->bufsize,"%u",*EditorT8Retail::Decode(value,key));m_savedNames[i]=m_names[i].data();}
                m_windowTitle=Owner().RecordLabel(m_table,structureId)+" - moves"+Owner().RecordWindowId(m_table,structureId);
            }
            unsavedChanges=false; justAppliedChanges=true; m_requestedClosure=false;
            m_pendingPanm.clear(); m_pendingName.clear();
            OnApply(); return true;
        });
    }
    void OnFieldLabelClick(int,EditorInput* input) override {
        if (!(input->flags & EditorInput_Clickable)) return;
        if(input->name.str()=="transition" || input->name.str()=="move_id" || input->name.str().ends_with("_moveid")) {
            try {auto id=RetailInputValue(input);if(id<=UINT32_MAX)Owner().OpenRecord(12,m_retail->ResolveMove(uint32_t(id)));
            }catch(const std::exception&){}return;
        }
        for (const auto& field:EditorT8Retail::Schema(m_table)) if (field.name==input->name.str() && field.kind==EditorT8Retail::Kind::Reference) {
            int64_t index=-1; auto text=std::string_view(input->buffer); auto parsed=std::from_chars(text.data(),text.data()+text.size(),index);
            if (parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && index>=0 && uint64_t(index)<m_retail->Count(field.target))
                static_cast<EditorVisualsT8Retail*>(m_baseWindow)->OpenRecord(field.target,static_cast<uint32_t>(index));
        }
    }
    void RenderInternal() override {
        // Reserve the footer for Apply; long groups scroll independently. Wide move forms with the preview shown: the
        // fields on the left half and the preview on the right half, filling the height. Otherwise the preview is
        // stacked above the fields and scrolls with them. The icon next to the Animation button shows / hides it.
        const bool sideBySide=m_table==12 && m_preview.IsOpen() && AnimationPreview::SideBySide();
        const bool previewOpen=sideBySide;
        const float fieldsHeight=(std::max)(1.0f,ImGui::GetContentRegionAvail().y-30.0f);
        const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)/2;
        const float savedWidth=m_winInfo.size.x;
        ImGui::BeginChild("##RetailFields",ImVec2(previewOpen?half:0,fieldsHeight));
        if(previewOpen)m_winInfo.size.x=ImGui::GetWindowWidth();
        if (m_table==12 && ImGui::BeginTable("Names",4)) {
            ImGui::TableNextRow();
            for (int i=0;i<2;++i) {
                ImGui::TableNextColumn();
                const char* label=i ? _("edition.move.anim_name") : _("edition.move.move_name");
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(std::max)(0.0f,(ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(label).x)/2));
                if(i){
                    const float spacing=ImGui::GetStyle().ItemSpacing.x/2;
                    if(ImGui::Button(label,ImVec2((std::max)(1.0f,ImGui::GetContentRegionAvail().x-AnimationPreview::ToggleButtonWidth()-spacing),0)))m_openAnimationPicker=true;
                    ImGui::SameLine(0,spacing);m_preview.ToggleButton();
                }
                else ImGui::TextUnformatted(label);
                ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg,m_names[i].data()!=m_savedNames[i]?ImU32(FORM_INPUT_CHANGED):AppTheme::Input);
                ImGui::PushID(i); if(ImGui::InputText("##name",m_names[i].data(),m_names[i].size())){unsavedChanges=true;m_lastTyped=ImGui::GetTime();} ImGui::PopID();
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
        if(m_table==12 && !sideBySide)m_preview.Render(PreviewInput());
        if(m_openAnimationPicker){m_openAnimationPicker=false;RetailAnimationLibrary::Get().EnsureLoaded();ImGui::OpenPopup("Animation library");}
        // Modal: clicking outside must not dismiss it (a pending import would be lost). Closed by its X or Close button.
        bool animationPickerOpen=true;
        if(ImGui::BeginPopupModal("Animation library",&animationPickerOpen,ImGuiWindowFlags_AlwaysAutoResize)){
            if(ImGui::Button(_("close")))ImGui::CloseCurrentPopup();
            ImGui::SameLine();ImGui::SetNextItemWidth(360);ImGui::InputTextWithHint("##animationSearch",_("edition.t8.animation_search_hint"),m_animationSearch,sizeof(m_animationSearch));
            ImGui::SameLine();if(ImGui::Button(_("edition.t8.import_panm")))try{auto path=BrowsePanm();if(!path.empty() && ImportPanmBytes(EditorT8Retail::ReadAnimationFile(path),std::filesystem::path(path).filename().wstring()))ImGui::CloseCurrentPopup();}
            catch(const std::exception& e){m_operationError=e.what();}
            ImGui::SameLine();if(ImGui::Button(_("edition.t8.export_panm")))try{auto bytes=m_retail->MoveAnimation(structureId);auto path=BrowseSavePanm(SuggestedPanmName(m_savedNames[1]));
                if(!path.empty()){if(!FileIO::WriteAtomically(path,bytes))throw std::runtime_error(_("edition.t8.cannot_write_panm"));m_operationError.clear();ImGui::CloseCurrentPopup();}}
            catch(const std::exception& e){m_operationError=e.what();}
            if(!m_pendingPanm.empty()){
                ImGui::TextWrapped("%s",_("edition.t8.pending_panm"));
                if(ImGui::SmallButton(_("edition.t8.cancel_custom_panm"))){m_pendingPanm.clear();m_pendingName.clear();snprintf(m_names[1].data(),m_names[1].size(),"%s",m_savedNames[1].c_str());}
            }
            const auto query=EditorVisualsT8Retail::NormalizeSearch(m_animationSearch);
            if(ImGui::BeginTabBar("##animationTabs")){
                if(ImGui::BeginTabItem(_("edition.t8.animation_library.this_moveset"))){
                    ImGui::BeginChild("##animationChoices",ImVec2(640,360));
                    for(const auto& [name,row]:m_animationChoices){if(!query.empty() && EditorVisualsT8Retail::NormalizeSearch(name+" "+std::to_string(row)).find(query)==std::string::npos)continue;
                        ImGui::PushID(int(row));if(ImGui::Selectable(name.c_str())){m_pendingPanm.clear();m_pendingName.clear();snprintf(m_names[1].data(),m_names[1].size(),"%s",name.c_str());ImGui::CloseCurrentPopup();unsavedChanges=true;m_lastTyped=ImGui::GetTime();}ImGui::PopID();}
                    ImGui::EndChild();ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(_("edition.t8.animation_library.library"))){
                    ImGui::BeginChild("##animationLibrary",ImVec2(640,360));RenderLibraryTab(query);ImGui::EndChild();ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndPopup();
        }
        EditorForm::RenderInternal();
        ImGui::EndChild();
        m_winInfo.size.x=savedWidth;
        if(previewOpen){
            ImGui::SameLine();
            ImGui::BeginChild("##RetailPreview",ImVec2(0,fieldsHeight),0,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            m_preview.Body(PreviewInput(),ImGui::GetContentRegionAvail().y);
            ImGui::EndChild();
        }
        m_ignoreRightClick=m_table==12 && m_preview.IsOpen() && m_preview.ViewportHovered();
    }
public:
    std::unique_ptr<TransactionState> BeginTransaction(size_t& budget) override {
        auto base=EditorForm::BeginTransaction(budget);
        struct State final:TransactionState {
            RetailRecordForm& form;std::unique_ptr<TransactionState> base;
            decltype(m_names) names;decltype(m_savedNames) saved;bool committed=false;
            State(RetailRecordForm& f,std::unique_ptr<TransactionState> b):form(f),base(std::move(b)),names(f.m_names),saved(f.m_savedNames){}
            ~State(){if(!committed){form.m_names.swap(names);form.m_savedNames.swap(saved);}}
            void Commit() noexcept override {base->Commit();committed=true;}
        };
        return std::make_unique<State>(*this,std::move(base));
    }
    void ApplyShortcut() { TryApply(); }
    RetailRecordForm(size_t table,uint32_t row,EditorT8Retail& editor,EditorVisualsT8Retail& owner):m_table(table),m_retail(&editor) {
        structureId=row; windowType=EditorT8Retail::Type(table); m_editor=&editor; m_baseWindow=&owner;
        m_windowTypeName=R::Tables[table].name; m_windowTitleBase="Tekken 8";
        m_isDeletable=editor.CanDeleteRecord(table,row); m_canDuplicate=true;
        VectorSet<std::string> order; m_fieldIdentifierMap=editor.RecordFields(table,row,order);
        EditorFormUtils::ValidateFields(m_fieldIdentifierMap,order);
        for (const auto& name:order) {
            auto* field=m_fieldIdentifierMap.at(name);
            auto label=name;
            std::replace(label.begin(),label.end(),'_',' ');
            label[0]=static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
            if (name.starts_with("hitbox_")) label=label.substr(9);
            label[0]=static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
            EditorFormUtils::SetFieldDisplayText(field,label);
            field->hoverDescription=RetailFieldDescription(R::Tables[table].name,name);
            if(table==12 && name=="hit_level")m_codecs[name]=Codec::HitLevel;
            if(table==12 && name=="vulnerability")m_codecs[name]=Codec::Vulnerability;
            if(table==12 && name.starts_with("hitbox_") && name.ends_with("_location"))m_codecs[name]=Codec::HitboxLocation;
            if(name=="transition" || name=="move_id" || name.ends_with("_moveid")) {
                field->flags|=EditorInput_Clickable;field->color=FORM_INPUT_REF;
                try {uint32_t id=0;auto text=std::string_view(field->buffer);auto parsed=std::from_chars(text.data(),text.data()+text.size(),id);
                    if(parsed.ec==std::errc{})EditorFormUtils::SetFieldDisplayText(field,label+" : "+owner.RecordLabel(12,editor.ResolveMove(id)));
                }catch(const std::exception&){}
            }
            if (table==12 && (name=="name_hash" || name=="animation_hash")) {
                uint32_t hash=0; auto text=std::string_view(field->buffer);
                std::from_chars(text.data(),text.data()+text.size(),hash);
                auto resolved=owner.NameForHash(hash); auto& destination=m_names[name=="animation_hash"];
                snprintf(destination.data(),destination.size(),"%s",resolved.c_str());m_savedNames[name=="animation_hash"]=resolved; field->visible=false;
            }
            m_categories.insert(field->category);
            m_fieldsCategoryMap[field->category].push_back(field);
            m_categoryStringIdentifiers[field->category]=field->category==2 ? "edition.move.category_2" : field->category==3 ? "edition.move.category_3" : "edition.move.category_5";
            if (field->category>=7) m_categoryStringIdentifiers[field->category]="Hitbox "+std::to_string((field->category-7)/2+1);
        }
        if(table==12){std::set<uint32_t> seen;for(uint32_t id=0;id<editor.Count(12);++id){auto record=editor.Record(12,id);uint64_t value,key;memcpy(&value,record.data()+0x20,8);memcpy(&key,record.data()+0x28,8);auto hash=EditorT8Retail::Decode(value,key);if(hash && seen.insert(*hash).second)m_animationChoices.emplace_back(owner.NameForHash(*hash),id);}std::sort(m_animationChoices.begin(),m_animationChoices.end());}
        m_windowTitle=owner.RecordLabel(table,row)+" - "+R::Tables[table].name+owner.RecordWindowId(table,row);
        MarkFieldsSaved(m_fieldIdentifierMap);
    }
};
class RetailListForm final:public EditorFormList {
    size_t m_table;
    EditorT8Retail& Retail(){return *static_cast<EditorT8Retail*>(m_editor);}
    EditorVisualsT8Retail& Owner(){return *static_cast<EditorVisualsT8Retail*>(m_baseWindow);}
    void OpenDuplicatedForm(uint32_t id,int) override {Owner().OpenRecord(m_table,id);}
    void RenderExtraContextMenuItems() override {if(ImGui::Selectable(_("edition.t8.find_references")))Owner().FindReferences(m_table,structureId);}
    void OnFieldLabelClick(int index,EditorInput* input) override {
        if(!(input->flags&EditorInput_Clickable))return;
        try{if(input->buffer[0]=='-')return;auto value=RetailInputValue(input);
            auto& fields=m_items[index]->identifierMap;
            if((m_table==6 || m_table==7) && input->name.str()=="command"){Owner().OpenRecord(14,RetailT8Commands::InputSequenceId(value));return;}
            if((m_table==9 || m_table==10 || m_table==11) && input->name.str()=="parameter_1"){auto property=RetailInputValue(fields.at("property"));Owner().OpenRecord(property==33403?3:18,uint32_t(value));return;}
            for(const auto& field:EditorT8Retail::Schema(m_table))if(field.name==input->name.str()){
                if(field.kind==EditorT8Retail::Kind::Reference)Owner().OpenRecord(field.target,uint32_t(value));
                else if(field.name=="move_id"){auto command=RetailInputValue(fields.at("command"));if(command==RetailT8Commands::GroupCancel)Owner().OpenRecord(7,uint32_t(value));else Owner().OpenRecord(12,Retail().ResolveMove(uint32_t(value)));}
            }
        }catch(const std::exception& error){m_operationError=error.what();}
    }
    void Apply() override {
        std::vector<int> ids;bool structural=m_listSizeChange!=0;
        for(size_t i=0;i<m_items.size();++i){ids.push_back(m_items[i]->id);structural|=ids.back()!=int(structureId+i);}
        if(structural)Owner().CheckStructuralEdit(this);
        RecordEdit edit(structureId,ids,m_deletedItemIds,Retail().Count(m_table));
        EditorFormList::Apply();
        if(structural && !unsavedChanges)Owner().StructureChanged(this,m_table,edit);
    }
    void OnApplyResize(int,int) override {} // Full typed refresh includes reorder, not only size changes.
    void DeleteStructure() override {
        Owner().CheckStructuralEdit(this);auto count=GetOriginalStructureListSize();std::set<int> deleted;for(int i=0;i<count;++i)deleted.insert(structureId+i);
        RecordEdit edit(structureId,{},deleted,Retail().Count(m_table));
        EditorFormList::DeleteStructure();Owner().StructureChanged(this,m_table,edit);
    }
    void BuildItemDetails(int index) override {
        namespace C=RetailT8Commands;
        auto& item=*m_items[index];item.itemLabel=std::to_string(structureId+index);item.color=0;
        auto& fields=item.identifierMap;
        if(m_table==6 || m_table==7){
            auto* command=fields.at("command");auto value=RetailInputValue(command);
            auto* target=fields.at("move_id");auto targetId=uint32_t(RetailInputValue(target));
            command->flags&=~EditorInput_Clickable;
            const std::string invalid=std::string(" [")+_("edition.form_list.invalid")+"]";
            EditorFormUtils::SetFieldDisplayText(command,_("edition.cancel.command"));
            EditorFormUtils::SetFieldDisplayText(target,value==C::GroupCancel?_("edition.cancel.group_id"):_("edition.cancel.move_id"));
            if(value==C::GroupCancel){
                item.color=MOVEID_GROUP_CANCEL;
                item.itemLabel+="  "+std::string(_("edition.grouped_cancel.window_name"))+": "+std::to_string(targetId);
                if(targetId>=Retail().Count(7))item.itemLabel+=invalid;
                return;
            }
            if(value==C::GroupEnd){item.itemLabel+="  [END]";return;}
            std::string move;
            try{auto resolved=Retail().ResolveMove(targetId);move=Owner().RecordLabel(12,resolved);if(resolved!=targetId)move=std::to_string(targetId)+" / "+move;}
            catch(const std::exception&){move=std::to_string(targetId)+" / "+_("edition.form_list.invalid");}
            item.itemLabel+="  "+move;
            if(C::IsInputSequence(value)){
                command->flags|=EditorInput_Clickable;command->color=FORM_INPUT_REF;item.color=MOVEID_INPUT_SEQUENCE;
                EditorFormUtils::SetFieldDisplayText(command,_("edition.cancel.sequence_id"));
                auto sequence=C::InputSequenceId(value);item.itemLabel+=" / "+std::string(_("edition.input_sequence.window_name"))+": "+std::to_string(sequence);
                if(sequence<Retail().Count(14))item.itemLabel+=" [ "+Retail().InputSequenceText(sequence)+" ]";
                else item.itemLabel+=invalid;
            }else{
                auto text=C::CommandString(value);
                item.itemLabel+=" / [ "+(text.empty()?std::format("0x{:X}",value):text)+" ]";
            }
            return;
        }
        if(m_table==15){
            // EditorVisuals_t7 TEditorInputStruct: the decoded direction and buttons.
            item.itemLabel+="  "+C::CommandString(uint32_t(RetailInputValue(fields.at("direction"))),uint32_t(RetailInputValue(fields.at("buttons"))));
            return;
        }
        if(m_table==5){
            // EditorVisuals_t7 TEditorPushbackExtra.
            item.itemLabel+=std::format("  {} = {}",_("edition.pushback_extradata.horizontal_offset"),fields.at("value")->buffer);
            return;
        }
        if(m_table==13){
            auto first=RetailInputValue(fields.at("parameter_1"));
            item.itemLabel+="  "+std::string(fields.at("parameter_1")->buffer);
            if(first==UINT32_MAX)item.itemLabel+="  [END]";
            else item.itemLabel+=std::format(" / [{}, {}]",fields.at("parameter_2")->buffer,fields.at("parameter_3")->buffer);
            return;
        }
        if(m_table==9 || m_table==10 || m_table==11){auto* parameter=fields.at("parameter_1");auto property=RetailInputValue(fields.at("property"));parameter->flags&=~EditorInput_Clickable;
            if(property==33403 || property==34447){parameter->flags|=EditorInput_Clickable;parameter->color=FORM_INPUT_REF;}}
        if(m_table==1 || m_table==9 || m_table==10 || m_table==11){
            // EditorVisuals_t7 TEditorRequirements / TEditorExtraproperties: "[frame: ]label = value" with the
            // first parameter in all interpretations when it is not a small integer; the others follow raw.
            const bool requirement=m_table==1;
            const auto id=uint32_t(RetailInputValue(fields.at(requirement?"condition":"property")));
            const char* label=Owner().labels?Owner().labels->GetText(id):nullptr;
            RelabelParameters(fields,id,requirement);
            item.itemLabel+="  ";
            if(m_table==9)item.itemLabel+=RetailPropertyFrameText(uint32_t(RetailInputValue(fields.at("frame"))))+": ";
            item.itemLabel+=label?std::string(label):std::format("{} / 0x{:x}",id,id);
            item.itemLabel+=" = "+RetailValueText(uint32_t(RetailInputValue(fields.at("parameter_1"))));
            if(requirement){const auto meaning=RequirementParamMeaning(id,uint32_t(RetailInputValue(fields.at("parameter_1"))));if(!meaning.empty())item.itemLabel+=" ("+meaning+")";}
            std::string rest;
            for(int parameter=2;parameter<=5;++parameter){auto name="parameter_"+std::to_string(parameter);if(fields.contains(name)){if(!rest.empty())rest+=", ";rest+=fields.at(name)->buffer;}}
            if(!rest.empty())item.itemLabel+=" / ["+rest+"]";
            return;
        }
        for(const char* key:{"damage","value"})if(fields.contains(key)){item.itemLabel+="  "+std::string(fields.at(key)->buffer);break;}
    }
    // Searchable id picker for the condition (table 1) and property (tables 9-11) fields: every label known to the
    // editor (EditorLabel) plus the handler names of the parameter review (RetailParamLabels), filtered as you type.
    struct IdChoice {uint32_t id;std::string text,search;};
    std::vector<IdChoice> m_idChoices;bool m_idChoicesBuilt=false;char m_idFilter[64]{};
    bool IdPickerField(const EditorInput* field) const {
        return (m_table==1 && field->name.str()=="condition") || ((m_table==9 || m_table==10 || m_table==11) && field->name.str()=="property");
    }
    void BuildIdChoices() {
        m_idChoicesBuilt=true;m_idChoices.clear();
        const bool requirements=m_table==1 || m_table==10 || m_table==11, properties=m_table!=1;
        std::map<uint32_t,std::string> labels;
        if(Owner().labels)for(const auto& [id,text]:Owner().labels->Labels())if(id>=0 && ((requirements && id<0x8000) || (properties && id>=0x8000)))labels[uint32_t(id)]=text;
        auto add=[&](uint32_t id,const RetailParamInfo* info){
            std::string label=labels.count(id)?labels[id]:info?info->name:std::string();
            if(label.empty())return;
            IdChoice choice{id,std::format("{}  {}",id,label),EditorVisualsT8Retail::NormalizeSearch(std::format("{} 0x{:x} {} {}",id,id,label,info?info->name:""))};
            m_idChoices.push_back(std::move(choice));
        };
        std::set<uint32_t> ids;for(const auto& [id,text]:labels)ids.insert(id);
        for(uint32_t id:ids)add(id,(id<0x8000?RetailParamLabels::Get().Requirement(id):RetailParamLabels::Get().Property(id)));
        // Reviewed ids without a community label still appear, named after their handler.
        for(uint32_t id=0;id<1200 && requirements;++id)if(!ids.count(id))if(const auto* info=RetailParamLabels::Get().Requirement(id))add(id,info);
        for(uint32_t id=0x8000;id<0x8800 && properties;++id)if(!ids.count(id))if(const auto* info=RetailParamLabels::Get().Property(id))add(id,info);
        std::sort(m_idChoices.begin(),m_idChoices.end(),[](const IdChoice& a,const IdChoice& b){return a.id<b.id;});
    }
    void RenderIdPicker(int listIdx,EditorInput* field) {
        if(!m_idChoicesBuilt)BuildIdChoices();
        const auto current=uint32_t(RetailInputValue(field));
        const char* label=Owner().labels?Owner().labels->GetText(int(current)):nullptr;
        const auto* info=current<0x8000?RetailParamLabels::Get().Requirement(current):RetailParamLabels::Get().Property(current);
        std::string preview=label?label:info?info->name:std::format("0x{:X}",current);
        ImGui::PushID(field);ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*0.6f);
        const ImGuiID comboId=ImGui::GetID("##idpicker");
        if(ImGui::BeginCombo("##idpicker",preview.c_str(),ImGuiComboFlags_HeightLargest)){
            if(ImGui::IsWindowAppearing()){ImGui::SetKeyboardFocusHere();m_idFilter[0]=0;}
            ImGui::SetNextItemWidth(460);ImGui::InputTextWithHint("##filter",_("edition.t8.id_picker_hint"),m_idFilter,sizeof(m_idFilter));
            const auto query=EditorVisualsT8Retail::NormalizeSearch(m_idFilter);
            std::vector<const IdChoice*> shown;for(const auto& choice:m_idChoices)if(query.empty() || choice.search.find(query)!=std::string::npos)shown.push_back(&choice);
            ImGui::BeginChild("##idchoices",ImVec2(460,300));
            ImGuiListClipper clip;clip.Begin(int(shown.size()));
            while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){
                const auto& choice=*shown[i];ImGui::PushID(int(choice.id));
                if(ImGui::Selectable(choice.text.c_str(),choice.id==current)){
                    snprintf(field->buffer,field->bufsize,"%u",choice.id);unsavedChanges=true;
                    field->errored=m_editor->ValidateField(windowType,field)==false;if(!field->errored)OnUpdate(listIdx,field);
                    ImGui::CloseCurrentPopup();
                }
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)){
                    const auto* hovered=choice.id<0x8000?RetailParamLabels::Get().Requirement(choice.id):RetailParamLabels::Get().Property(choice.id);
                    if(hovered)ImGui::SetTooltip("%s",Fmt("edition.t8.param_hover",hovered->name,hovered->confidence,hovered->meaning,hovered->param.empty()?std::string("-"):hovered->param,hovered->top.empty()?std::string("-"):hovered->top).c_str());
                }
                ImGui::PopID();
            }
            ImGui::EndChild();ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))ImGui::SetTooltip("%s",_("edition.t8.codec.id_picker"));
        if(ImGui::GetCurrentContext()->TestEngineHookItems)ImGuiTestEngineHook_ItemInfo(ImGui::GetCurrentContext(),comboId,("codec:"+field->name.str()).c_str(),0);
        ImGui::PopID();ImGui::SameLine();
    }
    std::string RequirementParamMeaning(uint32_t id,uint32_t param) {
        return RetailFieldCodecs::RequirementParamText(id,param,[this](uint32_t character){return Owner().CharacterName(character);});
    }
    // Cancel option: derived by the game at load (RetailFieldCodecs::CancelOptionText); shown decoded before the raw value.
    // A requirement's first parameter shows its decoded meaning the same way (RetailFieldCodecs::RequirementParamText).
    void RenderInput(int listIdx,EditorInput* field) override {
        // Rows added since the last Apply (inserted or pasted, id -1) count as changed as a whole.
        ChangedFieldTint tint(field,listIdx>=0 && size_t(listIdx)<m_items.size() && m_items[listIdx]->id<0);
        if(IdPickerField(field) && !field->errored)RenderIdPicker(listIdx,field);
        if((m_table==6 || m_table==7) && field->name.str()=="option" && !field->errored){
            const auto text=RetailFieldCodecs::CancelOptionText(uint32_t(RetailInputValue(field)));
            ImGui::PushID(field);ImGui::TextDisabled("%s",text.c_str());
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))ImGui::SetTooltip("%s",_("edition.t8.codec.cancel_option"));
            ImGui::PopID();
        }
        if(m_table==1 && field->name.str()=="parameter_1" && !field->errored && listIdx>=0 && size_t(listIdx)<m_items.size()){
            auto& row=m_items[listIdx]->identifierMap;
            const auto text=RequirementParamMeaning(uint32_t(RetailInputValue(row.at("condition"))),uint32_t(RetailInputValue(field)));
            if(!text.empty()){
                ImGui::PushID(field);ImGui::TextDisabled("%s",text.c_str());
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))ImGui::SetTooltip("%s",_("edition.t8.codec.requirement_param"));
                ImGui::PopID();
            }
        }
        EditorFormList::RenderInput(listIdx,field);
    }
    // Issue #122: the parameter labels follow the selected condition / property id (RetailParamLabels).
    void RelabelParameters(InputMap& fields,uint32_t id,bool requirementTable) {
        const auto* info=(requirementTable || id<0x8000)?RetailParamLabels::Get().Requirement(id):RetailParamLabels::Get().Property(id);
        auto* first=fields.at("parameter_1");
        const std::string base=_("edition.t8.parameter_1");
        if(!info){EditorFormUtils::SetFieldDisplayText(first,base);first->hoverDescription=RetailFieldDescription(R::Tables[m_table].name,"parameter_1");}
        else {
            const auto lower=EditorVisualsT8Retail::NormalizeSearch(info->param);
            const bool unused=info->param.empty() || lower.starts_with("unused") || lower.starts_with("ignored");
            std::string text=unused?base+" ("+_("edition.t8.param_unused")+")":base+": "+info->param;
            if(text.size()>44)text=text.substr(0,43)+"...";
            EditorFormUtils::SetFieldDisplayText(first,text);
            first->hoverDescription=Fmt("edition.t8.param_hover",info->name,info->confidence,info->meaning,info->param.empty()?std::string("-"):info->param,info->top.empty()?std::string("-"):info->top);
        }
        for(int n=2;n<=5;++n){
            const auto key="parameter_"+std::to_string(n);if(!fields.contains(key))continue;auto* field=fields.at(key);
            field->hoverDescription=info?Fmt("edition.t8.param_hover_other",info->name,info->meaning):RetailFieldDescription(R::Tables[m_table].name,key);
        }
    }
    // EditorT8Retail::ValidateCancelRow: the target must match the command type. Terminators are left alone.
    void ValidateCancelRow(int index){
        auto& fields=m_items[index]->identifierMap;auto* command=fields.at("command");auto* target=fields.at("move_id");
        if(command->errored)return;
        const auto errors=Retail().ValidateCancelRow(RetailInputValue(command),RetailInputValue(target));
        command->errored=errors.command;target->errored=errors.target;
    }
    void OnUpdate(int index,EditorInput* field) override {
        if((m_table==6 || m_table==7) && (field->name.str()=="command" || field->name.str()=="move_id"))ValidateCancelRow(index);
        BuildItemDetails(index);
    }
    // Fingerprint of the list's edit state (row ids and field texts). A failing automatic Apply leaves unsavedChanges
    // set; it is retried only once this changes, not every frame.
    size_t m_failedState=0;bool m_applyFailed=false;
    size_t EditState() const {
        size_t hash=m_items.size();
        for(const auto& item:m_items){hash=hash*31+size_t(item->id);for(const auto& [name,field]:item->identifierMap)hash=hash*31+std::hash<std::string_view>{}(field->buffer);}
        return hash;
    }
    void PostRender() override {
        if(!Owner().LiveEditing() || !unsavedChanges || !IsFormValid() || ImGui::IsAnyItemActive())return;
        const auto state=EditState();
        if(m_applyFailed && state==m_failedState)return;
        TryApply();m_applyFailed=unsavedChanges;m_failedState=state;
    }
    void OnApply() override {for(const auto& item:m_items)MarkFieldsSaved(item->identifierMap);}
public:
    void ApplyShortcut(){TryApply();}
    RetailListForm(size_t table,uint32_t row,EditorT8Retail& editor,EditorVisualsT8Retail& owner):
        EditorFormList("Tekken 8",EditorT8Retail::Type(table),row,&editor,&owner,int(editor.ListSize(table,row)),R::Tables[table].name),m_table(table){
        m_isDeletable=true;m_canDuplicate=true;
        m_windowTitle=owner.RecordLabel(table,row)+" - "+R::Tables[table].name+owner.RecordWindowId(table,row);
        m_categoryStringIdentifiers[2]="edition.move.category_2";m_categoryStringIdentifiers[5]="edition.move.category_5";
        for(auto& item:m_items)for(auto& [name,field]:item->identifierMap){
            auto label=name.str();std::replace(label.begin(),label.end(),'_',' ');label[0]=char(std::toupper((unsigned char)label[0]));EditorFormUtils::SetFieldDisplayText(field,label);
            field->hoverDescription=RetailFieldDescription(R::Tables[table].name,name.str());
            if(name.str()=="move_id"){field->flags|=EditorInput_Clickable;field->color=FORM_INPUT_REF;}
        }
        OnInitEnd();
        for(const auto& item:m_items)MarkFieldsSaved(item->identifierMap);
    }
};

}
std::string EditorVisualsT8Retail::NameForHash(uint32_t hash) const {
    if(m_editor)if(auto* custom=m_editor->CustomName(hash))return *custom;
    if(m_editor)if(auto* named=m_editor->FileName(hash))return *named;
    if (auto* name=m_names.FindName(hash)) return *name;
    return std::format("0x{:08X}",hash);
}
void EditorVisualsT8Retail::LoadNames() {
    m_names=GameStaticData::LoadOptional("t8-retail");
    m_character=m_names.CharacterName(m_editor->Character());
    if (!m_names.Error().empty()) m_saveMessage=std::string(_("edition.t8.names_unavailable"))+" "+m_names.Error();
}
EditorVisualsT8Retail::EditorVisualsT8Retail(const movesetInfo* info,GameAddressesFile* addresses,LocalStorage*) {
    // Loading a document is offline. Current/Import explicitly connect preview.
    m_liveEditable=true; m_liveEdition=false; m_loadedMoveset=0;
    m_loadedCharacter.filename=info->filename; filename=m_loadedCharacter.filename.c_str();
    m_loadedCharacter.name=info->name; m_loadedCharacter.gameId=GameId_T8;
    m_loadedCharacter.lastSavedDate=Helpers::formatDateTime(info->date);
    m_subwindowsTitle="Tekken 8"; m_windowTitle="Tekken 8 - "+info->name+"###retail_document_"+std::to_string(reinterpret_cast<uintptr_t>(this));
    m_lastAutoSave=m_lastChangeDate=Helpers::getCurrentTimestamp();
    m_compressionIndex=CompressionUtils::GetCompressionSettingIndex(TKMovesetCompressionType_CompactZstd);
    try {
        auto profile=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);
        auto editor=std::make_unique<EditorT8Retail>(m_offlineProcess,m_offlineGame,profile);
        MovesetFile file(info->filename); file.Validate();
        auto bytes=static_cast<Byte*>(malloc(file.Bytes().size())); if (!bytes) throw std::bad_alloc();
        memcpy(bytes,file.Bytes().data(),file.Bytes().size());
        if (!editor->LoadMoveset(bytes,file.Bytes().size())) throw std::runtime_error("Cannot open this retail T8 moveset");
        m_editor=editor.get(); m_abstractEditor=std::move(editor);
        labels=std::make_unique<EditorLabel>(profile,addresses); LoadNames();
        for(uint32_t row=0;row<m_editor->Count(12);++row) {auto record=m_editor->Record(12,row);m_originalMoveCrc.push_back(m_editor->MoveFingerprint(row));}
    } catch (const std::exception& error) { m_loadError=error.what(); }
}
std::string EditorVisualsT8Retail::RecordLabel(size_t table,uint32_t row) const {
    std::string label=std::to_string(row);
    if (table==12) {
        auto hash=m_editor->MoveNameHash(row);
        if (hash) label+="  "+NameForHash(*hash);
    } else if (labels && (table==1 || table==9 || table==10 || table==11)) {
        auto bytes=m_editor->Record(table,row); uint32_t id=0;
        memcpy(&id,bytes.data()+(table==9?16:(table==10 || table==11)?8:0),4);
        label+="  "+std::to_string(id);
        if (auto* text=labels->GetText(id)) label+=" "+std::string(text);
    }
    return label;
}
bool EditorVisualsT8Retail::OpenRecord(size_t table,uint32_t row) {
    // An exception here would leave render code and end the program (losing unsaved edits): report it instead.
    // Example: rows after the last terminator of a list table (native Kuma start_properties 405) have no list bound.
    try {
    if (!m_editor || table>=RetailT8Snapshot::Tables.size() || row>=m_editor->Count(table)) throw std::out_of_range("Retail record outside table");
    // A name typed in the move search box stays (it filters the list); an id or an empty box shows the opened row.
    if(table==12){m_selectedMove=row;const std::string_view typed=m_moveId;if(typed.empty() || Helpers::is_string_digits(m_moveId) || typed.starts_with("0x") || typed.starts_with("0X"))snprintf(m_moveId,sizeof(m_moveId),"%u",row);}
    for (auto* form:m_structWindows) if (form->popen && form->windowType==EditorT8Retail::Type(table) && form->structureId==row) { form->setFocus=true; return true; }
    ConsumePendingChanges();
    EditorForm* replace=nullptr;
    if(!m_restoringForms && ImGui::GetCurrentContext() && !ImGui::GetIO().KeyCtrl)for(auto* form:m_structWindows)
        if(form->popen && !form->unsavedChanges && form->windowType==EditorT8Retail::Type(table) && form->currentViewport==ImGui::GetWindowViewport())replace=form;
    std::unique_ptr<EditorForm> form;
    if((table==1 || table==2 || table==6 || table==7 || table==9 || table==10 || table==11 || table==13 || table==5 || table==15))
        form=std::make_unique<RetailListForm>(table,row,*m_editor,*this);
    else form=std::make_unique<RetailRecordForm>(table,row,*m_editor,*this);
    form->nextDockId=m_dockId;
    form->setFocus=!m_restoringForms;
    PublishFormWindow(std::move(form));
    if(replace)replace->popen=false;
    return true;
    } catch (const std::exception& error) {
        m_saveMessage=std::format("{} {} {}: {}",_("edition.t8.open"),table<RetailT8Snapshot::Tables.size()?RetailT8Snapshot::Tables[table].name:"?",row,error.what());
        return false;
    }
}
void EditorVisualsT8Retail::RenderGameSpecific(int dockid) {
    if(m_editor){PollPreview();RefreshPreview();
        auto reopen=std::move(m_reopen);m_reopen.clear();const auto selected=m_selectedMove;m_restoringForms=true;
        for(auto [table,row]:reopen)if(row<m_editor->Count(table))try{OpenRecord(table,row);}catch(const std::exception& e){m_saveMessage=e.what();}
        m_restoringForms=false;m_selectedMove=selected;
    }
    if(m_labelQueryPending && !labels->ongoingQuery){m_labelQueryPending=false;m_indexRevision=UINT64_MAX;m_filterDirty=true;}
    ImGui::PushID(this);
    if (m_editor && m_loadError.empty() && m_backedUpRevision<m_editRevision && !m_saveJob.Busy() &&
        Helpers::getCurrentTimestamp()-m_lastAutoSave >= 60*EDITOR_BACKUP_FREQUENCY_MINUTES) SaveBackup();
    const auto pending=std::count_if(m_structWindows.begin(),m_structWindows.end(),[](auto* form) { return form->unsavedChanges; });
    const bool unapplied=pending!=0;
    if (m_savedLastChange) m_savedRevision=m_editRevision;
    if (!popen && !m_savedLastChange && !m_closeAfterSave) { popen=true; m_closePrompt=true; }
    ImGui::SetNextWindowSize(ImVec2(1280,720),ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints(ImVec2(200,400),ImVec2(9999,9999));
    if(setFocus){ImGui::SetNextWindowFocus();setFocus=false;}
    if (ImGui::Begin(m_windowTitle.c_str(),&popen, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar | (m_savedLastChange ? 0 : ImGuiWindowFlags_UnsavedDocument))) {
        if (!m_loadError.empty()) ImGuiExtra_TextboxError(m_loadError.c_str());
        else {
            m_editor->live_loadedMoveset=0;
            const auto& io=ImGui::GetIO();
            const bool active=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || std::any_of(m_structWindows.begin(),m_structWindows.end(),[](auto* f){return f->windowFocused;});
            if (active) {
                DetectKeyboardShortcuts();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F,false)) m_focusSearch=true;
                for (auto* form:m_structWindows) if (form->windowFocused) {
                    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter,false)){if(auto* record=dynamic_cast<RetailRecordForm*>(form))record->ApplyShortcut();else if(auto* list=dynamic_cast<RetailListForm*>(form))list->ApplyShortcut();}
                    if (io.KeyAlt && (ImGui::IsKeyPressed(ImGuiKey_UpArrow,false) || ImGui::IsKeyPressed(ImGuiKey_DownArrow,false))) {
                        auto table=EditorT8Retail::TableIndex(form->windowType);auto row=int64_t(form->structureId)+(ImGui::IsKeyPressed(ImGuiKey_UpArrow,false)?-1:1);
                        if (row>=0 && uint64_t(row)<m_editor->Count(table)) OpenRecord(table,static_cast<uint32_t>(row));
                        break;
                    }
                }
            }
            ConsumePendingChanges();
            m_viewport=ImGui::GetWindowViewport();
            if(ImGui::BeginMenuBar()) {
                ImGuiExtra::HelpMarker(_("edition.window_controls_explanation"));
                ImGui::Separator();
                if(ImGui::Button(_("edition.move_current_1p")))CurrentMove(0);
                if(ImGui::Button(_("edition.move_current_2p")))CurrentMove(1);
                ImGui::Separator();
                if(ImGui::BeginMenu(_("edition.create_new"))) {
                    for(size_t table:{1,12,13,6,7,8,9,10,11,2,0,4,5,3,14,15,18,17,16,19})
                        if(ImGui::MenuItem(R::Tables[table].name))CreateRecord(table);
                    ImGui::EndMenu();
                }
                // Open any record by table and row: type the row, then Enter or the open button.
                if(ImGui::BeginMenu(_("edition.goto"))) {
                    for(size_t table:{1,12,13,6,7,8,9,10,11,2,0,4,5,3,14,15,18,17,16,19})
                        if(ImGui::BeginMenu(R::Tables[table].name)) {
                            const int count=int(m_editor?m_editor->Count(table):0);
                            ImGui::TextDisabled("0 - %d",count-1);
                            ImGui::SetNextItemWidth(120);
                            if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
                            bool open=ImGui::InputInt("##goto",&m_gotoId,1,10,ImGuiInputTextFlags_EnterReturnsTrue);
                            if(m_gotoId<0)m_gotoId=0;
                            ImGui::SameLine();
                            open|=ImGui::Button(_("edition.goto_open"));
                            if(open && OpenRecord(table,uint32_t(m_gotoId)))ImGui::CloseCurrentPopup();
                            ImGui::EndMenu();
                        }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if(ImGui::MenuItem(_("edition.extract_animations")))OpenAnimationExtraction();
                ImGui::Separator();
                const char* labelStatus=_(labels->ongoingQuery?"edition.updating_labels":labels->errored?"edition.updating_labels_err":labels->updated?"edition.updated_labels":labels->startedThread?"edition.up_to_date_labels":"edition.update_labels");
                if(ImGui::MenuItem(labelStatus,nullptr,false,!labels->ongoingQuery)){labels->UpdateFromWebsite();m_labelQueryPending=true;}
                ImGui::Separator();
                if(ImGui::BeginMenu(_("edition.t8.table"))) {
                    for(size_t i=0;i<R::Tables.size();++i)if(ImGui::MenuItem(R::Tables[i].name,nullptr,m_table==i)){m_table=i;m_search[0]=0;m_filterDirty=true;}
                    ImGui::EndMenu();
                }
                const auto unsaved=m_editRevision-m_savedRevision;
                const auto status=Fmt("edition.t8.status_counters",pending,unsaved);
                const auto right=ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-ImGui::CalcTextSize(status.c_str()).x;
                if(right>ImGui::GetCursorPosX()){ImGui::SetCursorPosX(right);ImGui::TextUnformatted(status.c_str());}
                ImGui::EndMenuBar();
            }
            if(m_saveJob.Busy())ImGui::TextUnformatted(FileJobStageText(m_saveJob.Stage()));
            if(!m_saveMessage.empty())ImGui::TextUnformatted(m_saveMessage.c_str());
            if(m_savingError)ImGuiExtra_TextboxError(_("edition.t8.saving_failed"));
            ImVec2 size=ImGui::GetContentRegionAvail();
            size.y-=ImGui::GetFrameHeightWithSpacing()+(m_previewMessage.empty()?0:ImGui::GetTextLineHeightWithSpacing());
            if(ImGui::BeginTable("MovesetMainTable",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_Borders|ImGuiTableFlags_NoHostExtendY,size)) {
                ImGui::TableSetupColumn("##Records",ImGuiTableColumnFlags_WidthFixed,300.0f);
                ImGui::TableSetupColumn("##Editor");
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();ImGui::TableNextColumn();
                RenderBrowser();
                ImGui::TableNextColumn();
                m_dockId=AppTheme::DockSpace(dockid+2,ImVec2(0,(std::max)(1.0f,size.y-ImGui::GetFrameHeightWithSpacing())));
                ImGui::BeginDisabled(m_closeAfterSave);RenderSubwindows();ImGui::EndDisabled();
                ImGui::EndTable();
            }
            ConsumePendingChanges();
            const bool pendingNow=std::any_of(m_structWindows.begin(),m_structWindows.end(),[](auto* f){return f->unsavedChanges;});
            RenderPreviewControls(pendingNow);
        }
    }
    ImGui::End();
    if(m_editor)RenderReferences();
    if(m_editor)RenderAnimationExtraction();
    ConsumePendingChanges();
    if (!popen && (m_saveJob.Busy() || PreviewBusy() || ExtractionBusy())) { popen=true; }
    if (!popen && (unapplied || !m_savedLastChange)) { popen=true; m_closePrompt=true; }
    if (m_closePrompt) { ImGui::OpenPopup("###UnsavedRetailEdits"); m_closePrompt=false; }
    if (ImGuiExtra::BeginFittedModal("UnsavedRetailEdits",_("edition.t8.unsaved_title"),nullptr,30.0f)) {
        ImGui::TextUnformatted(_(unapplied ? "edition.t8.unsaved_unapplied" : "edition.t8.unsaved_question"));
        ImGui::BeginDisabled(unapplied || m_saveJob.Busy());
        if (ImGui::Button(_("edition.t8.save_and_close"))) { Save(); if (m_saveJob.Busy()) { m_closeAfterSave=true; ImGui::CloseCurrentPopup(); } }
        ImGui::EndDisabled(); ImGui::SameLine();
        ImGui::BeginDisabled(m_saveJob.Busy());
        if (ImGui::Button(_("edition.t8.discard_and_close"))) { popen=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndDisabled(); ImGui::SameLine();
        if (ImGui::Button(_("edition.t8.keep_editing"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

std::string EditorVisualsT8Retail::RecordWindowId(size_t table,uint32_t row) const {
    return std::format("###retail_{}_{}_{}",reinterpret_cast<uintptr_t>(this),table,row);
}
std::string EditorVisualsT8Retail::NormalizeSearch(std::string text) {
    for (char& c:text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::string EditorVisualsT8Retail::SearchMetadata(size_t table,uint32_t row) const {
    auto key=std::to_string(row);
    if (table==12) {
        if (auto hash=m_editor->MoveNameHash(row)) {
            key+=std::format(" 0x{:08X}",*hash);
            key+=" "+NameForHash(*hash);
        }
    } else if (table==1 || table==9 || table==10 || table==11) {
        auto bytes=m_editor->Record(table,row);uint32_t id=0;
        memcpy(&id,bytes.data()+(table==9?16:(table==10 || table==11)?8:0),4);
        key+=std::format(" {} 0x{:08X}",id,id);
        if (labels) if (auto* label=labels->GetText(id)) key+=" "+std::string(label);
    }
    return NormalizeSearch(std::move(key));
}


void EditorVisualsT8Retail::CheckStructuralEdit(const EditorForm* source) const {
    for(auto* form:m_structWindows)if(form!=source && form->popen && form->unsavedChanges)throw std::runtime_error(_("edition.t8.apply_other_forms"));
}
void EditorVisualsT8Retail::StructureChanged(EditorForm* source,size_t table,const RecordEdit& edit){
    std::vector<std::pair<size_t,uint32_t>> reopen;
    for(auto* form:m_structWindows)if(form!=source && form->popen){auto t=EditorT8Retail::TableIndex(form->windowType);auto row=uint64_t(form->structureId);if(t==table)row=edit.Remap(row);if(row<m_editor->Count(t))reopen.emplace_back(t,uint32_t(row));}
    m_reopen=std::move(reopen);for(auto* form:m_structWindows)if(form!=source)form->popen=false;
    if(table==12 && m_selectedMove>=0){auto row=edit.Remap(m_selectedMove);m_selectedMove=row<m_editor->Count(12)?int64_t(row):-1;}
    if(m_referencesOpen && table==m_referenceTable){auto row=edit.Remap(m_referenceRow);if(row<m_editor->Count(table))m_referenceRow=uint32_t(row);else m_referencesOpen=false;}
    m_filterDirty=true;m_indexRevision=UINT64_MAX;m_checkedRoots={};
}
void EditorVisualsT8Retail::OpenAnimationExtraction() {
    m_openExtraction=true;
    if(ExtractionBusy())return; // reopened to watch the running extraction
    try {
        // The document as it would be saved (unsaved edits included); the extraction later reads these same bytes.
        auto bytes=std::make_shared<const std::vector<Byte>>(m_editor->BuildSaveSnapshot());
        const auto* header=reinterpret_cast<const TKMovesetHeader*>(bytes->data());
        const auto list=RetailT8Animations::List(std::span<const Byte>(*bytes).subspan(header->moveset_data_start),[this](uint32_t hash) {
            if(auto* custom=m_editor->CustomName(hash))return *custom;
            if(auto* named=m_editor->FileName(hash))return *named;
            auto* known=m_names.FindName(hash);return known?*known:std::string();
        });
        if(list.empty())throw std::runtime_error(_("edition.t8.extract.none"));
        m_extractChoices.clear();
        for(const auto& animation:list)
            m_extractChoices.push_back({animation,NormalizeSearch(std::format("{} {} {} {:08X}",animation.name,animation.file,RetailT8Animations::Categories[animation.category],animation.instance))});
        m_extractSnapshot=std::move(bytes);m_extractFilterDirty=true;m_extractMessage.clear();m_extractFailed=false;
    }catch(const std::exception& e){m_openExtraction=false;m_previewMessage=e.what();}
}
void EditorVisualsT8Retail::RenderAnimationExtraction() {
    // Finished job: reported here even when the modal was closed meanwhile.
    if(ExtractionBusy() && m_extractJob.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{m_extractMessage=m_extractJob.get();m_extractFailed=false;}catch(const std::exception& e){m_extractMessage=e.what();m_extractFailed=true;}
        m_previewMessage=m_extractMessage;
        RetailAnimationLibrary::Get().Refresh();
    }
    if(m_openExtraction){m_openExtraction=false;ImGui::OpenPopup("###RetailAnimationExtraction");}
    if(!ImGuiExtra::BeginFittedModal("RetailAnimationExtraction",_("edition.t8.extract.title"),nullptr,60.0f))return;
    const bool busy=ExtractionBusy();
    ImGui::TextWrapped("%s",_("edition.t8.extract.explanation"));
    ImGui::BeginDisabled(busy);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
    if(ImGui::InputTextWithHint("##extractSearch",_("edition.t8.extract.search_hint"),m_extractSearch,sizeof(m_extractSearch)))m_extractFilterDirty=true;
    if(m_extractFilterDirty) {
        const auto query=NormalizeSearch(m_extractSearch);m_extractShown.clear();
        for(size_t i=0;i<m_extractChoices.size();++i)if(query.empty() || m_extractChoices[i].search.find(query)!=std::string::npos)m_extractShown.push_back(i);
        m_extractFilterDirty=false;
    }
    // Both act on the rows the search shows.
    if(ImGui::SmallButton(_("edition.t8.extract.select_all")))for(auto i:m_extractShown)m_extractChoices[i].selected=true;
    ImGui::SameLine();
    if(ImGui::SmallButton(_("edition.t8.extract.select_none")))for(auto i:m_extractShown)m_extractChoices[i].selected=false;
    ImGui::SameLine();ImGuiExtra::HelpMarker(_("edition.t8.extract.select_explanation"));
    if(ImGui::BeginTable("##extractAnimations",4,ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_Borders|ImGuiTableFlags_Resizable,
        ImVec2(0,ImGui::GetFrameHeightWithSpacing()*16))) {
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableSetupColumn(_("edition.t8.extract.column_name"),ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(_("edition.t8.extract.column_category"),ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(_("edition.t8.extract.column_id"),ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(_("edition.t8.extract.column_size"),ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(int(m_extractShown.size()));
        while(clipper.Step())for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;++line) {
            auto& choice=m_extractChoices[m_extractShown[line]];const auto& animation=choice.animation;
            ImGui::TableNextRow();ImGui::TableNextColumn();
            ImGui::PushID(int(m_extractShown[line]));
            ImGui::Checkbox(animation.name.empty()?animation.file.c_str():animation.name.c_str(),&choice.selected);
            ImGui::PopID();
            ImGui::TableNextColumn();ImGui::TextUnformatted(_(std::format("extraction.panm.{}",RetailT8Animations::Categories[animation.category]).c_str()));
            ImGui::TableNextColumn();ImGui::Text("%08X",animation.instance);
            ImGui::TableNextColumn();ImGui::Text("%u KB",(animation.bytes+1023)/1024);
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    const auto selected=size_t(std::count_if(m_extractChoices.begin(),m_extractChoices.end(),[](const auto& c){return c.selected;}));
    ImGui::TextUnformatted(Fmt("edition.t8.extract.selected",selected,m_extractChoices.size()).c_str());
    if(busy) {
        const auto done=m_extractDone.load();
        ImGui::ProgressBar(m_extractTotal?float(done)/float(m_extractTotal):0.0f,ImVec2(-FLT_MIN,0),std::format("{}/{}",done,m_extractTotal).c_str());
        if(ImGuiExtra::RenderButtonEnabled(_("edition.animation_extraction.interrupt"),!m_extractInterrupt))m_extractInterrupt=true;
    } else {
        if(!m_extractMessage.empty()){
            if(m_extractFailed)ImGuiExtra_TextboxError(m_extractMessage.c_str());else ImGui::TextWrapped("%s",m_extractMessage.c_str());
        }
        if(ImGuiExtra::RenderButtonEnabled(_("edition.t8.extract.start"),selected!=0 && m_extractSnapshot)) {
            std::vector<RetailT8Animations::Animation> selection;
            for(const auto& choice:m_extractChoices)if(choice.selected)selection.push_back(choice.animation);
            try {
                const auto base=std::filesystem::absolute(EDITOR_LIB_DIRECTORY);std::filesystem::create_directories(base);
                const auto stem=std::filesystem::path(m_loadedCharacter.filename).stem().wstring()+L"_"+std::to_wstring(Helpers::getCurrentTimestamp());
                auto directory=base/stem;
                for(unsigned suffix=2;std::filesystem::exists(directory);++suffix)directory=base/(stem+L"_"+std::to_wstring(suffix));
                m_extractInterrupt=false;m_extractDone=0;m_extractTotal=uint32_t(selection.size());m_extractMessage.clear();m_extractFailed=false;
                const std::string extracted=_("edition.t8.extracted"),stopped=_("edition.t8.extract.stopped");
                m_extractJob=std::async(std::launch::async,[this,snapshot=m_extractSnapshot,selection=std::move(selection),directory,extracted,stopped]() {
                    const auto* header=reinterpret_cast<const TKMovesetHeader*>(snapshot->data());
                    const auto count=RetailT8Animations::Extract(std::span<const Byte>(*snapshot).subspan(header->moveset_data_start),selection,directory,&m_extractInterrupt,&m_extractDone);
                    if(m_extractInterrupt)return stopped;
                    const auto where=Helpers::to_utf8(directory.wstring());
                    return std::vformat(extracted,std::make_format_args(count,where));
                });
            }catch(const std::exception& e){m_extractMessage=e.what();m_extractFailed=true;}
        }
        ImGui::SameLine();
        if(ImGui::Button(_("close")))ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
uint32_t EditorVisualsT8Retail::ResolveAnimation(std::string_view name) const {
    for(uint32_t row=0;row<m_editor->Count(12);++row){auto record=m_editor->Record(12,row);uint64_t value,key;memcpy(&value,record.data()+0x20,8);memcpy(&key,record.data()+0x28,8);
        auto hash=EditorT8Retail::Decode(value,key);if(hash && NormalizeSearch(NameForHash(*hash))==NormalizeSearch(std::string(name)))return row;}
    throw std::invalid_argument(_("edition.t8.animation_missing"));
}
void EditorVisualsT8Retail::CreateRecord(size_t table){
    try{
        CheckStructuralEdit(nullptr);auto row=m_editor->CreateNew(EditorT8Retail::Type(table));MarkDirty();m_filterDirty=true;
        // Say what the new move was built from (EditorT8Retail::CreateNew copies the idle stance).
        if(table==12)try{m_saveMessage=Fmt("edition.t8.new_move_created",row,RecordLabel(12,m_editor->ResolveMove(0x8001)));}catch(const std::exception&){}
        OpenRecord(table,row);
    }
    catch(const std::exception& error){m_saveMessage=error.what();}
}
