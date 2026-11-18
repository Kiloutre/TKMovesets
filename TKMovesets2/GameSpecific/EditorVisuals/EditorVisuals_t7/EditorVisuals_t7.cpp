#include "EditorVisuals_t7.hpp"
#include "GameSharedMem.hpp"
#include "Compression.hpp"
#include "Localization.hpp"
#include "GameIDs.hpp"
// Structures
#include "Structures/TEditorStructures.hpp"
#include "Structures/DisplayableMovelist/TEditorMovelist.hpp"

using namespace EditorVisualsT7;

// -- Private methods -- //

void EditorVisuals_T7::PopulateWindowCreatorMap()
{
	m_windowCreatorMap = {
		{TEditorWindowType_Move, { new EditorWindowFactory<TEditorMove>, "move" }},

		{TEditorWindowType_Voiceclip, { new EditorWindowFactory<TEditorVoiceclip>, "voiceclip" }},

		{TEditorWindowType_Extraproperty, { new EditorWindowFactory<TEditorExtraproperties>, "extraproperty" }},
		{TEditorWindowType_MoveBeginProperty, { new EditorWindowFactory<TEditorMoveStartProperty>, "move_start_extraprop" }},
		{TEditorWindowType_MoveEndProperty, { new EditorWindowFactory<TEditorMoveEndProperty>, "move_end_extraprop" }},

		{TEditorWindowType_Requirement, { new EditorWindowFactory<TEditorRequirements>, "requirement" }},

		{TEditorWindowType_HitCondition, { new EditorWindowFactory<TEditorHitConditions>, "hit_condition" }},
		{TEditorWindowType_Reactions, { new EditorWindowFactory<TEditorReactions>, "reactions" }},
		{TEditorWindowType_Pushback, { new EditorWindowFactory<TEditorPushback>, "pushback" }},
		{TEditorWindowType_PushbackExtradata, { new EditorWindowFactory<TEditorPushbackExtra>, "pushback_extradata" }},

		{TEditorWindowType_Cancel, { new EditorWindowFactory<TEditorCancels>, "cancel" }},
		{TEditorWindowType_CancelExtradata, { new EditorWindowFactory<EditorForm>, "cancel_extra" }},
		{TEditorWindowType_GroupedCancel, { new EditorWindowFactory<TEditorGroupedCancels>, "grouped_cancel" }},

		{TEditorWindowType_InputSequence, { new EditorWindowFactory<TEditorInputSequence>, "input_sequence" }},
		{TEditorWindowType_Input, { new EditorWindowFactory<TEditorInputStruct>, "input" }},

		{TEditorWindowType_Projectile, { new EditorWindowFactory<TEditorProjectile>, "projectile" }},

		{TEditorWindowType_CameraData, { new EditorWindowFactory<EditorForm>, "camera_data" }},
		{TEditorWindowType_ThrowCamera, { new EditorWindowFactory<TEditorThrowCamera>, "throw_camera" }},

		{TEditorWindowType_MovelistDisplayable, { new EditorWindowFactory<TEditorMovelistDisplayable>, "mvl_displayable" }},
		{TEditorWindowType_MovelistPlayable, { new EditorWindowFactory<TEditorMovelistPlayable>, "mvl_playable" }},
		{TEditorWindowType_MovelistInput, { new EditorWindowFactory<TEditorMovelistInput>, "mvl_input"}}
	};
}

void EditorVisuals_T7::FilterMovelist(EditorMovelistFilter_ filter)
{
	m_filteredMovelist.clear();

	m_movelistFilter = filter;
	if (filter == EditorMovelistFilter_All) {
		m_filteredMovelist = *movelist;
		return;
	}

	if (filter == EditorMovelistFilter_PostIdle) {
		// Get idle move ID, only list moves beyond it
		size_t startingIndex = m_editor->aliases[1];
		for (; startingIndex < movelist->size(); ++startingIndex) {
			m_filteredMovelist.push_back(movelist->at(startingIndex));
		}
		return;
	}

	EditorMoveFlags flags = 0;

	switch (filter)
	{
	case EditorMovelistFilter_Attacks:
		flags |= EditorMoveFlags_Attack | EditorMoveFlags_OtherAttack;
		break;
	case EditorMovelistFilter_Generic:
		flags |= EditorMoveFlags_Generic;
		break;
	case EditorMovelistFilter_ThrowCameras:
		flags |= EditorMoveFlags_Throw | EditorMoveFlags_ThrowReaction;
		break;
	case EditorMovelistFilter_Custom:
		flags |= EditorMoveFlags_Custom;
		break;
	}

	for (DisplayableMove* move : *movelist)
	{
		if (move->flags & flags) {
			m_filteredMovelist.push_back(move);
		}
	}

	// If displaying generic moves, sort by generic id
	if (filter & EditorMovelistFilter_Generic) {
		struct sortByAlias {
			bool operator()(DisplayableMove const* a, DisplayableMove const* b) const {
				return a->aliasId < b->aliasId;
			}
		};
		std::sort(m_filteredMovelist.begin(), m_filteredMovelist.end(), sortByAlias());
	}
}

