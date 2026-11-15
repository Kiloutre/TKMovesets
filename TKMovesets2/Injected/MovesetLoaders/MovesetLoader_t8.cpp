#include <stdarg.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <iostream>
#include <stdarg.h>
#include <cstdio>
//#include <openssl/evp.h>


#include "MovesetLoader_t8.hpp"
#include "TagT8.hpp"
#include "Helpers.hpp"
#include "steamHelper.hpp"

#include "steam_api.h"

using namespace StructsT8CNT;

// Reference to the MovesetLoader
MovesetLoaderT8* g_loader = nullptr;
std::ofstream* g_log = nullptr;
std::ofstream* g_Wslog = nullptr;
std::ofstream* g_packetHistory = nullptr;
std::ofstream* g_decryptHistory = nullptr;
std::ofstream* g_encryptHistory = nullptr;
unsigned int g_roundType = 0;
byte g_mm_aes_key[16];
byte g_mm_aes_iv[16];
byte g_turn_aes_key[16];
byte g_turn_aes_iv[16];

# define GET_HOOK(x) ((uint64_t)&T8Hooks::x)

// -- Game functions -- //

struct tk_DiarkisPacketInfo
{
	uint64_t packet_size_minus_code;
	int* data_ptr;
};

struct tk_EncryptedPayload
{
	uint64_t size;
	byte* data;
	void* a1;
	void* a2;
	void* a3;
	void* a4;
	void* a5;
	void* a6;
	byte* packetMagicPtr;
};

struct tk_Encryption
{
	int _unk0x0;
	int _unk0x4;
	char* keyStart;
	char* keyEnd;
	int _unk0x18;
	int _unk0x1c;
	char* ivStart;
	char* ivEnd;
};


namespace T8Functions
{
	typedef void** (*Log)(void** a1, const char* a2, size_t a3);
	typedef void** (*Log2)(void** a1, const char* a2, size_t a3);
	typedef void* (*Format)(void* a1, const char** format, void* a3, void* a4);

	typedef char (*ResetRound)(unsigned int a1);
	typedef int64 (*DiarkisPacketRecv)(int64 a1, int64 a2, tk_DiarkisPacketInfo* original_packet, int64 a4);

	typedef int64(*BotanDecrypt)(int64, void* payload, uint64_t payload_size);
	typedef int64(*BotanDecryptParent)(tk_Encryption* enc, int64 a2, tk_EncryptedPayload* _payload);
	typedef int64(*BotanEncryptParent)(tk_Encryption* enc, int64 a2, tk_EncryptedPayload* _payload);
	
	typedef const char* (*DiarkisErrString)(const unsigned int* err_ptr);
	typedef const char* (*DiarkisErrString)(const unsigned int* err_ptr);

	typedef int (*WsLog)(int a1, char* a2, int a3);
	typedef int (*WsLogFmt)(int a1, char* format, ...);
}

// -- Helpers --
// Try to cache variable addresses when possible instead of calling the helpers too much


// -- Other helpers -- //


// -- Hook functions --
int filterException(int code, PEXCEPTION_POINTERS ex) {
	return EXCEPTION_EXECUTE_HANDLER;
}

