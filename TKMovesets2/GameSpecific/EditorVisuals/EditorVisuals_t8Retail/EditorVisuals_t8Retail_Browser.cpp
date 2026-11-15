#include "Importer_t8/RetailLiveEdit.hpp"
#include "RetailCommands.hpp"
#include "EditorVisuals_t8Retail.hpp"
#include "Games.hpp"
#include "Settings.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "TEditor.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include "FileIO.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include <charconv>
#include <format>

namespace {
template<class T>T At(std::span<const Byte> bytes,size_t offset){T v{};if(offset+sizeof(v)>bytes.size())throw std::out_of_range("Truncated move");memcpy(&v,bytes.data()+offset,sizeof(v));return v;}
uint32_t Crc(std::span<const Byte> bytes){return Helpers::CalculateCrc32(bytes.data(),bytes.size());}
}
void EditorVisualsT8Retail::RebuildBrowser() {
    if(m_indexTable!=m_table || m_indexRevision!=m_editRevision) {
        m_searchIndex.clear();m_moveRows.clear();
        for(uint32_t row=0;row<m_editor->Count(m_table);++row) {
            m_searchIndex.push_back(SearchMetadata(m_table,row));
            if(m_table!=12)continue;
            MoveRow move;const auto data=m_editor->Record(12,row);
            if(auto hash=m_editor->MoveNameHash(row))move.name=NameForHash(*hash);else move.name=_("edition.t8.unknown_move");
            const auto aliases=m_editor->MoveAliases(row);
            for(auto alias:aliases){if(!move.aliases.empty())move.aliases+=" / ";move.aliases+=std::to_string(alias);}
            if(!aliases.empty())move.flags|=EditorMoveFlags_Generic;
            auto hit=EditorT8Retail::Decode(At<uint64_t>(data,0x78),At<uint64_t>(data,0x80)).value_or(0);
            bool any=false,active=false;
            for(unsigned box=0;box<8;++box) {
                auto start=At<uint32_t>(data,0x160+box*0x30),end=At<uint32_t>(data,0x164+box*0x30),location=At<uint32_t>(data,0x168+box*0x30);
                any|=start||end||location;active|=start&&end&&location;
            }
            if(hit||any)move.flags|=hit&&active ? EditorMoveFlags_Attack : EditorMoveFlags_OtherAttack;
            if(move.name.ends_with("_y"))move.flags|=EditorMoveFlags_Throw;
            else if(move.name.ends_with("_n"))move.flags|=EditorMoveFlags_ThrowReaction;
            if(move.name.starts_with(MOVESET_CUSTOM_MOVE_NAME_PREFIX) || (row>=m_originalMoveCrc.size() || m_editor->MoveFingerprint(row)!=m_originalMoveCrc[row]))move.flags|=EditorMoveFlags_Custom;
            move.colour=TEditorUtils::GetMoveColorFromFlag(move.flags);m_moveRows.push_back(std::move(move));
        }
        m_indexTable=m_table;m_indexRevision=m_editRevision;m_filterDirty=true;
    }
    if(!m_filterDirty)return;
    m_filtered.clear();const auto query=NormalizeSearch(m_search);
    uint32_t idle=0;try{idle=m_editor->ResolveMove(0x8001);}catch(const std::exception&){}
    for(uint32_t row=0;row<m_editor->Count(m_table);++row) {
        if(!query.empty() && m_searchIndex[row].find(query)==std::string::npos)continue;
        if(m_table==12) {
            const auto flags=m_moveRows[row].flags;
            if(m_moveFilter==1 && !(flags&(EditorMoveFlags_Attack|EditorMoveFlags_OtherAttack)))continue;
            if(m_moveFilter==2 && !(flags&EditorMoveFlags_Generic))continue;
            if(m_moveFilter==3 && !(flags&(EditorMoveFlags_Throw|EditorMoveFlags_ThrowReaction)))continue;
            if(m_moveFilter==4 && !(flags&EditorMoveFlags_Custom))continue;
            if(m_moveFilter==5 && row<idle)continue;
        }
        m_filtered.push_back(row);
    }
    m_filterDirty=false;
}
void EditorVisualsT8Retail::SelectMove(uint32_t id,bool open) {
    auto row=m_editor->ResolveMove(id);
    m_table=12;m_moveFilter=0;m_search[0]=0;m_filterDirty=true;
    m_selectedMove=m_scrollMove=row;snprintf(m_moveId,sizeof(m_moveId),"%u",row);
    if(open)OpenRecord(12,row);
}
void EditorVisualsT8Retail::RenderBrowser() {
    if(m_table==12) {
        constexpr const char* tabs[]={"edition.moves_all","edition.moves_attacks","edition.moves_generic","edition.moves_throws","edition.moves_char_specific","edition.moves_custom"};
        constexpr int filters[]={0,1,2,3,5,4};
        const float width=ImGui::GetContentRegionAvail().x/7;
        for(int i=0;i<6;++i) {
            if(i)ImGui::SameLine();ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button,m_moveFilter==filters[i]?MOVELIST_TAB_ACTIVE:MOVELIST_TAB_INACTIVE);
            if(ImGui::Button(_(tabs[i]),ImVec2(width,0))){m_moveFilter=filters[i];m_filterDirty=true;}
            ImGui::PopStyleColor();if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",filters[i]==4?_("edition.t8.custom_filter_tooltip"):_(tabs[i]));ImGui::PopID();
        }
    }
    RebuildBrowser();
    const float reserve=ImGui::GetFrameHeightWithSpacing()*(m_table==12?2:1);
    auto size=ImVec2(0,(std::max)(1.0f,ImGui::GetContentRegionAvail().y-reserve));
    if(ImGui::BeginTable("RetailRecords",m_table==12?3:2,ImGuiTableFlags_SizingFixedSame|ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_Borders|ImGuiTableFlags_Resizable,size)) {
        ImGui::TableSetupColumn(_("edition.t8.column_id"));ImGui::TableSetupColumn(m_table==12?_("edition.move_name"):_("edition.t8.column_record"));
        if(m_table==12)ImGui::TableSetupColumn(_("edition.move_generic_id"));
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;clip.Begin(static_cast<int>(m_filtered.size()));
        if(m_scrollMove>=0) {auto found=std::find(m_filtered.begin(),m_filtered.end(),static_cast<uint32_t>(m_scrollMove));if(found!=m_filtered.end())clip.IncludeItemByIndex(static_cast<int>(found-m_filtered.begin()));}
        while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
            auto row=m_filtered[i];ImGui::PushID(static_cast<int>(row));ImGui::TableNextRow();
            if(m_table==12 && m_moveRows[row].colour)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,m_moveRows[row].colour);
            ImGui::TableNextColumn();ImGui::Text("%u",row);ImGui::TableNextColumn();
            auto label=m_table==12?m_moveRows[row].name:RecordLabel(m_table,row);
            if(ImGui::Selectable(label.c_str(),m_table==12 && m_selectedMove==row,ImGuiSelectableFlags_SpanAllColumns)) {
                try {if(m_table==12){m_selectedMove=row;snprintf(m_moveId,sizeof(m_moveId),"%u",row);}OpenRecord(m_table,row);}catch(const std::exception& e){m_previewMessage=e.what();}
            }
            if(ImGui::BeginPopupContextItem("Record actions")) {
                if(ImGui::MenuItem(_("edition.t8.open")))OpenRecord(m_table,row);
                if(ImGui::MenuItem(_("edition.t8.find_references")))FindReferences(m_table,row);
                if(m_table==12 && ImGui::MenuItem(_("edition.t8.copy_move_id")))ImGui::SetClipboardText(std::to_string(row).c_str());
                ImGui::EndPopup();
            }
            if(m_table==12) {
                ImGui::TableNextColumn();ImGui::TextUnformatted(m_moveRows[row].aliases.c_str());
                if(m_scrollMove==row){ImGui::SetScrollHereY(0.5f);m_scrollMove=-1;}
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if(m_table==12) {
        auto half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)/2;
        ImGui::SetNextItemWidth(half);
        if(m_focusSearch){ImGui::SetKeyboardFocusHere();m_focusSearch=false;}
        if(ImGui::InputTextWithHint("##moveId",_("edition.move_id_hint"),m_moveId,sizeof(m_moveId),ImGuiInputTextFlags_EnterReturnsTrue)) {
            try {
                std::string_view text=m_moveId;int base=10;if(text.starts_with("0x")||text.starts_with("0X")){text.remove_prefix(2);base=16;}
                uint32_t id=0;auto value=std::from_chars(text.data(),text.data()+text.size(),id,base);
                if(value.ec!=std::errc{} || value.ptr!=text.data()+text.size()){
                    const auto query=NormalizeSearch(m_moveId);auto found=std::find_if(m_moveRows.begin(),m_moveRows.end(),[&](const auto& row){return NormalizeSearch(row.name).find(query)!=std::string::npos;});
                    if(query.empty() || found==m_moveRows.end())throw std::invalid_argument(_("edition.t8.no_matching_move"));
                    id=static_cast<uint32_t>(found-m_moveRows.begin());
                }
                SelectMove(id);m_previewMessage.clear();
            }catch(const std::exception& e){m_previewMessage=e.what();}
        }
        ImGui::SameLine();if(ImGui::Button(_("edition.move_current"),ImVec2(half,0)))CurrentMove(m_previewPlayer);
        for(uint8_t slot=0;slot<2;++slot) {
            if(slot)ImGui::SameLine();const auto& binding=m_previewBindings[slot];
            const bool ready=!PreviewBusy() && m_selectedMove>=0 && binding.root && binding.pid==m_previewReader.processId && binding.revision==m_editRevision && binding.root==m_playerStates[slot].root;
            if(ImGuiExtra::RenderButtonEnabled(_(slot?"edition.play_move_2p":"edition.play_move_1p"),ready,ImVec2(half,0)))StartPreview(false,slot);
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",_("edition.t8.play_tooltip"));
        }
    }else{
        ImGui::SetNextItemWidth(-1);if(m_focusSearch){ImGui::SetKeyboardFocusHere();m_focusSearch=false;}
        if(ImGui::InputTextWithHint("##recordSearch",_("edition.t8.record_search_hint"),m_search,sizeof(m_search)))m_filterDirty=true;
    }
}
bool EditorVisualsT8Retail::PreviewBusy() const {return m_previewJob.valid();}
void EditorVisualsT8Retail::ScanGameProcess() {
    const auto now=std::chrono::steady_clock::now();if(now<m_nextProcessScan)return;m_nextProcessScan=now+std::chrono::seconds(2);
    if(m_previewReader.IsAttached()){m_gameRunning=true;return;}
    const auto* profile=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);
    m_gameRunning=false;
    for(const auto& process:GameProcessUtils::GetRunningProcessList())if(profile && process.name==profile->processName){m_gameRunning=true;break;}
}
void EditorVisualsT8Retail::RefreshPreview(bool force) {
    if(!m_previewConnected)return;
    const auto now=std::chrono::steady_clock::now();if(!force && now<m_nextPreviewRead)return;m_nextPreviewRead=now+std::chrono::milliseconds(250);
    if(m_previewReader.IsAttached() && !m_previewReader.CheckRunning()){m_previewReader.Detach();m_checkedRoots={};m_playerMatches={};}
    if(!m_previewReader.IsAttached()) {
        if(!m_previewReader.Attach("Polaris-Win64-Shipping.exe",0)){m_playerStates={};m_playerMatches={};return;}
        m_previewAddresses.Reload();m_previewGame.SetProcess(&m_previewReader,&m_previewAddresses);
        m_previewGame.SetCurrentGame(Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL));m_checkedRoots={};
    }
    for(uint8_t slot=0;slot<2;++slot)try {
        auto state=RetailT8Preview::Read(m_previewReader,m_previewGame,*Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL),slot);
        if(m_checkedRoots[slot]!=state.root) {
            m_playerMatches[slot]=false;
            if(state.count==m_editor->Count(12)) {
                std::vector<Byte> moves(state.count*0x448);
                if(m_previewReader.probeBytes(state.moves,moves.data(),moves.size()))m_playerMatches[slot]=m_editor->MatchesMoveNames(moves);
            }
            m_checkedRoots[slot]=state.root;
        }
        m_playerStates[slot]=state;
    }catch(const std::exception&){m_playerStates[slot]={};m_playerMatches[slot]=false;m_checkedRoots[slot]=0;}
}
void EditorVisualsT8Retail::CurrentMove(uint8_t slot) {
    m_previewConnected=true;RefreshPreview(true);
    m_previewMessage.clear();
    if(slot>=m_playerStates.size() || !m_playerMatches[slot] || !m_playerStates[slot].current)return;
    try {
        SelectMove(*m_playerStates[slot].current);
    }catch(const std::exception& e){m_previewMessage=e.what();}
}
void EditorVisualsT8Retail::StartPreview(bool import,uint8_t slot) {
    if(PreviewBusy())return;
    try {
        auto bytes=import ? (slot==1 && m_bothSnapshot ? m_bothSnapshot : std::make_shared<const std::vector<Byte>>(m_editor->BuildSaveSnapshot())) : nullptr;
        const auto requireBinding=import && m_liveEdition;
        const auto binding=m_previewBindings[slot];const auto revision=slot==1 && m_bothSnapshot?m_bothRevision:m_editRevision;
        if(import && slot==0 && m_importBothPending){m_bothSnapshot=bytes;m_bothRevision=revision;}
        const auto move=m_selectedMove<0?0:static_cast<uint32_t>(m_selectedMove);
        m_previewMessage=_(import?"edition.t8.importing":"edition.t8.queuing");
        // Translations are resolved here, on the UI thread; the worker only formats them.
        struct Texts {std::string cannotAttach,liveStopped,liveEdit,reimported,imported,queued;} texts{
            _("edition.t8.cannot_attach"),_("edition.t8.live_stopped"),_("edition.t8.live_edit_written"),_("edition.t8.reimported"),_("edition.t8.imported"),_("edition.t8.move_queued")};
        m_previewJob=std::async(std::launch::async,[bytes=std::move(bytes),slot,binding,revision,move,import,requireBinding,texts]() {
            PreviewResult result;result.slot=slot;result.revision=revision;result.imported=import;
            const int player=slot+1;
            try {
                GameProcess process;
                const bool attached=import?process.Attach("Polaris-Win64-Shipping.exe",PROCESS_VM_WRITE|PROCESS_VM_OPERATION):process.AttachById(binding.pid,PROCESS_VM_WRITE|PROCESS_VM_OPERATION);
                if(!attached)throw std::runtime_error(texts.cannotAttach);
                GameAddressesFile addresses;addresses.Reload();GameData game;game.SetProcess(&process,&addresses);
                const auto* profile=Games::GetGameInfoFromIdentifier(GameId_T8,GameVersions::T8::RETAIL);game.SetCurrentGame(profile);
                if(import) {
                    ImporterT8 importer(process,game,profile);
                    if(requireBinding && (process.processId!=binding.pid || importer.GetMovesetAddress(slot)!=binding.root))throw std::runtime_error(texts.liveStopped);
                    result.snapshot=bytes;
                    // Live editing writes changed records in place; structural edits reimport.
                    auto published=RetailLiveEdit::Apply(importer,process,slot,requireBinding?binding.root:0,requireBinding?binding.snapshot.get():nullptr,*bytes);
                    result.root=published.root;
                    if(published.direct) {
                        result.pid=process.processId;result.ok=true;
                        const auto records=published.sync.records;
                        result.message=std::vformat(texts.liveEdit,std::make_format_args(records,player));
                        return result;
                    }
                    if(requireBinding && published.sync.structural) {
                        result.pid=process.processId;result.ok=true;
                        const auto& reason=published.sync.reason;
                        result.message=std::vformat(texts.reimported,std::make_format_args(player,reason));
                        return result;
                    }
                }else {RetailT8Preview::Queue(process,game,*profile,slot,binding.root,move);result.root=binding.root;}
                result.pid=process.processId;result.ok=true;result.message=std::vformat(import?texts.imported:texts.queued,std::make_format_args(player));
            }catch(const std::exception& e){result.message=e.what();}
            return result;
        });
    }catch(const std::exception& e){m_previewMessage=e.what();m_liveEdition=false;m_importBothPending=false;m_bothSnapshot.reset();}
}
void EditorVisualsT8Retail::PollPreview() {
    if(!PreviewBusy() || m_previewJob.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    auto result=m_previewJob.get();m_previewMessage=result.message;
    if(result.ok && result.gameAction) {
        if(result.imported)m_previewBindings[result.slot]={result.root,result.pid,result.revision,result.snapshot};
        m_previewConnected=true;m_checkedRoots[result.slot]=0;RefreshPreview(true);
    }else if(!result.ok)m_liveEdition=false;
    if(m_importBothPending) {
        m_importBothPending=false;
        if(result.ok && result.imported){StartPreview(true,1);m_bothSnapshot.reset();}
        else m_bothSnapshot.reset();
    }
}
void EditorVisualsT8Retail::RenderPreviewControls(bool unapplied) {
    const auto separate=[] {ImGui::SameLine();ImGui::TextDisabled("|");ImGui::SameLine();};
    if(ImGuiExtra::RenderButtonEnabled(_("edition.save"),!m_saveJob.Busy()&&!m_savedLastChange&&!unapplied))Save();
    separate();ImGui::Text("%s %s",_("edition.last_saved"),m_loadedCharacter.lastSavedDate.empty()?_("edition.t8.no_saved_changes"):m_loadedCharacter.lastSavedDate.c_str());
    separate();ImGui::TextUnformatted("Tekken 8");
    // EditorVisuals_t7 RenderStatusBar process indicator.
    ScanGameProcess();ImGui::SameLine();
    if(m_previewReader.IsAttached())ImGui::TextColored(ImVec4(0.55f,0.85f,0.55f,1.0f),"%s",_("edition.t8.game_attached"));
    else if(m_gameRunning)ImGui::TextDisabled("%s",_("edition.t8.game_running"));
    else ImGuiExtra::RenderTextbox(_("edition.t8.game_not_running"),TEXTBOX_BORDER_ERROR,TEXTBOX_BORDER_ERROR,2.0f);
    separate();ImGui::SetNextItemWidth(110);
    const char* players[]={_("1p"),_("2p")};int slot=m_previewPlayer;
    if(ImGui::Combo("##previewPlayer",&slot,players,2)){m_previewPlayer=static_cast<uint8_t>(slot);m_liveEdition=false;}
    ImGui::SameLine();if(ImGuiExtra::RenderButtonEnabled(_("moveset.import"),!PreviewBusy()&&!unapplied))StartPreview(true,m_previewPlayer);
    separate();ImGui::BeginDisabled(!m_previewBindings[m_previewPlayer].root || PreviewBusy());
    ImGui::Checkbox(_("edition.live_edition"),&m_liveEdition);ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",_("edition.t8.live_editing_explanation"));
    if(m_liveEdition && !unapplied && !PreviewBusy() && m_previewBindings[m_previewPlayer].revision!=m_editRevision)StartPreview(true,m_previewPlayer);
    if(m_saveJob.Busy()){ImGui::SameLine();if(ImGui::SmallButton(_("edition.t8.cancel_save")))m_saveJob.Cancel();}
    if(!m_previewMessage.empty()){ImGui::TextUnformatted(m_previewMessage.c_str());if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",m_previewMessage.c_str());}
}
void EditorVisualsT8Retail::OnKeyboardShortcut(const std::string& id) {
    const bool pending=std::any_of(m_structWindows.begin(),m_structWindows.end(),[](auto* f){return f->unsavedChanges;});
    if(id=="keybind_editor.save"){if(!pending&&!m_saveJob.Busy())Save();return;}
    if(pending || PreviewBusy())return;
    if(id=="keybind_editor.import_p1")StartPreview(true,0);
    else if(id=="keybind_editor.import_p2")StartPreview(true,1);
    else if(id=="keybind_editor.import_both"){m_importBothPending=true;StartPreview(true,0);}
}
void EditorVisualsT8Retail::FindReferences(size_t table,uint32_t row) {
    try {m_references=m_editor->ReferencesTo(table,row);m_referenceTable=table;m_referenceRow=row;m_referenceRevision=m_editRevision;BuildReferenceDetails();
        m_referenceTitle=RecordLabel(table,row)+" - "+RetailT8Snapshot::Tables[table].name;m_referencesOpen=true;m_focusReferences=true;
    }catch(const std::exception& e){m_previewMessage=e.what();}
}
std::string EditorVisualsT8Retail::RequirementsText(uint32_t head) const {
    // EditorVisuals_t7 TEditor_MoveReferences conditions_str: "label: value, label: value" up to the 1100 terminator.
    std::string text;
    for(uint32_t row=head,count=0;row<m_editor->Count(1) && count<16;++row,++count){
        auto record=m_editor->Record(1,row);uint32_t id,p1,p2;memcpy(&id,record.data(),4);memcpy(&p1,record.data()+4,4);memcpy(&p2,record.data()+8,4);
        if(id==1100)break;
        if(!text.empty())text+=", ";
        const char* label=labels?labels->GetText(id):nullptr;
        text+=label?std::string(label):std::to_string(id);
        text+=": "+std::to_string(int32_t(p1));if(p2)text+=", "+std::to_string(int32_t(p2));
    }
    return text.empty()?std::string("Always"):text;
}
void EditorVisualsT8Retail::BuildReferenceDetails() {
    namespace C=RetailT8Commands;
    m_referenceDetails.assign(m_references.size(),std::string());
    if(m_references.empty())return;
    // Moves owning each cancel-list head through cancels and cancels_2..4.
    std::map<uint32_t,std::vector<uint32_t>> cancelOwners;
    for(uint32_t move=0;move<m_editor->Count(12);++move)for(const auto& field:EditorT8Retail::Schema(12))
        if(field.kind==EditorT8Retail::Kind::Reference && field.target==6)if(auto head=m_editor->ReferenceIndex(12,move,field))cancelOwners[*head].push_back(move);
    std::map<uint32_t,std::vector<uint32_t>> groupOwners;
    std::map<uint32_t,uint32_t> reactionUsers;bool reactionUsersBuilt=false;
    auto listHead=[&](size_t table,uint32_t row){
        const auto end=table==6?C::ListEnd:C::GroupEnd;
        while(row>0){uint64_t command;memcpy(&command,m_editor->Record(table,row-1).data(),8);if(command==end)break;--row;}
        return row;
    };
    auto movesText=[&](const std::vector<uint32_t>& moves){
        std::string text;for(size_t i=0;i<moves.size() && i<6;++i){if(i)text+=", ";text+=RecordLabel(12,moves[i]);}
        if(moves.size()>6)text+=std::format(", +{} more",moves.size()-6);return text;
    };
    auto requirementsOf=[&](size_t table,uint32_t row){
        for(const auto& field:EditorT8Retail::Schema(table))if(field.name=="requirements")if(auto head=m_editor->ReferenceIndex(table,row,field))return " | "+RequirementsText(*head);
        return std::string();
    };
    for(size_t i=0;i<m_references.size();++i){
        const auto& ref=m_references[i];std::string text;
        try{
            if(ref.table==6 || ref.table==7){
                auto record=m_editor->Record(ref.table,ref.row);uint64_t command;uint32_t windowStart,windowEnd,startingFrame;
                memcpy(&command,record.data(),8);memcpy(&windowStart,record.data()+24,4);memcpy(&windowEnd,record.data()+28,4);memcpy(&startingFrame,record.data()+32,4);
                const auto head=listHead(ref.table,ref.row);
                std::vector<uint32_t> owners;
                if(ref.table==6){if(auto found=cancelOwners.find(head);found!=cancelOwners.end())owners=found->second;}
                else{
                    auto& cached=groupOwners[head];
                    if(cached.empty())for(uint32_t cancel=0;cancel<m_editor->Count(6);++cancel){
                        auto row=m_editor->Record(6,cancel);uint64_t rowCommand;uint16_t target;memcpy(&rowCommand,row.data(),8);memcpy(&target,row.data()+36,2);
                        if(rowCommand!=C::GroupCancel || target!=head)continue;
                        if(auto found=cancelOwners.find(listHead(6,cancel));found!=cancelOwners.end())for(auto move:found->second)if(std::find(cached.begin(),cached.end(),move)==cached.end())cached.push_back(move);
                    }
                    owners=cached;
                }
                text=std::format("list {}",head);
                if(!owners.empty())text+=" of "+movesText(owners);
                auto commandText=C::IsInputSequence(command)?"sequence "+std::to_string(C::InputSequenceId(command)):C::CommandString(command);
                if(commandText.empty())commandText=std::format("0x{:X}",command);
                text+=std::format(" | {} | {}-{}, start {}",commandText,windowStart,windowEnd,startingFrame)+requirementsOf(ref.table,ref.row);
            }else if(ref.table==2){
                auto record=m_editor->Record(2,ref.row);uint32_t damage;memcpy(&damage,record.data()+8,4);
                text=std::format("damage {}",damage)+requirementsOf(2,ref.row);
            }else if(ref.table==0){
                if(!reactionUsersBuilt){reactionUsersBuilt=true;
                    for(uint32_t condition=0;condition<m_editor->Count(2);++condition)for(const auto& field:EditorT8Retail::Schema(2))
                        if(field.name=="reactions")if(auto target=m_editor->ReferenceIndex(2,condition,field))++reactionUsers[*target];
                }
                auto situation=ref.field;if(situation.ends_with("_moveid"))situation.resize(situation.size()-7);std::replace(situation.begin(),situation.end(),'_',' ');
                text="situation: "+situation;
                if(auto found=reactionUsers.find(ref.row);found!=reactionUsers.end())text+=std::format(" | used by {} hit condition(s)",found->second);
            }else if(ref.table==9 || ref.table==10 || ref.table==11){
                auto record=m_editor->Record(ref.table,ref.row);uint32_t property;memcpy(&property,record.data()+(ref.table==9?16:8),4);
                const char* label=labels?labels->GetText(property):nullptr;
                text=label?std::string(label):std::to_string(property);
            }else if(ref.table==12){
                auto aliases=m_editor->MoveAliases(ref.row);
                for(auto alias:aliases)text+=(text.empty()?"alias ":", ")+std::format("0x{:X}",alias);
            }
        }catch(const std::exception& error){text=error.what();}
        m_referenceDetails[i]=std::move(text);
    }
}
void EditorVisualsT8Retail::RenderReferences() {
    if(!m_referencesOpen)return;
    if(m_referenceRevision!=m_editRevision){if(m_referenceRow>=m_editor->Count(m_referenceTable)){m_referencesOpen=false;return;}m_references=m_editor->ReferencesTo(m_referenceTable,m_referenceRow);m_referenceRevision=m_editRevision;BuildReferenceDetails();}
    if(m_referenceDetails.size()!=m_references.size())BuildReferenceDetails();
    if(m_focusReferences){ImGui::SetNextWindowDockID(m_dockId);ImGui::SetNextWindowFocus();m_focusReferences=false;}
    ImGui::SetNextWindowSize(ImVec2(820,420),ImGuiCond_Once);
    const auto title=std::string(_("edition.t8.references_title"))+" "+m_referenceTitle+"###retail_references_"+std::to_string(reinterpret_cast<uintptr_t>(this));
    if(ImGui::Begin(title.c_str(),&m_referencesOpen)) {
        {const auto count=m_references.size();ImGui::TextUnformatted(std::vformat(_("edition.t8.references_count"),std::make_format_args(count,m_referenceTitle)).c_str());}
        if(m_references.empty())ImGui::TextUnformatted(_("references.no_reference"));
        if(ImGui::BeginTable("References",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_Borders|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,ImVec2(0,0))) {
            ImGui::TableSetupColumn(_("edition.t8.column_table"),ImGuiTableColumnFlags_WidthFixed,110.0f);ImGui::TableSetupColumn(_("edition.t8.column_record"),ImGuiTableColumnFlags_WidthFixed,180.0f);
            ImGui::TableSetupColumn(_("edition.t8.column_field"),ImGuiTableColumnFlags_WidthFixed,110.0f);ImGui::TableSetupColumn(_("edition.t8.references.details"),ImGuiTableColumnFlags_WidthStretch);ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(static_cast<int>(m_references.size()));
            while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto& ref=m_references[i];ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
                ImGui::TextUnformatted(RetailT8Snapshot::Tables[ref.table].name);ImGui::TableNextColumn();
                if(ImGui::Selectable(RecordLabel(ref.table,ref.row).c_str(),false,ImGuiSelectableFlags_SpanAllColumns))OpenRecord(ref.table,ref.row);
                ImGui::TableNextColumn();ImGui::TextUnformatted(ref.field.c_str());
                ImGui::TableNextColumn();const auto& details=m_referenceDetails[i];ImGui::TextUnformatted(details.c_str());
                if(!details.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*40.0f);ImGui::TextUnformatted(details.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
