#pragma once
#include "EditorVisuals.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "GameStaticData.hpp"
#include "CancelGraph.hpp"
#include "Importer_t8/RetailPreview.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include <atomic>
#include <future>
#include <chrono>

class EditorVisualsT8Retail final : public EditorVisuals {
    GameProcess m_offlineProcess;
    GameData m_offlineGame;
    EditorT8Retail* m_editor = nullptr;
    GameStaticData m_names;
    std::string m_character, m_loadError;
    size_t m_table = 12;
    char m_search[128]{};
    std::vector<uint32_t> m_filtered;
    bool m_filterDirty = true;
    bool m_closePrompt = false;
    bool m_focusSearch=false;
    bool m_labelQueryPending=false;
    uint64_t m_indexRevision=UINT64_MAX, m_savedRevision=0;
    size_t m_indexTable=SIZE_MAX;
    std::vector<std::string> m_searchIndex;
    struct MoveRow {std::string name,aliases;uint32_t flags=0;ImU32 colour=0;bool tornado=false;};
    std::vector<MoveRow> m_moveRows;
    std::vector<uint32_t> m_originalMoveCrc;
    int m_moveFilter=0;
    int64_t m_selectedMove=-1,m_scrollMove=-1;
    // Frames left during which the move list takes the focus back (after it opened a move, which focuses its form).
    int m_browserKeepFocus=0;
    char m_moveId[32]{};
    // Row typed in the menu bar's "Go to" menu.
    int m_gotoId=0;
    struct PreviewResult {bool ok=false,imported=false,gameAction=true;uint8_t slot=0;uint64_t revision=0;DWORD pid=0;gameAddr root=0;std::string message;std::shared_ptr<const std::vector<Byte>> snapshot;};
    // snapshot: the document bytes currently published at root (direct live edits diff against it).
    struct PreviewBinding {gameAddr root=0;DWORD pid=0;uint64_t revision=UINT64_MAX;std::shared_ptr<const std::vector<Byte>> snapshot;};
    std::future<PreviewResult> m_previewJob;
    std::array<PreviewBinding,2> m_previewBindings{};
    std::array<RetailT8Preview::State,2> m_playerStates{};
    std::array<bool,2> m_playerMatches{};
    std::array<gameAddr,2> m_checkedRoots{};
    GameProcess m_previewReader;
    GameData m_previewGame;
    GameAddressesFile m_previewAddresses;
    std::chrono::steady_clock::time_point m_nextPreviewRead{};
    bool m_previewConnected=false;
    uint8_t m_previewPlayer=1;
    std::string m_previewMessage;
    void RebuildBrowser();
    void RenderBrowser();
    void SelectMove(uint32_t,bool open=true);
    void RefreshPreview(bool force=false);
    void CurrentMove(uint8_t);
    bool PreviewBusy() const;
    void StartPreview(bool import,uint8_t slot);
    void PollPreview();
    void RenderPreviewControls(bool unapplied);
    // "Extract animations" modal: every embedded animation of the document with a checkbox; the selection is written
    // to a new editor_library folder on a worker thread (RetailT8Animations::Extract).
    struct ExtractChoice {RetailT8Animations::Animation animation;std::string search;bool selected=true;};
    std::vector<ExtractChoice> m_extractChoices;
    std::vector<size_t> m_extractShown;
    char m_extractSearch[128]{};
    bool m_extractFilterDirty=true,m_openExtraction=false,m_extractFailed=false;
    std::shared_ptr<const std::vector<Byte>> m_extractSnapshot;
    // Read by the worker: declared before m_extractJob so they outlive it (the future's destructor waits for the job).
    std::atomic<bool> m_extractInterrupt=false;
    std::atomic<uint32_t> m_extractDone=0;
    std::future<std::string> m_extractJob;
    uint32_t m_extractTotal=0;
    std::string m_extractMessage;
    void OpenAnimationExtraction();
    void RenderAnimationExtraction();
    bool ExtractionBusy() const {return m_extractJob.valid();}
    void RenderReferences();
    // T7 TEditor_MoveReferences details: owning moves, decoded command, window and requirements per reference.
    void BuildReferenceDetails();
    std::string RequirementsText(uint32_t head) const;
    std::vector<std::string> m_referenceDetails;
    // Game-attached indicator (T7 status bar): a process-list scan every 2 s, no handle is opened.
    std::chrono::steady_clock::time_point m_nextProcessScan{};
    bool m_gameRunning=false;
    void ScanGameProcess();
    bool m_referencesOpen=false;
    bool m_focusReferences=false;
    std::string m_referenceTitle;
    std::vector<EditorT8Retail::Reference> m_references;
    bool m_importBothPending=false;
    std::shared_ptr<const std::vector<Byte>> m_bothSnapshot;
    uint64_t m_bothRevision=0;
    size_t m_referenceTable=0;uint32_t m_referenceRow=0;uint64_t m_referenceRevision=UINT64_MAX;
    bool m_restoringForms=false;
    std::vector<std::pair<size_t,uint32_t>> m_reopen;
    void LoadNames();
    void PopulateWindowCreatorMap() override {}
    void OnKeyboardShortcut(const std::string&) override;
    void RenderGameSpecific(int dockid) override;
public:
    EditorVisualsT8Retail(const movesetInfo*, GameAddressesFile*, LocalStorage*);
    std::string RecordWindowId(size_t table,uint32_t row) const;
    const EditorT8Retail* Editor() const {return m_editor;}
    // Open record form of (table,row), for tests.
    const EditorForm* RecordForm(size_t table,uint32_t row) const {for(auto* form:m_structWindows)if(form->popen && form->windowType==EditorT8Retail::Type(table) && form->structureId==row)return form;return nullptr;}
    static std::string NormalizeSearch(std::string text);
    std::string SearchMetadata(size_t table,uint32_t row) const;
    // Never throws: called from render code (browser, references, forms, Alt+arrows). A record that cannot
    // be opened (row outside its table, list without a terminator) leaves the reason in the status line.
    // Returns whether a form is open on the record.
    bool OpenRecord(size_t table, uint32_t row);
    const std::string& StatusMessage() const {return m_saveMessage;}
    std::string RecordLabel(size_t table, uint32_t row) const;
    std::string NameForHash(uint32_t hash) const;
    std::string CharacterName(uint32_t id) const {return m_names.CharacterName(id);}
    void FindReferences(size_t table,uint32_t row);
    bool LiveEditing() const {return m_liveEdition;}
    void CheckStructuralEdit(const EditorForm* source) const;
    void StructureChanged(EditorForm* source,size_t table,const RecordEdit& edit);
    uint32_t ResolveAnimation(std::string_view name) const;
    void CreateRecord(size_t table);
    // Cancel graph of a move form (EditorVisuals_t8Retail_CancelGraph.cpp; BuildCancelGraph over the main cancel list)
    CancelGraph BuildCancelGraph(uint32_t row, int depth, bool incoming, bool hideCommon) const;
};
