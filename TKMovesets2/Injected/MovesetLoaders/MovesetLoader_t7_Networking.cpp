#include "MovesetLoader_t7.hpp"
#include "steamHelper.hpp"
#include "Compression.hpp"

#include "steam_api.h"

// -- -- //

bool PacketT7::IsValidSize(uint32_t packetSize) const
{
	switch (packetType)
	{
	case PacketT7Type_RequestSync:
		return packetSize == sizeof(PacketT7_RequestMovesetSync);
	case PacketT7Type_AnswerSync:
		return packetSize == sizeof(PacketT7_AnswerMovesetSync);
	case PacketT7Type_Ready:
		return packetSize == sizeof(PacketT7_Ready);
	default:
		return false;
	}
}

CSteamID MovesetLoaderT7::GetLobbyOpponentSteamId() const
{
	// Note that this function is highly likely to return the wrong player ID in lobbies with more than two players
	// This is because the player order here does not match the displayed player order
	CSteamID lobbyID = CSteamID(ReadVariable<uint64_t>("gTK_roomId_addr"));
	CSteamID mySteamID = SteamHelper::SteamUser()->GetSteamID();

	unsigned int lobbyMemberCount = SteamHelper::SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
	for (unsigned int i = 0; i < lobbyMemberCount; ++i)
	{
		CSteamID memberId = SteamHelper::SteamMatchmaking()->GetLobbyMemberByIndex(lobbyID, i);
		if (memberId != mySteamID) {
			return memberId;
		}
	}
	return k_steamIDNil;
}

// -- -- //

bool MovesetLoaderT7::SendPacket(void* packetBuffer, uint32_t packetSize, bool sendUnreliable)
{
	DEBUG_LOG("SendPacket(%u)\n", packetSize);
	return SteamHelper::SteamNetworking()->SendP2PPacket(opponentId, packetBuffer, packetSize, sendUnreliable ? k_EP2PSendUnreliable : k_EP2PSendReliable, MOVESET_LOADER_P2P_CHANNEL);
}