void fancyPrint(const unsigned char* data, size_t data_size, std::ofstream* out)
{
	std::string output;
	const char* charset = "0123456789ABCDEF";
	char hexVal[4];
	hexVal[2] = ' ';
	hexVal[3] = '\0';
	unsigned int i = 0;
	const unsigned char* cursor = data;
	while ((i + 16) <= data_size)
	{
		for (unsigned int j = 0; j < 16; ++j)
		{
			hexVal[0] = charset[cursor[j] / 16];
			hexVal[1] = charset[cursor[j] % 16];
			output += hexVal;
			//out->write(hexVal, 3);
		}
		output += ' ';
		//out->write(" ", 1);
		for (unsigned int j = 0; j < 16; ++j)
		{
			if (isprint(cursor[j])) {
				//out->write((char*)&cursor[j], 1);
				output += (char)cursor[j];
			}
			else {
				//out->write(".", 1);
				output += '.';
			}
		}

		cursor += 16;
		i += 16;
		output += '\n';
		//out->write("\n", 1);
	}

	if (data_size != i)
	{
		const unsigned int remainingBytes = data_size - i;

		unsigned int j = 0;
		for (; j < remainingBytes; ++j)
		{
			hexVal[0] = charset[cursor[j] / 16];
			hexVal[1] = charset[cursor[j] % 16];
			output += hexVal;
			//out->write(hexVal, 3);
		}
		while (j++ < 16)
		{
			output += "   ";
			//out->write("   ", 3);
		}
		output += ' ';
		//out->write(" ", 1);
		for (j = 0; j < remainingBytes; ++j)
		{
			if (isprint(cursor[j])) {
				//out->write((char*)&cursor[j], 1);
				output += cursor[j];
			}
			else {
				//out->write(".", 1);
				output += ".";
			}
		}


		output += '\n';
		//out->write("\n", 1);
	}

	{
		output += "b'";
		//out->write("b'", 2);
		cursor = data;
		i = 0;
		auto mustPrintHexChar = [](unsigned char c) {
			return isprint(c) && c != '\\' && c != '\'';
		};
		hexVal[2] = '\0';
		while (i < data_size)
		{
			unsigned char c = cursor[i];
			/*
			if (mustPrintHexChar(c)) {
				g_packetHistory->write((char*)& c, 1);
			} else {
			*/
			hexVal[0] = charset[cursor[i] / 16];
			hexVal[1] = charset[cursor[i] % 16];
			output += "\\x";
			output += hexVal;
			//out->write("\\x", 2);
			//out->write(hexVal, 2);
			//}
			++i;
		}
		output += "'\n";
		//out->write("'\n", 2);
	}

	out->write(output.c_str(), output.size());
}

const int MAX_SIZE = 4096;
unsigned char* plaintext = (unsigned char*)malloc(MAX_SIZE);

void decryptAES_CBC(const unsigned char* ciphertext, size_t ciphertextSize, byte* key, byte* iv)
{
	/*
	const int offset = 19 + 32;
	if (ciphertextSize <= (offset + 16)) return;

	ciphertext += offset;
	ciphertextSize -= offset;

	size_t payload_size = (ciphertext[17] << 8) + ciphertext[18];	// bad for 0xD0 EB

	if (payload_size > ciphertextSize) {
		payload_size = ciphertextSize;
	}


	if (ciphertextSize >= MAX_SIZE)
	{
		std::string s = std::format("Size {} is bigger than the limit ({})\n", ciphertextSize, MAX_SIZE);
		g_packetHistory->write(s.c_str(), s.size());
		return;
	}

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

	if (!ctx) {
		return ;
	}

	EVP_CIPHER_CTX_set_padding(ctx, 0);

	if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) == 1) {
		int out_len = 0;
		if (EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext, ciphertextSize) == 1) {
			int final_len;
			if (EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len) == 1) {
				fancyPrint(plaintext, payload_size, g_packetHistory);
			}
		}
	}

	EVP_CIPHER_CTX_free(ctx);
	*/
}

bool hexStringToBytes(const char* hexString, byte* byteArray)
{
	printf("[%s]\n", hexString);
	unsigned char byteValue = 0;

	for (size_t i = 0; i < 32; i++) {
		char c = hexString[i];

		if (c >= '0' && c <= '9') {
			byteValue = (byteValue << 4) | (c - '0');
		}
		else if (c >= 'a' && c <= 'f') {
			byteValue = (byteValue << 4) | (c - 'a' + 10);
		}
		else if (c >= 'A' && c <= 'F') {
			byteValue = (byteValue << 4) | (c - 'A' + 10);
		}
		else {
			std::cerr << "Invalid character in the hex string." << std::endl;
			return false;
		}

		if (i % 2 == 1) {
			byteArray[i / 2] = byteValue;
			byteValue = 0;
		}
	}

	return true;
}


