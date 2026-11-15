#include <ImGui.h>

#include "Submenu_Extra.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"
#include "GameStaticData.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "Online_t8.hpp"
#include "SharedMemory_t8.h"
#include "constants.h"

#include <algorithm>

namespace {
constexpr const char* T8_PROCESS = "Polaris-Win64-Shipping.exe";
}

Submenu_Extra::Submenu_Extra() = default;
Submenu_Extra::~Submenu_Extra() = default;

void Submenu_Extra::LoadFighters()
{
	// The running game's tkdata.bin decides which fighters exist and their in-match ids (motbin header identities);
	// the names file only labels them. Without a running game: the names file's playable roster, retried every 2 s
	// until the game is found (then the archive roster replaces it, keeping the picks).
	if (m_fromArchive) return;
	const ULONGLONG now = GetTickCount64();
	if (!m_fighters.empty() && now - m_rosterCheckedAt < 2000) return;
	m_rosterCheckedAt = now;
	const auto data = GameStaticData::LoadShared("t8-retail");
	std::vector<Fighter> fighters;
	bool fromArchive = false;
	if (const auto exe = RetailT8GameData::RunningGameExecutable()) {
		for (const auto& f : RetailT8GameData::TkdataRoster(RetailT8GameData::TkdataPath(*exe))) {
			const auto* c = data ? data->FindCharacter(f.code) : nullptr;
			fighters.push_back({ c && !c->name.empty() ? c->name : f.code, (int)f.id });
		}
		fromArchive = !fighters.empty();
	}
	if (fighters.empty() && !m_fighters.empty()) return;     // still no game: keep the names-file list
	if (fighters.empty() && data) for (const auto& c : data->Characters()) {
		if (c.playable != true || c.name.empty() || !(c.runtimeId || c.id)) continue;
		fighters.push_back({ c.name, (int)c.runtimeId.value_or(*c.id) });
	}
	if (fighters.empty()) {
		// neither the game nor the names data (should not happen: the names file is embedded in the exe)
		static const Fighter builtIn[] = {
			{ "Alisa Bosconovitch", 18 }, { "Anna Williams", 37 }, { "Armor King", 39 }, { "Asuka Kazama", 11 },
			{ "Azucena", 29 }, { "Bob", 42 }, { "Bryan Fury", 7 }, { "Claudio Serafino", 19 }, { "Clive Rosfield", 36 },
			{ "Devil Jin", 12 }, { "Eddy Gordo", 33 }, { "Fahkumram", 38 }, { "Feng Wei", 13 },
			{ "Heihachi Mishima", 35 }, { "Hwoarang", 4 }, { "Jack-8", 10 }, { "Jin Kazama", 6 }, { "Jun Kazama", 27 },
			{ "Kazuya Mishima", 8 }, { "King", 2 }, { "Kuma", 23 }, { "Kunimitsu", 41 }, { "Lars Alexandersson", 17 },
			{ "Lee Chaolan", 22 }, { "Leo", 16 }, { "Leroy Smith", 26 }, { "Lidia Sobieska", 34 }, { "Lili", 14 },
			{ "Ling Xiaoyu", 5 }, { "Marshall Law", 1 }, { "Miary Zo", 40 }, { "Nina Williams", 21 }, { "Panda", 24 },
			{ "Paul Phoenix", 0 }, { "Raven", 31 }, { "Reina", 28 }, { "Roger Jr.", 43 }, { "Sergei Dragunov", 15 },
			{ "Shaheen", 20 }, { "Steve Fox", 9 }, { "Victor Chevalier", 30 }, { "Yoshimitsu", 3 }, { "Zafina", 25 },
		};
		fighters.assign(std::begin(builtIn), std::end(builtIn));
		m_namesMissing = true;
	}
	std::sort(fighters.begin(), fighters.end(), [](const Fighter& a, const Fighter& b) { return a.label < b.label; });
	// keep the current picks (by fighter id) across the switch to the archive roster; first load: P3 Dragunov, P4 none
	const int p3 = m_fighters.empty() ? 15 : m_fighters[m_p3].id;
	const int p4 = m_fighters.empty() ? -1 : (m_p4 == 0 ? -1 : m_fighters[m_p4 - 1].id);
	m_fighters = std::move(fighters);
	m_fromArchive = fromArchive;
	m_p3 = 0; m_p4 = 0;
	for (size_t i = 0; i < m_fighters.size(); ++i) {
		if (m_fighters[i].id == p3) m_p3 = (int)i;
		if (m_fighters[i].id == p4) m_p4 = (int)i + 1;
	}
}