void MovesetLoaderT7::OnCommunicationPacketReceive(const PacketT7* packet)
{
	if (!sharedMemPtr->locked_in) {
		DEBUG_LOG("COMMUNICATION: not locked in, ignoring.\n");
		return;
	}

	if (sharedMemPtr->moveset_sync_status == MovesetSyncStatus_NotStarted ||
		sharedMemPtr->moveset_sync_status == MovesetSyncStatus_AcceptPackets) {
		DEBUG_LOG("COMMUNICATION: Sync not started, ignoring packet.\n");
		return;
	}

	switch (packet->packetType)
	{
	default:
		DEBUG_LOG("!! COMMUNICTION: Unknown packet type %u !!\n", packet->packetType);
		break;
	case PacketT7Type_Ready:
		{
			// If we received a packet from the opponnent telling us that they are ready
			if (sharedMemPtr->moveset_sync_status == MovesetSyncStatus_DownloadingMoveset || sharedMemPtr->moveset_sync_status == MovesetSyncStatus_AwaitingReady)
			{
				DEBUG_LOG("COMMUNICATION: PACKET READY RECEIVED\n");
				syncStatus.opponent_ready = true;
				if (syncStatus.CanStart()) {
					DEBUG_LOG("-- CanStart() = true --\n");
					sharedMemPtr->moveset_sync_status = MovesetSyncStatus_Ready;
				}
			}
			else {
				DEBUG_LOG("COMMUNICATION: READY received but moveset_sync_status was incorrect (%u). Ignoring.\n", sharedMemPtr->moveset_sync_status);
			}
			break;

		}
	case PacketT7Type_RequestSync:
		{
			if (sharedMemPtr->moveset_sync_status != MovesetSyncStatus_RequestSync) {
				DEBUG_LOG("COMMUNICATION: PACKET REQUEST_SYNC received but was not expected. Ignoring.\n");
				return;
			}

			DEBUG_LOG("COMMUNICATION: PACKET REQUEST_SYNC RECEIVED\n");
			// Received a packet from the opponent providing us with informations about what he wants

			auto _packet = (PacketT7_RequestMovesetSync*)packet;

			if (_packet->local_moveset.size == 0)
			{
				sharedMemPtr->players[1].custom_moveset_addr = 0;
				sharedMemPtr->moveset_sync_status = MovesetSyncStatus_AwaitingReady;

				DEBUG_LOG("COMMUNICATION: Remote moveset is empty. Sending ready packet.\n");
				// Remote moveset is empty so no need to load it, we are ready

				// Tell the opponent we are ready to start
				PacketT7_Ready newPacket;
				if (!SendPacket(&newPacket, sizeof(PacketT7_Ready))) {
					DEBUG_LOG("COMMUNICATION: Failed to Ready in RequestSync\n");
					sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
					return;
				} else {
					DEBUG_LOG("COMMUNICATION: Sending READY.\n");
				}
				syncStatus.loaded_remote_moveset = true;

				if (syncStatus.CanStart()) {
					DEBUG_LOG("-- CanStart() = true --\n");
					sharedMemPtr->moveset_sync_status = MovesetSyncStatus_Ready;
				}
			}
			else {
				DEBUG_LOG("COMMUNICATION: Remote moveset is %llu bytes big.\n", _packet->local_moveset.size);
				PacketT7_AnswerMovesetSync newPacket;
				newPacket.requesting_download = true;
				
				if (_packet->local_moveset.crc32 == sharedMemPtr->players[1].crc32) {
					newPacket.requesting_download = false;
				}
				else if (false) {
					// todo: list movesets in sharedMemPtr->program_path, check their crc32, load them if needed
				}

				if (_packet->local_moveset.size >= ONLINE_MOVESET_MAX_SIZE_BYTES) {
					DEBUG_LOG("- Not downloading : Moveset way too big. -\n");
					sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
					return;
				}

				if (newPacket.requesting_download) {
					incoming_moveset.size = _packet->local_moveset.size;
					incoming_moveset.remaining_bytes = incoming_moveset.size;
					incoming_moveset.data = new Byte[incoming_moveset.size];
					incoming_moveset.cursor = incoming_moveset.data;
					sharedMemPtr->players[1].custom_moveset_addr = 0;
				}

				if (SendPacket(&newPacket, sizeof(newPacket)))
				{
					if (newPacket.requesting_download) {
						DEBUG_LOG("COMMUNICATION: Requesting opponent's moveset.\n");
						// We are awaiting the opponent's moveset
						sharedMemPtr->moveset_sync_status = MovesetSyncStatus_DownloadingMoveset;
					}
					else {
						DEBUG_LOG("COMMUNICATION: Opponent moveset was already found locally, not requesting it. Sending ready packet.\n");
						// We are not awaiting any moveset, we can simply reuse the old one
						// Wait for the opponent to be marked as ready
						sharedMemPtr->moveset_sync_status = MovesetSyncStatus_AwaitingReady;

						// Tell the opponent we are ready to start
						PacketT7_Ready newPacket;
						if (!SendPacket(&newPacket, sizeof(PacketT7_Ready))) {
							DEBUG_LOG("COMMUNICATION: Failed to Ready in AnswerSync\n");
							sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
							delete[] incoming_moveset.data;
							incoming_moveset.data = 0;
							return;
						} else {
							DEBUG_LOG("COMMUNICATION: Sending READY.\n");
						}
						syncStatus.loaded_remote_moveset = true;

						if (syncStatus.CanStart()) {
							DEBUG_LOG("-- CanStart() = true --\n");
							sharedMemPtr->moveset_sync_status = MovesetSyncStatus_Ready;
						}
					}
				} else {
					DEBUG_LOG("COMMUNICATION: Failed to AnswerMovesetSync\n");
					sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
				}
			}
		}
		break;
	case PacketT7Type_AnswerSync:
		{
			auto _packet = (PacketT7_AnswerMovesetSync*)packet;
			if (_packet->requesting_download && sharedMemPtr->players[0].custom_moveset_addr != 0) {
				DEBUG_LOG("COMMUNICATION: ANSWER_MOVESET_SYNC, sending our moveset\n");
				SendMoveset();
			}
			else {
				DEBUG_LOG("COMMUNICATION: ANSWER_MOVESET_SYNC , not sending moveset\n");
			}
		}
		break;
	}
}

