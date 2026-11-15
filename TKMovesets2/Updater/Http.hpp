#pragma once

#include <filesystem>
#include <string>

namespace Http {
	std::string Get(const std::string& url, size_t limit);
	void InstallDownload(const std::filesystem::path& destination, const std::string& content);
	std::string Sha256(const std::string& content);
	// git blob id (SHA-1 of "blob <size>\0" + content), as listed by the GitHub contents API
	std::string GitBlobSha(const std::string& content);
	std::string RepositoryApi();
}