namespace T8Hooks
{
	// Log function 
	void** Log(void** a1, const char* format, size_t string_length)
	{
		if (string_length > 200 && strstr(format, "serverType\":"))
		{
			auto encKeyStart = strstr(format, "encryptionKey") + strlen("encryptionKey\":\"");
			auto ivKeyStart = strstr(format, "encryptionIV") + strlen("encryptionIV\":\"");

			if (strstr(format, "serverType\":\"MM"))
			{
				hexStringToBytes(encKeyStart, g_mm_aes_key);
				hexStringToBytes(ivKeyStart, g_mm_aes_iv);
				printf("MM ptr [%p] -- [%p]\n", g_mm_aes_key, g_mm_aes_iv);
			}
			else
			{
				hexStringToBytes(encKeyStart, g_turn_aes_key);
				hexStringToBytes(ivKeyStart, g_turn_aes_iv);
				printf("TURN ptr [%p] -- [%p]\n", g_turn_aes_key, g_turn_aes_iv);
			}

			printf("%s\n", format);
		}

		//encryptionk
		std::string t = Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp());
		/*
		if (strcmp(format, "exchangeId") == 0) {
			while (true) {
				;
			}
		}
		*/
		if (g_log) {
			std::string myStr = "[" + t + "-]" + std::string(format, string_length) + "\n";
			g_log->write(myStr.c_str(), myStr.size());
		}
		
		//printf("[%s] %s\n", t.c_str(), format);
		
		auto ret = g_loader->CastTrampoline<T8Functions::Log>("TK__Log")(a1, format, string_length);