bool Submenu_Extra::Attach(GameAddressesFile& addresses)
{
	if (m_process.IsAttached() && m_process.CheckRunning()) return true;
	m_loader.reset();
	m_process.Detach();
	if (!m_process.Attach(T8_PROCESS, PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD)) return false;
	const GameInfo* info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
	if (!info) return false;
	m_game.SetProcess(&m_process, &addresses);
	m_game.SetCurrentGame(info);
	m_loader = std::make_unique<OnlineT8>(m_process, m_game, info);
	return true;
}

void Submenu_Extra::Send(int command)
{
	m_pendingCommand = command;
	m_message.clear();
}

void Submenu_Extra::Update()
{
	if (!m_loader) return;
	if (!m_process.CheckRunning()) { m_loader.reset(); m_process.Detach(); m_pendingCommand = 0; return; }
	if (!m_loader->IsMemoryLoaded()) {
		if (m_loader->LoadSharedMemory()) m_loader->VerifyDllVersionMismatch();
		else if (m_pendingCommand && !m_loader->isInjecting) {
			// two attempts: the first injection into a freshly started game has failed without a trace (2026-11-14)
			if (m_injectionAttempts < 2) { m_loader->InjectDll(); m_injectionStarted = true; ++m_injectionAttempts; }
			else { m_message = _("extra.tag.inject_failed"); m_pendingCommand = 0; m_injectionStarted = false; m_injectionAttempts = 0; }
		}
		if (!m_loader->IsMemoryLoaded()) return;
	}
	m_injectionStarted = false;
	m_injectionAttempts = 0;
	if (m_loader->versionMismatch) { m_message = _("extra.tag.version_mismatch"); m_pendingCommand = 0; return; }
	auto mem = m_loader->SharedMemory();
	if (!mem || !m_pendingCommand) return;
	if (mem->moveset_loader_version[0] == '\0') return;   // loader still initializing
	mem->tag_command = m_pendingCommand;
	mem->tag_p3_fighter = m_fighters[m_p3].id;
	mem->tag_p4_fighter = m_p4 == 0 ? -1 : m_fighters[m_p4 - 1].id;
	MemoryBarrier();
	m_requestId = (std::max)(m_requestId, mem->tag_request_id) + 1;
	mem->tag_request_id = m_requestId;
	m_pendingCommand = 0;
}

void Submenu_Extra::Render(GameAddressesFile& addresses)
{
	LoadFighters();
	Update();

	ImGui::SeparatorText(_("extra.tag.title"));
	ImGuiExtra::RenderTextbox(_("extra.tag.explanation"));
	ImGui::Spacing();

	auto combo = [&](const char* label, int& index, bool withNone) {
		const char* preview = withNone && index == 0 ? _("extra.tag.none") : m_fighters[withNone ? index - 1 : index].label.c_str();
		ImGui::SetNextItemWidth(260);
		if (ImGui::BeginCombo(label, preview)) {
			if (withNone && ImGui::Selectable(_("extra.tag.none"), index == 0)) index = 0;
			for (size_t i = 0; i < m_fighters.size(); ++i) {
				const int value = (int)i + (withNone ? 1 : 0);
				if (ImGui::Selectable(m_fighters[i].label.c_str(), index == value)) index = value;
			}
			ImGui::EndCombo();
		}
	};
	combo(_("extra.tag.p3"), m_p3, false);
	combo(_("extra.tag.p4"), m_p4, true);
	if (m_namesMissing) ImGui::TextDisabled("%s", _("extra.tag.names_missing"));
	ImGui::Spacing();

	const bool busy = m_pendingCommand != 0 || (m_loader && m_loader->isInjecting);
	// P3/P4 being added to the match (grow + per-slot setup, a few seconds): neither button until it's done
	const auto shared = m_loader && m_loader->IsMemoryLoaded() ? m_loader->SharedMemory() : nullptr;
	const bool loading = shared && shared->tag_ack_id && shared->tag_state == TagState_Loading;
	if (ImGuiExtra::RenderButtonEnabled(_("extra.tag.load"), !busy && !loading, ImVec2(260, 0))) {
		if (Attach(addresses)) Send(TagCommand_Enable);
		else m_message = _("extra.tag.no_game");
	}
	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("extra.tag.disable"), !busy && !loading && m_loader && m_loader->IsMemoryLoaded())) Send(TagCommand_Disable);

	ImGui::Spacing();
	if (!m_message.empty()) ImGui::TextWrapped("%s", m_message.c_str());
	else if (busy) ImGui::TextUnformatted(_("extra.tag.injecting"));
	else if (auto mem = m_loader ? m_loader->SharedMemory() : nullptr; mem && mem->tag_ack_id) {
		char status[sizeof(mem->tag_status) + 1] = {};
		memcpy(status, mem->tag_status, sizeof(mem->tag_status));
		const bool error = mem->tag_state == TagState_Error;
		if (error) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 110, 110, 255));
		ImGui::TextWrapped("%s", status);
		if (error) ImGui::PopStyleColor();
	}
}
