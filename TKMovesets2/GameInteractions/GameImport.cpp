#include "GameImport.hpp"
#include "Games.hpp"
#include "Importer.hpp"
#include "Importer_t8/Importer_t8.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameTypes.h"

/// Class storing an importation queue and an Importer* instance to feed it to

// -- Private methods -- //

void GameImport::OnProcessAttach()
{
	auto state = LockState();
	InstantiateFactory();

	m_plannedImportations.clear();
}

void GameImport::OnProcessDetach()
{
	auto state = LockState();
}

void GameImport::InstantiateFactory()
{
	auto state = LockState();
	m_importer.reset();

	game.SetCurrentGame(currentGame);
	// Every game has its own importation subtleties so we use polymorphism to manage that
	m_importer.reset(Games::FactoryGetImporter(currentGame, process, game));
}

void GameImport::PreProcessDetach()
{
	auto state = LockState();
	if (m_importer)
	{
		m_importer->CleanupUnusedMovesets();
	}
}

void GameImport::RunningUpdate()
{
	auto state = LockState();
	while (true) {
        if (m_plannedImportations.empty()) return;
        auto request = m_plannedImportations.front();
        uint8_t operationProgress = 0;
        progress = 0;
        auto error = RunOutsideState(state, [&] {
            auto result = request.moveset
                ? m_importer->Import(request.moveset->data(), request.moveset->size(), request.playerAddress, request.settings, operationProgress)
                : m_importer->Import(request.filename.c_str(), request.playerAddress, request.settings, operationProgress);
            if (request.settings & ImportSettings_FreeUnusedMovesets) m_importer->CleanupUnusedMovesets();
            return result;
        });
        progress = operationProgress;

		if (request.out_moveset) *request.out_moveset = error == ImportationErrcode_Successful ? m_importer->lastLoaded.address : 0;
		if (error != ImportationErrcode_Successful) {
			m_errors.emplace_back(error,m_importer->LastError());
			m_plannedImportations.clear();
			return;
		}
		if (const auto* retail = dynamic_cast<const ImporterT8*>(m_importer.get())) {
			// Lenient import: animations found neither in the file nor in the game's archive (memory-only), or that the
			// target's own moveset does not play (target fallback), play the idle one.
			const auto idle = retail->MemoryOnlyFallbackMoves() + (retail->TargetFallback() ? retail->BindingFallbackMoves() : 0);
			if (idle) m_warning = {idle, retail->UnavailableAnimations().size(), retail->ArchiveError(), retail->TargetFallback()};
		}
		m_plannedImportations.erase(m_plannedImportations.begin());
	}
}

// -- Public methods -- //

ImportationErrcode_ GameImport::GetLastError(std::string* detail)
{
	auto state = LockState();

	if (m_errors.size() > 0) {
		ImportationErrcode_ err = m_errors[0].first;
        if (detail) *detail = m_errors[0].second;
		m_errors.erase(m_errors.begin());
		return err;
	}
	return ImportationErrcode_Successful;
}

bool GameImport::GetLastWarning(ImportWarning& warning)
{
	auto state = LockState();
	if (!m_warning.fallbackMoves) return false;
	warning = std::move(m_warning);
	m_warning = {};
	return true;
}

void GameImport::StopThreadAndCleanup()
{
	// Order thread to stop
	ThreadedClass::StopThreadAndCleanup();
	auto state = LockState();

	if (m_importer != nullptr) {
		if (process.IsAttached()) {
			m_importer->CleanupUnusedMovesets();
		}
		m_importer.reset();
	}

	m_plannedImportations.clear();
}

bool GameImport::CanStart(bool cached) const
{
	auto state = LockState();
	if (m_importer == nullptr) {
		return false;
	}
	if (cached) {
		return m_canStart;
	}
	// Per-game definition
	return m_importer->CanImport();
}

bool GameImport::IsBusy() const
{
	auto state = LockState();
	// There are still movesets to import

	return m_executingOperation || m_plannedImportations.size() > 0;
}

void GameImport::QueueCharacterImportationOnBothPlayers(const Byte* moveset, uint64_t movesetSize, ImportSettings settings, gameAddr* out_moveset1, gameAddr* out_moveset2)
{
	auto state = LockState();
	if (m_executingOperation || !m_importer) return;

	gameAddr playerAddress_1 = m_importer->GetCharacterAddress(currentPlayerId);
	gameAddr playerAddress_2 = m_importer->GetCharacterAddress(!currentPlayerId);

	if (!moveset || !movesetSize) return;
	auto data = std::make_shared<const std::vector<Byte>>(moveset, moveset + movesetSize);
	m_plannedImportations.push_back({
		.moveset = data,
		.filename = L"",
		.playerAddress = playerAddress_1,
		.settings = settings,
		.out_moveset = out_moveset1
		});

	m_plannedImportations.push_back({
		.moveset = data,
		.filename = L"",
		.playerAddress = playerAddress_2,
		.settings = settings,
		.out_moveset = out_moveset2
		});
}

void GameImport::QueueCharacterImportation(int playerid, const Byte* moveset, uint64_t movesetSize, ImportSettings settings, gameAddr* out_moveset)
{
	auto state = LockState();
	if (m_executingOperation || !m_importer) return;

	if (!moveset || !movesetSize || playerid < 0 || playerid >= GetCharacterCount()) return;
	gameAddr playerAddress = m_importer->GetCharacterAddress(playerid);
	m_plannedImportations.push_back({
		.moveset = std::make_shared<const std::vector<Byte>>(moveset, moveset + movesetSize),
		.filename = L"",
		.playerAddress = playerAddress,
		.settings = settings,
		.out_moveset = out_moveset
		});
}

void GameImport::QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings, gameAddr* out_moveset)
{
	auto state = LockState();
	QueueCharacterImportation(currentPlayerId, moveset, movesetSize, settings, out_moveset);
}

void GameImport::QueueCharacterImportation(std::wstring filename, ImportSettings settings, gameAddr* out_moveset)
{
	auto state = LockState();
	if (m_executingOperation || !m_importer) return;

	gameAddr playerAddress = m_importer->GetCharacterAddress(currentPlayerId);
	m_plannedImportations.push_back({
		.moveset = nullptr,
		.filename = filename,
		.playerAddress = playerAddress,
		.settings = settings,
		.out_moveset = out_moveset
	});
}

gameAddr GameImport::GetCurrentPlayerMovesetAddr() const
{
	auto state = LockState();
	// todo: If the importer gets deallocated while we do this, things can go bad
	if (!m_executingOperation && m_importer != nullptr) {
		return m_importer->GetMovesetAddress(currentPlayerId);
	}
	return 0;
}

void GameImport::FreeExpiredFactoryClasses()
{
    // Factory lifetime is synchronized with the worker and rendering frame.
}