		return ret;
	}

	// Log function 
	void** Log2(void** a1, const char* format, size_t string_length)
	{
		//encryptionk
		std::string t = Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp());
		/*
		if (strcmp(format, "exchangeId") == 0) {
			while (true) {
				;
			}
		}
		*/
		if (g_log) {
			std::string myStr = "[" + t + "_]" + std::string(format, string_length) + "\n";
			g_log->write(myStr.c_str(), myStr.size());
		}
		//printf("[%s] %s\n", t.c_str(), format);
		auto ret = g_loader->CastTrampoline<T8Functions::Log>("TK__Log2")(a1, format, string_length);
		return ret;
	}

	void* Format(void* a1, const char** format, void* a3, void* a4)
	{
		//std::string t = std::format("----> %s\n", format[0]);
		char** retval = (char**)g_loader->CastTrampoline<T8Functions::Format>("TK__Format")(a1, format, a3, a4);
		/*
		__try {
			printf(" = %s\n", retval[0]);
		}
		__except (filterException(GetExceptionCode(), GetExceptionInformation())) {
			printf("\n");
		}
		*/
		return (void*)retval;
	}

	int64 DiarkisPacketRecv(int64 a1, int64 a2, tk_DiarkisPacketInfo* original_packet, int64 a4)
	{
		/*
		std::string text = std::format("\n[{}] - Received packet of size {}\n", Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp()).c_str(), original_packet->packet_size_minus_code);
		g_packetHistory->write(text.c_str(), text.size());
		fancyPrint((unsigned char*)original_packet->data_ptr, original_packet->packet_size_minus_code, g_packetHistory);
		decryptAES_CBC((unsigned char*)original_packet->data_ptr, original_packet->packet_size_minus_code, g_mm_aes_key, g_mm_aes_iv);
		decryptAES_CBC((unsigned char*)original_packet->data_ptr, original_packet->packet_size_minus_code, g_turn_aes_key, g_turn_aes_iv);
		auto ret =  g_loader->CastTrampoline<T8Functions::DiarkisPacketRecv>("TK__DiarkisPacketRecv")(a1, a2, original_packet, a4);

		return ret;
		*/
		return 0;
	}


	int64 BotanDecrypt(int64 a1, void* payload, uint64_t payload_size)
	{
		/*
		std::string text = std::format("\n[{}] - Decrypting size {}\n", Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp()).c_str(), payload_size);
		g_decryptHistory->write(text.c_str(), text.size());
		fancyPrint((unsigned char*)payload, payload_size, g_decryptHistory);

		auto ret = g_loader->CastTrampoline<T8Functions::BotanDecrypt>("TK__BotanDecrypt")(a1, payload, payload_size);
		fancyPrint((unsigned char*)payload, payload_size, g_decryptHistory);
		return ret;
		*/
		return 0;
	}

	int64 BotanDecryptParent(tk_Encryption* enc, int64 a2, tk_EncryptedPayload* payload)
	{
		/*
		byte* packetPtr;
		uint64_t fullPacketSize;

		bool isD0Packet = payload->data[-0x2C] == 0x0D && payload->data[-0x2B] == 0x0E;
		if (isD0Packet) {
			packetPtr = payload->data - 0x2C;
			fullPacketSize = payload->size + 0x2C;
		}
		else {
			packetPtr = payload->packetMagicPtr - 4;
			fullPacketSize = 19 + 32 + payload->size;
		}

		std::string text = std::format("\n[{} BOTANPARENT] - Payload Size {} / Packet Size {} - Type: {}\n", Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp()).c_str(), payload->size, fullPacketSize, isD0Packet);
		g_decryptHistory->write(text.c_str(), text.size());
		fancyPrint((unsigned char*)enc->keyStart, enc->keyEnd - enc->keyStart, g_decryptHistory);
		fancyPrint((unsigned char*)enc->ivStart, enc->ivEnd - enc->ivStart, g_decryptHistory);
		g_decryptHistory->write("====\n", 5);

		fancyPrint(packetPtr, fullPacketSize, g_decryptHistory);

		auto ret = g_loader->CastTrampoline<T8Functions::BotanDecryptParent>("TK__BotanDecryptParent")(enc, a2, payload);

		fancyPrint(payload->data, payload->size, g_decryptHistory);

		return ret;
		*/
		return 0;
	}

	int64 BotanEncryptParent(tk_Encryption* enc, int64 a2, tk_EncryptedPayload* payload)
	{
		/*
		std::string text = std::format("\n[{} BOTANPARENT] - Payload Size: {}\n", Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp()).c_str(), payload->size);
		g_encryptHistory->write(text.c_str(), text.size());
		fancyPrint((unsigned char*)enc->keyStart, enc->keyEnd - enc->keyStart, g_encryptHistory);
		fancyPrint((unsigned char*)enc->ivStart, enc->ivEnd - enc->ivStart, g_encryptHistory);
		g_encryptHistory->write("====\n", 5);

		fancyPrint(payload->data, payload->size, g_encryptHistory);

		auto ret = g_loader->CastTrampoline<T8Functions::BotanEncryptParent>("TK__BotanEncryptParent")(enc, a2, payload);

		fancyPrint(payload->data, payload->size, g_encryptHistory);
		return ret;
		*/
		return 0;
	}


	const char* DiarkisErrString(const unsigned int* err_ptr)
	{
		/*
		auto ret = g_loader->CastTrampoline<T8Functions::DiarkisErrString>("TK__DiarkisErrString")(err_ptr);

		std::string text = std::format("\n[{}] - DIARKIS ERROR: [{}]\n", Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp()).c_str(), ret);
		g_log->write(text.c_str(), text.size());
		*/

		return 0;
	}


	int WsLog(int a1, char* a2, int a3)
	{
		if (g_Wslog) {
			std::string t = Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp());
			char* t2 = (char*)g_loader->moduleAddrPtr + 0x91E9BE0;
			std::string myStr = "[" + t + "-]" + t2 + "\n";
			g_Wslog->write(myStr.c_str(), myStr.size());
		}
		return g_loader->CastTrampoline<T8Functions::WsLog>("TK__WsLog")(a1, a2, a3);
	}

	void WsLogFmt(unsigned int a1, const char* format, ...)
	{
		va_list va;
		va_start(va, format);

		char buf[1024];
		vsnprintf(buf, sizeof(buf), format, va);
		va_end(va);

		if (g_Wslog) {
			std::string t = Helpers::formatDateTimeMs(Helpers::getCurrentTimestamp());
			std::string myStr = "[" + t + "-]" + buf + "\n";
			g_Wslog->write(myStr.c_str(), myStr.size());
		}

		// Variadic arguments cannot be forwarded: hand the already formatted text to the original function
		auto original = g_loader->CastTrampoline<T8Functions::WsLogFmt>("TK__WsLogFmt");
		if (original) {
			char passthroughFormat[] = "%s";
			original((int)a1, passthroughFormat, buf);
		}
	};
}

// -- -- //

// -- Hooking init --

