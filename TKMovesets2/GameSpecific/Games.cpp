#include "Games.hpp"
#include "BaseAddresses.hpp"


#include "Extractor_t7/Extractor_t7.hpp"
#include "Extractor_ttt2/Extractor_ttt2.hpp"
#include "Extractor_t6/Extractor_t6.hpp"
#include "Extractor_t5/Extractor_t5.hpp"

#include "Importer_t7/Importer_t7.hpp"

#include "Editor_t7/Editor_t7.hpp"

#include "EditorVisuals_t7/EditorVisuals_t7.hpp"

#include "Online_t7.hpp"

// -- -- //

const GameInfo cg_gamesInfo[] = {
	{
		.name = "Tekken 8 CNT",
		.processName = "Tekken8.exe",
		.movesetNamePrefix = "T8_",
		.gameId = GameId_T8,
		.minorVersion = GameVersions::T8::CLOSED_NETWORK_TEST,
		.characterCount = 2,
		.ptrSize = 8,
		.flags = 0,
		.dataString = "t8",
		.minorDataString = "t8",
		.supportedImports = { },
		.supportedOnlineImports = { },
		.extractor = nullptr,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetBaseAddrFromFile,
		.bigEndian = false,
		.autoProcessSelection = true,
	},
	{
		.name = "Tekken 7",
		.processName = "TekkenGame-Win64-Shipping.exe",
		.movesetNamePrefix = "T7_",
		.gameId = GameId_T7,
		.minorVersion = GameVersions::T7::NONE,
		.characterCount = 2,
		.ptrSize = 8,
		.flags = GameFlag_MovesetLiveEditable,
		.dataString = "t7",
		.minorDataString = "t7",
		.supportedImports = { GameId_TTT2, GameId_TREV, GameId_T6, GameId_T5 },
		.supportedOnlineImports = { GameId_TTT2, GameId_TREV, GameId_T6 },
		.extractor = new GameFactory<ExtractorT7>,
		.importer = new GameFactory<ImporterT7>,
		.editorLogic = new GameFactory<EditorT7>,
		.editorVisuals = new EditorFactory<EditorVisuals_T7>,
		.onlineHandler = new GameFactory<OnlineT7>,
		.GetBaseAddressFunc = GetBaseAddrFromFile,
		.bigEndian = false,
		.autoProcessSelection = true,
	},
	{
		.name = "Tekken Tag 2 1.00 (RPCS3)",
		.processName = "rpcs3.exe",
		.movesetNamePrefix = "TTT2_",
		.gameId = GameId_TTT2,
		.minorVersion = GameVersions::TTT2::RPCS3_BLES01702_0100,
		.characterCount = 4,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "ttt2",
		.minorDataString = "ttt2",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorTTT2>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetRPCS3BaseAddr,
		.bigEndian = true,
		.autoProcessSelection = false
	},
	{
		.name = "Tekken Tag 2 1.03 (RPCS3)",
		.processName = "rpcs3.exe",
		.movesetNamePrefix = "TTT2_",
		.gameId = GameId_TTT2,
		.minorVersion = GameVersions::TTT2::RPCS3_BLES01702_0103,
		.characterCount = 4,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "ttt2",
		.minorDataString = "ttt2-1-03",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorTTT2>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetRPCS3BaseAddr,
		.bigEndian = true,
		.autoProcessSelection = true
	},
	{
		.name = "Tekken Revolution (RPCS3)",
		.processName = "rpcs3.exe",
		.movesetNamePrefix = "TREV_",
		.gameId = GameId_TREV,
		.minorVersion = 0,
		.characterCount = 2,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "trev",
		.minorDataString = "trev",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorTTT2>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetRPCS3BaseAddr,
		.bigEndian = true,
		.autoProcessSelection = false
	},
	{
		.name = "Tekken 6 (RPCS3)",
		.processName = "rpcs3.exe",
		.movesetNamePrefix = "T6_",
		.gameId = GameId_T6,
		.minorVersion = GameVersions::T6::RPCS3_BLES00635,
		.characterCount = 2,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "t6",
		.minorDataString = "t6",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorT6>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetRPCS3BaseAddr,
		.bigEndian = true,
		.autoProcessSelection = false
	},
	{
		.name = "Tekken 6 (PPSSPP)",
		.processName = "PPSSPPWindows64.exe",
		.movesetNamePrefix = "T6_",
		.gameId = GameId_T6,
		.minorVersion = GameVersions::T6::PPSSPP_ULUS10466,
		.characterCount = 2,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "t6",
		.minorDataString = "t6-psp",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorT6>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetPPSSPPBaseAddr,
		.bigEndian = false,
		.autoProcessSelection = true
	},
	/*
	{
		.name = "Tekken 5 DR (RPCS3)",
		.processName = "rpcs3.exe",
		.movesetNamePrefix = "T5_",
		.gameId = GameId_T5,
		.minorVersion = GameVersions::T5::DR_ONLINE,
		.characterCount = 2,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "t5",
		.minorDataString = "t5dr",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorT5>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetRPCS3BaseAddr,
		.bigEndian = true,
		.autoProcessSelection = false
	},
	*/
	{
		.name = "Tekken 5 (PCSX2)",
		.processName = "pcsx2.exe",
		.movesetNamePrefix = "T5_",
		.gameId = GameId_T5,
		.minorVersion = GameVersions::T5::DEFAULT,
		.characterCount = 2,
		.ptrSize = 4,
		.flags = 0,
		.dataString = "t5",
		.minorDataString = "t5",
		.supportedImports = {},
		.supportedOnlineImports = {},
		.extractor = new GameFactory<ExtractorT5>,
		.importer = nullptr,
		.editorLogic = nullptr,
		.editorVisuals = nullptr,
		.onlineHandler = nullptr,
		.GetBaseAddressFunc = GetPCSX2BaseAddr,
		.bigEndian = false,
		.autoProcessSelection = true
	},
};