int32_t EditorVisuals_T7::ValidateMoveId(const char* buf)
{
	int32_t moveId = atoi(buf);

	const size_t movelistSize = movelist->size();
	if (moveId >= movelistSize)
	{
		const size_t aliasesCount = m_editor->aliases.size();
		if (moveId < 0x8000 || moveId >= (0x8000 + aliasesCount)) {
			return -1;
		}
		moveId = m_editor->aliases[(uint16_t)moveId - (uint16_t)0x8000];
	}

	return moveId;
}

void EditorVisuals_T7::OnMoveCreate(unsigned int moveId)
{
	m_editor->ReloadDisplayableMoveList();
    m_editor->mustReloadMovelist = true;
	m_moveToScrollTo = moveId;
	m_highlightedMoveId = moveId;
}

bool EditorVisuals_T7::MovesetStillLoaded()
{
	gameAddr movesetAddress = m_importerHelper->GetCurrentPlayerMovesetAddr();
#ifdef BUILD_TYPE_DEBUG
	if (movesetAddress != m_loadedMoveset) {
		DEBUG_LOG("MovesetStillLoaded = false, current is %llx, expected is %llx\n", movesetAddress, m_loadedMoveset);
	}
#endif
	return movesetAddress == m_loadedMoveset;
}

void EditorVisuals_T7::OnKeyboardShortcut(const std::string& identifier)
{
	if (identifier == "keybind_editor.import_p1")
	{
		bool isAttached = m_importerHelper->process.IsAttached();
		bool canImport = isAttached && !m_importerHelper->IsBusy() && m_canInteractWithGame && !RetailConversionBusy();

		if (canImport) {
			ImportToPlayer(0);
		}
	}
	else if (identifier == "keybind_editor.import_p2")
	{
		bool isAttached = m_importerHelper->process.IsAttached();
		bool canImport = isAttached && !m_importerHelper->IsBusy() && m_canInteractWithGame && !RetailConversionBusy();

		if (canImport) {
			ImportToPlayer(1);
		}
	}
	else if (identifier == "keybind_editor.import_both")
	{
		bool isAttached = m_importerHelper->process.IsAttached();
		bool canImport = isAttached && !m_importerHelper->IsBusy() && m_canInteractWithGame && !RetailConversionBusy();

		if (canImport) {
			ImportToPlayer(-1);
		}
	}
	else if (identifier == "keybind_editor.save")
	{
		if (!m_savedLastChange)
		{
			Save();
		}
	}
}

void EditorVisuals_T7::ImportToPlayer(int playerid)
{
	DEBUG_LOG("ImportToPlayer %d\n", playerid);

	uint64_t movesetSize;
	const Byte* moveset = m_editor->GetMoveset(movesetSize);

	m_editor->live_loadedMoveset = 0;
	m_importNeeded = false;
	m_loadedMoveset = 0; // We will get the loaded moveset later since the import is in another thread
	static_cast<EditorT7*>(m_abstractEditor.get())->SetRetailLive(nullptr);
	static_cast<EditorT7*>(m_abstractEditor.get())->SetLiveBaseline({});
	m_retailPending.reset();
	m_t7PendingBaseline.clear();

	if (TargetsRetailT8()) {
		// Converted once off the UI thread; PollRetailImport imports the result and binds it for live edition.
		if (RetailConversionBusy()) return;
		auto bytes = std::make_shared<const std::vector<Byte>>(moveset, moveset + movesetSize);
		m_retailImportPlayer = playerid;
		m_localProcessError = "Converting the moveset for Tekken 8...";
		m_retailConversion = std::async(std::launch::async, [bytes] { return std::make_unique<T7RetailLive>(*bytes); });
		return;
	}
	// The editor keeps the imported bytes: an Apply writes the rows that changed since (Live_OnApplied)
	m_t7PendingBaseline.assign(moveset, moveset + movesetSize);
	QueueImport(playerid, moveset, movesetSize);
}

bool EditorVisuals_T7::TargetsRetailT8() const
{
	const auto* game = m_importerHelper->currentGame;
	return game && game->gameId == GameId_T8 && game->minorVersion == GameVersions::T8::RETAIL;
}