void MovesetLoaderT8::InitHooks()
{
	// Set the global reference to us
	g_loader = this;

	// Declare which hooks must absolutely be found to initialize this DLL
	// These signatures are specific to the closed beta build (TK__Format embeds a build-specific RIP displacement):
	// requiring them makes Init() fail, before anything is patched, on any other Polaris build such as retail Tekken 8.
	m_requiredSymbols = {
		// Hooks
		"TK__Log",
		"TK__Log2",
		"TK__Format",
		// Functions
	};

	// Crucial functions / hooks

	// Other less important things

	// Find TK__Log
	RegisterHook("TK__Log", m_moduleName, "f_Log", GET_HOOK(Log));
	RegisterHook("TK__Log2", m_moduleName, "f_Log2", GET_HOOK(Log2));
	RegisterHook("TK__Format", m_moduleName, "f_format", GET_HOOK(Format));

	if (!HasHook("TK__Log") || !HasHook("TK__Log2") || !HasHook("TK__Format"))
	{
		// Not the closed beta: retail. The loader only serves Tools > Extra's Tag mode there (its addresses are the
		// t8-retail_tag_* entries of game_addresses.ini, checked against this executable's bytes by TagT8::Init).
		isRetail = true;
		m_requiredSymbols.clear();
		addresses.gameKey = "t8-retail";
		addresses.minorGameKey = "t8-retail";
		tagInitError.clear();
		if (!TagT8::Init(this, moduleAddr, moduleSize, addresses, tagInitError)) {
			DEBUG_LOG("Tag mode unavailable: %s\n", tagInitError.c_str());
		}
		return;
	}
	//RegisterHook("TK__DiarkisPacketRecv", m_moduleName, "f_DiarkisPacketRecv", GET_HOOK(DiarkisPacketRecv));
	//RegisterHook("TK__BotanDecrypt", m_moduleName, "f_BotanDecrypt", GET_HOOK(BotanDecrypt));
	//RegisterHook("TK__BotanDecryptParent", m_moduleName, "f_BotanDecryptParent", GET_HOOK(BotanDecryptParent));
	//RegisterHook("TK__BotanEncryptParent", m_moduleName, "f_BotanEncryptParent", GET_HOOK(BotanEncryptParent));
	//RegisterHook("TK__DiarkisErrString", m_moduleName, "f_DiarkisErrString", GET_HOOK(DiarkisErrString));

	// The websocket log hooks are at fixed RVAs of the closed beta build: only install them once the
	// closed beta signatures above have proven that this is that build.
	if (HasHook("TK__Log") && HasHook("TK__Log2") && HasHook("TK__Format"))
	{
		{
			auto funcAddr = addresses.ReadPtrPathInCurrProcess("f_WsLog_addr", moduleAddr);
			DEBUG_LOG("WsLog is %llx\n", funcAddr);
			InitHook("TK__WsLog", funcAddr, (uint64_t)&T8Hooks::WsLog);
		}
		{
			auto funcAddr = addresses.ReadPtrPathInCurrProcess("f_WsLogFmt_addr", moduleAddr);
			DEBUG_LOG("WsLogFmt is %llx\n", funcAddr);
			InitHook("TK__WsLogFmt", funcAddr, (uint64_t)&T8Hooks::WsLogFmt);
		}
	}
	else {
		DEBUG_LOG("Closed beta signatures not found: not installing the fixed-address websocket log hooks\n");
	}

}
void MovesetLoaderT8::OnInitEnd()
{
	sharedMemPtr = (SharedMemT8*)orig_sharedMemPtr;

	// Compute important variable addresses

	// Apply the hooks that need to be applied immediately

	// Other
	HookFunction("TK__Log");
	HookFunction("TK__Log2");
	HookFunction("TK__Format");
	//HookFunction("TK__DiarkisPacketRecv");
	//HookFunction("TK__BotanDecrypt");
	//HookFunction("TK__BotanDecryptParent");
	//HookFunction("TK__BotanEncryptParent");
	//HookFunction("TK__DiarkisErrString"); // Never registered (see InitHooks)
	HookFunction("TK__WsLog");
	HookFunction("TK__WsLogFmt");
	
	DEBUG_LOG("g_roundType = %p\n", &g_roundType);

#ifdef BUILD_TYPE_DEBUG
	// Research logs (game log, websocket, packets, encryption): debug builds only, in %TEMP%\TKMovesets2.
	// Release builds open no file; every writer below checks its stream for null.
	{
		std::string currDate = Helpers::formatDateTime(Helpers::getCurrentTimestamp(), true);

		std::error_code error;
		const auto logDirectory = std::filesystem::temp_directory_path(error) / "TKMovesets2";
		std::filesystem::create_directories(logDirectory, error);
		const std::string basePath = logDirectory.generic_string();
		{
			std::string filename = std::format("{}/Logs_{}.txt", basePath, currDate);
			g_log = new std::ofstream(filename);
			g_log->write("test\n", 5);
		}

		{
			std::string filename = std::format("{}/WsLogs_{}.txt", basePath, currDate);
			g_Wslog = new std::ofstream(filename);
			g_Wslog->write("test\n", 5);
		}

		{
			std::string filename = std::format("{}/PacketLogs_{}.txt", basePath, currDate);
			g_packetHistory = new std::ofstream(filename);
			g_packetHistory->write("test\n", 5);
		}

		{
			std::string filename = std::format("{}/DecryptLogs_{}.txt", basePath, currDate);
			g_decryptHistory = new std::ofstream(filename);
			g_decryptHistory->write("test\n", 5);
		}

		{
			std::string filename = std::format("{}/EncryptLogs_{}.txt", basePath, currDate);
			g_encryptHistory = new std::ofstream(filename);
			g_encryptHistory->write("test\n", 5);
		}
	}
#endif
}