void MovesetLoaderT7::OnMovesetBytesReceive(const Byte* buf, uint32_t bufSize)
{
	DEBUG_LOG("OnMovesetBytesReceive() %u bytes\n", bufSize);

	if (incoming_moveset.remaining_bytes < bufSize) {
		// Received too much data
		sharedMemPtr->moveset_sync_status = MovesetSyncStatus_AcceptPackets;
		DEBUG_LOG("PACKET: Received too much data.\n");
		return;
	}

	memcpy(incoming_moveset.cursor, buf, bufSize);
	incoming_moveset.cursor += bufSize;
	incoming_moveset.remaining_bytes -= bufSize;

	if (incoming_moveset.remaining_bytes == 0)
	{
		DEBUG_LOG("PACKET: Successfully received entire moveset!\n");

		auto& player = sharedMemPtr->players[1];

		incoming_moveset.data = ImportForOnline(sharedMemPtr->players[1], incoming_moveset.data, incoming_moveset.size);

		if (incoming_moveset.data != nullptr)
		{
			if (incoming_moveset.prev_moveset != 0) {
				DEBUG_LOG("Freeing old moveset.\n");
				delete[] incoming_moveset.prev_moveset;
			}
			incoming_moveset.prev_moveset = incoming_moveset.data;

			player.custom_moveset_addr = (uint64_t)incoming_moveset.data;
			player.is_initialized = false;

			// Tell the opponent we are ready to start
			PacketT7_Ready newPacket;
			if (!SendPacket(&newPacket, sizeof(PacketT7_Ready))) {
				DEBUG_LOG("COMMUNICATION: Failed to Ready in MovesetReception\n");
				player.custom_moveset_addr = 0;
				sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
				delete[] incoming_moveset.data;
				return;
			}
			else {
				DEBUG_LOG("COMMUNICATION: Sending READY.\n");
			}

			syncStatus.loaded_remote_moveset = true;

			if (syncStatus.CanStart()) {
				DEBUG_LOG("-- CanStart() = true --\n");
				sharedMemPtr->moveset_sync_status = MovesetSyncStatus_Ready;
			}
		}
		else {
			DEBUG_LOG("ImportForOnline() returned nullptr: not using received moveset. (badly formated?)\n");
			sharedMemPtr->moveset_sync_status = MovesetSyncStatus_AcceptPackets;
			player.custom_moveset_addr = 0;
		}
	} else {
		DEBUG_LOG("Remaining bytes: %llu\n", incoming_moveset.remaining_bytes);
	}
}

void MovesetLoaderT7::OnPacketReceive(CSteamID senderId, const Byte* packetBuf, uint32_t packetSize)
{
	DEBUG_LOG("OnPacketReceive(%u)\n", packetSize);
	if (senderId != opponentId) {
		DEBUG_LOG("OnPacketReceive: Bad sender id, does not match opponent ID.\n");
		return;
	}

	const PacketT7* packet = (PacketT7*)packetBuf;
	if (packetSize >= sizeof(PacketT7) && packet->IsCommunicationPacket())
	{
		// Protect against packet that are too small.
		if (!packet->IsValidSize(packetSize)) {
			DEBUG_LOG("Communication packet has invalid size %u\n", packetSize);
			return;
		}
		OnCommunicationPacketReceive(packet);
	}
	else if (sharedMemPtr->moveset_sync_status == MovesetSyncStatus_DownloadingMoveset) {
		OnMovesetBytesReceive(packetBuf, packetSize);
	}
	else {
		DEBUG_LOG("OnPacketReceive: Received packet of size %u, but nothing to do with it.\n", packetSize);
	}
}

// Compress moveset to LZMA before sending
// Sending packets can be so slow at this, this becomes worth it
// to use when a moveset is loaded for online use
static bool CompressMovesetLzma(Byte*& moveset_out, uint64_t& size_out)
{
	const TKMovesetHeader* current_header = (TKMovesetHeader*)moveset_out;
	if (current_header->compressionType == TKMovesetCompressionType_LZMA) {
		DEBUG_LOG("Local moveset is already LZMA, sending as is.\n");
		return false;
	}
	DEBUG_LOG("Local moveset is not LZMA (%llu bytes), compressing.\n", size_out);

	Byte* moveset = moveset_out;
	uint64_t moveset_size = size_out;

	if (current_header->isCompressed())
	{
		// Create temp uncompressed copy
		uint64_t compressed_data_size = size_out - current_header->moveset_data_start;
		moveset_size = current_header->moveset_data_start + current_header->moveset_data_size;
		moveset = CompressionUtils::RAW::Moveset::DecompressWithHeader(moveset, compressed_data_size, moveset_size);

		DEBUG_LOG("Local moveset was compressed, its decompressed size is %llu bytes.\n", moveset_size);
	}

	// Compress to LZMA
	Byte* new_moveset = CompressionUtils::RAW::Moveset::Compress(moveset, moveset_size, TKMovesetCompressionType_LZMA, moveset_size);
	DEBUG_LOG("Local moveset now compressed to LZMA is %llu bytes.\n", moveset_size);

	if (current_header->isCompressed()) {
		// Delete the temp uncompressed copy we made
		delete[] moveset;
	}

	if (new_moveset == nullptr) {
		return false;
	}

	moveset_out = new_moveset;
	size_out = moveset_size;
	return true;
}