namespace Games
{
	const GameInfo* GetGameInfoFromIndex(unsigned int idx)
	{
		return (GameInfo*)&cg_gamesInfo[idx];
	}

	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId)
	{
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].gameId == gameId)
			{
				return &cg_gamesInfo[i];
			}
		}
		return nullptr;
	}

	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId, uint16_t minorVersion)
	{
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].gameId == gameId && cg_gamesInfo[i].minorVersion == minorVersion)
			{
				return &cg_gamesInfo[i];
			}
		}
		return nullptr;
	}

	unsigned int GetGamesCount()
	{
		return (int)_countof(cg_gamesInfo);
	}

	//

	unsigned int GetExtractableGamesCount()
	{
		unsigned int count = 0;
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].extractor != nullptr) {
				++count;
			}
		}
		return count;
	}

	unsigned int GetImportableGamesCount()
	{
		unsigned int count = 0;
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].importer != nullptr) {
				++count;
			}
		}
		return count;
	}

	//

	bool IsGameEditable(uint16_t gameId, uint16_t minorVersion)
	{
		auto game = GetGameInfoFromIdentifier(gameId, minorVersion);
		return game->IsEditable();
	}

	bool IsGameLiveEditable(uint16_t gameId, uint16_t minorVersion)
	{
		auto game = GetGameInfoFromIdentifier(gameId, minorVersion);
		return game && (game->flags & GameFlag_MovesetLiveEditable) != 0;
	}

	//
	
	Extractor* FactoryGetExtractor(const GameInfo* gameInfo, GameProcess& process, GameData& game)
	{
		Extractor* ex = (Extractor*)gameInfo->extractor->allocate(process, game, gameInfo);
		return ex;
	}

	Importer* FactoryGetImporter(const GameInfo* gameInfo, GameProcess& process, GameData& game)
	{
		Importer* im = (Importer*)gameInfo->importer->allocate(process, game, gameInfo);
		return im;
	}

	EditorLogic* FactoryGetEditorLogic(const GameInfo* gameInfo, GameProcess& process, GameData& game)
	{
		EditorLogic* ed = (EditorLogic*)gameInfo->editorLogic->allocate(process, game, gameInfo);
		return ed;
	}

	EditorVisuals* FactoryGetEditorVisuals(const GameInfo* gameInfo, const movesetInfo* movesetInfos, GameAddressesFile* addrFile, LocalStorage* storage)
	{
		EditorVisuals* ed = (EditorVisuals*)gameInfo->editorVisuals->allocate(movesetInfos, addrFile, storage);
		return ed;
	}

	Online* FactoryGetOnline(const GameInfo* gameInfo, GameProcess& process, GameData& game)
	{
		Online* on = (Online*)gameInfo->onlineHandler->allocate(process, game, gameInfo);
		return on;
	}
};