// -- Main -- //

void MovesetLoaderT8::HandleTagCommands()
{
	auto mem = sharedMemPtr;
	if (!mem) return;
	const int32_t request = mem->tag_request_id;
	if (request != mem->tag_ack_id) {
		std::string error;
		bool ok = true;
		if (!isRetail) { ok = false; error = "Tag mode needs retail Tekken 8"; }
		else if (!TagT8::IsInitialized()) { ok = false; error = tagInitError; }
		else if (mem->tag_command == TagCommand_Enable) ok = TagT8::Enable(mem->tag_p3_fighter, mem->tag_p4_fighter, error);
		else if (mem->tag_command == TagCommand_Disable && !TagT8::Disable()) { ok = false; error = "can't disable while P3/P4 are loading"; }
		if (!ok) {
			mem->tag_state = TagState_Error;
			strncpy_s(mem->tag_status, sizeof(mem->tag_status), ("Tag: " + error).c_str(), _TRUNCATE);
		}
		mem->tag_ack_id = request;
		if (!ok) return;
	}
	if (isRetail && TagT8::IsInitialized() && mem->tag_trigger_id != lastTriggerId) {
		lastTriggerId = mem->tag_trigger_id;
		TagT8::RequestTagFor(mem->tag_trigger_team);
	}
	if (isRetail && TagT8::IsInitialized() && mem->tag_ack_id != 0) {
		mem->tag_state = TagT8::State();
		strncpy_s(mem->tag_status, sizeof(mem->tag_status), TagT8::Status().c_str(), _TRUNCATE);
	}
}

void MovesetLoaderT8::Mainloop()
{
	DEBUG_LOG("Mainloop()\n");
	if (sharedMemPtr && isRetail && !TagT8::IsInitialized()) {
		sharedMemPtr->tag_state = TagState_Error;
		strncpy_s(sharedMemPtr->tag_status, sizeof(sharedMemPtr->tag_status), ("Tag: " + tagInitError).c_str(), _TRUNCATE);
	}
	while (!mustStop)
	{
		HandleTagCommands();
		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
	TagT8::Shutdown();
	for (auto stream : { g_log, g_packetHistory, g_encryptHistory, g_decryptHistory, g_Wslog }) {
		if (stream) stream->close();
	}
	DEBUG_LOG("Mainloop - END\n");
}

// -- Interaction -- //

// -- Debug -- //

void MovesetLoaderT8::Debug()
{
#ifdef BUILD_TYPE_DEBUG
	void* func_addr;
	switch (g_roundType)
	{
	case 0:
		func_addr = (void*)0x14153DE80;
		break;
	case 1:
		func_addr = (void*)0x14153DF20;
		break;
	default:
		return;
	}
	//void (*test)(void) = (void(*)(void))func_addr;
	//test();
	//g_loader->CastFunction<T8Functions::ResetRound>("TK__ResetRound")(g_roundType);
#endif
}