void EditorVisuals_T7::PollRetailImport()
{
	auto* editor = static_cast<EditorT7*>(m_abstractEditor.get());
	if (m_retailConversion.valid() && m_retailConversion.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		try {
			auto live = m_retailConversion.get();
			const auto& container = live->Container();
			const auto player = m_retailImportPlayer;
			// m_loadedMoveset receives the currently selected player's moveset: live edition follows that player.
			const bool followed = player < 0 || player == static_cast<int>(m_importerHelper->currentPlayerId);
			m_retailPendingSlot = static_cast<uint8_t>(m_importerHelper->currentPlayerId);
			QueueImport(player, container.data(), container.size());
			if (followed) m_retailPending = std::move(live);
			m_localProcessError.clear();
		} catch (const std::exception& error) {
			m_localProcessError = std::string("T7 to T8 conversion failed: ") + error.what();
			m_importNeeded = true;
		}
	}
	if (!m_t7PendingBaseline.empty() && m_loadedMoveset != 0 && !TargetsRetailT8()) {
		editor->SetLiveBaseline(std::move(m_t7PendingBaseline));
		m_t7PendingBaseline.clear();
	}
	if (m_retailPending && m_loadedMoveset != 0 && TargetsRetailT8()) {
		m_retailPending->Bind(m_loadedMoveset, m_retailPendingSlot, m_importerHelper->currentGame);
		editor->SetRetailLive(std::move(m_retailPending));
	}
}

void EditorVisuals_T7::QueueImport(int playerid, const Byte* moveset, uint64_t movesetSize)
{
	if (playerid == 0 || playerid == 1) {
		// Import on player 0 OR player 1
		m_importerHelper->QueueCharacterImportation(playerid, moveset, movesetSize, ImportSettings_DEFAULT, playerid == m_importerHelper->currentPlayerId  ? &m_loadedMoveset : nullptr);
	}
	else if (playerid == -2) {
		// Import on currently selected player
		m_importerHelper->QueueCharacterImportation(moveset, movesetSize, ImportSettings_DEFAULT, &m_loadedMoveset);
	}
	else {
		// Import on both players
		gameAddr* out_moveset1 = m_importerHelper->currentPlayerId == 0 ? &m_loadedMoveset : nullptr;
		gameAddr* out_moveset2 = m_importerHelper->currentPlayerId == 1 ? &m_loadedMoveset : nullptr;
		m_importerHelper->QueueCharacterImportationOnBothPlayers(moveset, movesetSize, ImportSettings_DEFAULT, out_moveset1, out_moveset2);
	}
}

// -- Public methods -- //

EditorVisuals_T7::EditorVisuals_T7(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
	: EditorVisuals(movesetInfo, addrFile, storage)
{
	m_editor = static_cast<EditorT7*>(m_abstractEditor.get());

	Byte* moveset;
	uint64_t movesetSize;
	try {
		moveset = Helpers::ReadMovesetFile(movesetInfo->filename, movesetSize);
	} catch (const std::exception&) {
		popen = false;
		return;
	}

	TKMovesetCompressionType_ compressionType = TKMovesetCompressionType_None;

	if (movesetSize >= sizeof(TKMovesetHeader)) {
		compressionType = (TKMovesetCompressionType_)((TKMovesetHeader*)moveset)->compressionType;
	}

	if (!m_editor->LoadMoveset(moveset, movesetSize)) {
		this->popen = false;
		return;
	}

	PopulateWindowCreatorMap();
	// Read what needs to be read and potentially displayed right away
	m_editor->ReloadDisplayableMoveList();
	movelist = &m_editor->displayableMovelist;
	ReloadMovelistFilter();

	m_compressionIndex = CompressionUtils::GetDefaultCompressionSetting();

	for (auto& tab: m_movelistTabs) {
		tab.label = _(tab.label.c_str());
	}
}

void EditorVisuals_T7::ReloadMovelistFilter()
{
	FilterMovelist(m_movelistFilter);
}

struct EditorVisuals_T7::ViewState final : EditorForm::TransactionState {
    std::unique_ptr<EditorForm::TransactionState> base;
    EditorVisuals_T7& owner;
    std::vector<DisplayableMove*> filtered;
    EditorVisualsT7::EditorMovelistFilter_ filter;
    int32_t scroll;
    int16_t highlighted;
    bool committed = false;
    explicit ViewState(EditorVisuals_T7& target)
        : base(target.EditorVisuals::BeginWindowTransaction()), owner(target), filtered(target.m_filteredMovelist),
          filter(target.m_movelistFilter), scroll(target.m_moveToScrollTo), highlighted(target.m_highlightedMoveId) {}
    ~ViewState() override {
        if (committed) return;
        filtered.swap(owner.m_filteredMovelist); std::swap(filter, owner.m_movelistFilter);
        std::swap(scroll, owner.m_moveToScrollTo); std::swap(highlighted, owner.m_highlightedMoveId);
    }
    void Commit() noexcept override { base->Commit(); committed = true; }
};
std::unique_ptr<EditorForm::TransactionState> EditorVisuals_T7::BeginWindowTransaction() {
    return std::make_unique<ViewState>(*this);
}