void MovesetLoaderT7::SendMoveset()
{
	auto& player = sharedMemPtr->players[0];

	if (player.custom_moveset_addr != 0)
	{
		Byte* dataToSend = (Byte*)player.custom_moveset_original_data_addr;
		uint64_t leftToSend = player.custom_moveset_original_data_size;

		const unsigned int bufSize = 1000000;
		while (leftToSend != 0)
		{
			uint32_t sendAmount = leftToSend >= bufSize ? bufSize : (uint32_t)leftToSend;
			DEBUG_LOG("MOVESET_SYNC: Sending %u bytes\n", sendAmount);
			if (!SendPacket(dataToSend, sendAmount))
			{
				DEBUG_LOG("MOVESET_SYNC: Failed to send moveset bytes\n");
				sharedMemPtr->moveset_sync_status = MovesetSyncStatus_AcceptPackets;
				return;
			}
			dataToSend += sendAmount;
			leftToSend -= sendAmount;
		}
	}
}

void MovesetLoaderT7::InitMovesetSyncing()
{
	DEBUG_LOG("-- InitMovesetSyncing --\n");
	sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;

	syncStatus.Reset();
	incoming_moveset.data = nullptr;

	if (sharedMemPtr->locked_in == false) {
		DEBUG_LOG("MOVESET_SYNC: Not locked in.\n");
		return;
	}

	CSteamID desiredOpponent = GetLobbyOpponentSteamId();
	opponentId = desiredOpponent;
	if (desiredOpponent == k_steamIDNil) {
		DEBUG_LOG("MOVESET_SYNC: No valid opponent to send to\n");
		return;
	}

	sharedMemPtr->moveset_sync_status = MovesetSyncStatus_RequestSync;
	PacketT7_RequestMovesetSync packet;

	if (sharedMemPtr->players[0].custom_moveset_addr == 0) {
		packet.local_moveset.size = 0;
		packet.local_moveset.crc32 = 0;
		DEBUG_LOG("MOVESET_SYNC: Size of moveset is zero (no moveset to send but will still notify the opponent)\n");
	}
	else {
		packet.local_moveset.size = sharedMemPtr->players[0].custom_moveset_original_data_size;
		packet.local_moveset.crc32 = sharedMemPtr->players[0].crc32;
		DEBUG_LOG("MOVESET_SYNC: Size of moveset is %llu\n", packet.local_moveset.size);
	}

	DEBUG_LOG("MOVESET_SYNC: Sending PacketT7_RequestMovesetSync\n");
	if (!SendPacket(&packet, sizeof(packet)))
	{
		DEBUG_LOG("MOVESET_SYNC: Failed to send moveset info packet\n");
		sharedMemPtr->moveset_sync_status = MovesetSyncStatus_NotStarted;
		return;
	}
}

// -- Packet consumption -- //


void MovesetLoaderT7::DiscardIncomingPackets()
{
	char buf[8];
	uint32_t packetSize;
	while (SteamHelper::SteamNetworking()->IsP2PPacketAvailable(&packetSize, MOVESET_LOADER_P2P_CHANNEL))
	{
		DEBUG_LOG("DiscardIncomingPackets - %u\n", packetSize);
		CSteamID senderId;
		SteamHelper::SteamNetworking()->ReadP2PPacket(buf, sizeof(buf), &packetSize, &senderId, MOVESET_LOADER_P2P_CHANNEL);
	}
}

void MovesetLoaderT7::ConsumePackets()
{
	uint32_t packetSize;
	while (SteamHelper::SteamNetworking()->IsP2PPacketAvailable(&packetSize, MOVESET_LOADER_P2P_CHANNEL))
	{
		Byte* packetBuf = new Byte[packetSize];

		CSteamID senderId;
		if (SteamHelper::SteamNetworking()->ReadP2PPacket(packetBuf, packetSize, &packetSize, &senderId, MOVESET_LOADER_P2P_CHANNEL)) {
			OnPacketReceive(senderId, packetBuf, packetSize);
		}
		else {
			DEBUG_LOG("PACKET: Read failed\n");
		}

		delete[] packetBuf;
	}
}

bool MovesetLoaderT7::CanConsumePackets() {
	return sharedMemPtr->moveset_sync_status != MovesetSyncStatus_NotStarted && sharedMemPtr->moveset_sync_status != MovesetSyncStatus_Ready;
};