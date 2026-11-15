#include "FileIO.hpp"
#include "Http.hpp"
#include "constants.h"
#include <curl/curl.h>
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <format>
#include <memory>
#include <vector>

namespace {
	struct Response {
		std::string data;
		size_t limit;
	};

	size_t Receive(char* bytes, size_t size, size_t count, void* context) noexcept
	{
		auto& response = *static_cast<Response*>(context);
		if (size && count > (response.limit - response.data.size()) / size) return 0;
		try {
			response.data.append(bytes, size * count);
			return size * count;
		} catch (...) {
			return 0;
		}
	}
}

std::string Http::RepositoryApi()
{
	const std::string repo = UPDATE_REPO_URL;
	return "https://api.github.com/repos/" + repo.substr(sizeof("https://github.com/") - 1);
}

std::string Http::Get(const std::string& url, size_t limit)
{
	static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (initialized != CURLE_OK) throw std::runtime_error("curl initialization failed");
	std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
	if (!curl) throw std::bad_alloc();
	Response response{{}, limit};
	curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
	curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
	curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "TKMovesets2");
	curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(HTTP_REQUEST_TIMEOUT));
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Receive);
	curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
	auto status = curl_easy_perform(curl.get());
	if (status != CURLE_OK) throw std::runtime_error(curl_easy_strerror(status));
	return std::move(response.data);
}

void Http::InstallDownload(const std::filesystem::path& destination, const std::string& content)
{
    if (!FileIO::WriteAtomically(destination, content))
        throw std::runtime_error("Cannot install download");
}

static std::string HexDigest(LPCWSTR algorithmId, ULONG digestSize, const std::string& content)
{
	BCRYPT_ALG_HANDLE algorithm{};
	if (BCryptOpenAlgorithmProvider(&algorithm, algorithmId, nullptr, 0) != 0)
		throw std::runtime_error("Hash algorithm unavailable");
	unsigned char digest[64];
	auto status = BCryptHash(algorithm, nullptr, 0, (PUCHAR)content.data(), static_cast<ULONG>(content.size()), digest, digestSize);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	if (status != 0) throw std::runtime_error("Hashing failed");
	std::string hex;
	for (ULONG i = 0; i < digestSize; ++i) hex += std::format("{:02x}", digest[i]);
	return hex;
}

std::string Http::Sha256(const std::string& content)
{
	return HexDigest(BCRYPT_SHA256_ALGORITHM, 32, content);
}

std::string Http::GitBlobSha(const std::string& content)
{
	// git's object id for a file: SHA-1 of "blob <size>\0" + bytes, the "sha" the GitHub contents API reports
	std::string object = std::format("blob {}", content.size());
	object.push_back('\0');
	object += content;
	return HexDigest(BCRYPT_SHA1_ALGORITHM, 20, object);
}
