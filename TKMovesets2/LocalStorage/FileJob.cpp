#include "constants.h"
#include "FileJob.hpp"
#include "MovesetFile.hpp"
#include "FileIO.hpp"
#include "Compression.hpp"
#include "GameIDs.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Helpers.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;
namespace {
struct Cancelled {};
void Phase(FileJobStage stage, std::stop_token stop, const FileJobObserver& observer) {
    if (stop.stop_requested()) throw Cancelled{};
    if (observer) observer(stage);
    if (stop.stop_requested()) throw Cancelled{};
}
std::vector<Byte> Encode(std::span<const Byte> bytes, TKMovesetCompressionType_ codec) {
    uint64_t size = 0;
    std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(bytes.data(), bytes.size(), codec, size));
    if (!compressed) throw std::runtime_error("Cannot compress moveset");
    return {compressed.get(), compressed.get() + size};
}
// Matches exactly the backup names RunFileJob generates: prefix + formatDateTime(..., true)
// ("dd-mm-yy_HHhMM", empty if formatting failed) + optional " (N)". A bare prefix match would also
// catch the backups of a moveset whose own name starts with this prefix (e.g. "Jin_[BAK]_<date>").
bool IsGeneratedBackup(const std::wstring& stem, const std::wstring& prefix) {
    if (!stem.starts_with(prefix)) return false;
    std::wstring_view rest(stem); rest.remove_prefix(prefix.size());
    const auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
    constexpr std::wstring_view date = L"00-00-00_00h00";
    if (rest.size() >= date.size() && std::equal(date.begin(), date.end(), rest.begin(),
        [&](wchar_t pattern, wchar_t c) { return pattern == L'0' ? digit(c) : pattern == c; }))
        rest.remove_prefix(date.size());
    if (rest.empty()) return true;
    if (rest.size() < 4 || !rest.starts_with(L" (") || !rest.ends_with(L')')) return false;
    rest = rest.substr(2, rest.size() - 3);
    return std::all_of(rest.begin(), rest.end(), digit);
}
std::vector<fs::path> Backups(const fs::path& directory, const std::wstring& prefix) {
    std::vector<fs::path> files;
    if (!fs::exists(directory)) return files;
    const auto absolute = fs::absolute(directory).lexically_normal();
    for (const auto& entry : fs::directory_iterator(absolute)) {
        if (entry.is_symlink() || !entry.is_regular_file()) continue;
        const auto path = entry.path().lexically_normal();
        if (path.parent_path() == absolute && path.extension() == L"" MOVESET_FILENAME_EXTENSION &&
            IsGeneratedBackup(path.stem().wstring(), prefix))
            files.push_back(path);
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        auto ta = fs::last_write_time(a), tb = fs::last_write_time(b);
        return ta == tb ? a < b : ta < tb;
    });
    return files;
}
}
const char* FileJobStageText(FileJobStage stage) {
    switch (stage) {
    case FileJobStage::Reading: return "Reading moveset...";
    case FileJobStage::Converting: return "Converting moveset...";
    case FileJobStage::Validating: return "Validating moveset...";
    case FileJobStage::Compressing: return "Compressing moveset...";
    case FileJobStage::BackingUp: return "Saving backup...";
    case FileJobStage::Installing: return "Saving file...";
    case FileJobStage::Completed: return "File saved.";
    case FileJobStage::Failed: return "File operation failed.";
    case FileJobStage::Cancelled: return "File operation cancelled.";
    default: return "";
    }
}
FileJobResult RunFileJob(FileJobRequest request, std::stop_token stop, FileJobObserver observer) {
    FileJobResult result;
    result.output = request.output;
    try {
        if (request.output.empty()) throw std::invalid_argument("Missing output path");
        auto codec = request.codec.value_or(TKMovesetCompressionType_None);
        std::vector<Byte> bytes;
        if (request.kind == FileJobKind::ConvertToT7 || request.kind == FileJobKind::ConvertToT8) {
            if (fs::absolute(request.source).lexically_normal() == fs::absolute(request.output).lexically_normal() ||
                (fs::exists(request.output) && fs::equivalent(request.source, request.output)))
                throw std::invalid_argument("Conversion output must differ from the source");
            Phase(FileJobStage::Reading, stop, observer);
            MovesetFile source(request.source);
            Phase(FileJobStage::Converting, stop, observer);
            bytes = request.kind == FileJobKind::ConvertToT8 ? source.ConvertToT8() : source.ConvertToT7();
            codec = request.codec.value_or(source.OriginalCompression());
        } else bytes = std::move(request.bytes);
        Phase(FileJobStage::Validating, stop, observer);
        if (bytes.size() < sizeof(TKMovesetHeader)) throw std::runtime_error("Incomplete save snapshot");
        const auto& header = *reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        if (!header.ValidateHeader(bytes.size()) || header.isCompressed()) throw std::runtime_error("Invalid save snapshot");
        if (header.gameId == GameId_T8 && header.minorVersion == GameVersions::T8::RETAIL) {
            auto payload=std::span<const Byte>(bytes).subspan(header.moveset_data_start);
            if (header.header_size < offsetof(TKMovesetHeader,game_specific_flags)+sizeof(uint64_t) || header.block_list_size!=1 || *reinterpret_cast<const uint64_t*>(bytes.data()+header.block_list)!=0)
                throw std::runtime_error("Invalid retail save directory");
            RetailT8Snapshot::Validate(payload);
            uint32_t version; memcpy(&version,payload.data()+4,4);
            if (header.game_specific_flags!=(version==1?RetailT8Snapshot::TablesOnlyFlag:RetailT8Snapshot::RuntimeAssetsFlag) ||
                Helpers::CalculateCrc32(payload.data(),payload.size())!=header.crc32)
                throw std::runtime_error("Invalid retail save flags or checksum");
        } else {
            if (!header.ValidateHeader(bytes.size()) || header.isCompressed() || header.gameId != GameId_T7 ||
                header.block_list_size < 8) throw std::runtime_error("Invalid T7 save snapshot");
            auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header.block_list);
            std::string reason;
            if (!T7ImportUtils::ValidateT7Semantics(*blocks, bytes.data() + header.moveset_data_start,
                bytes.size() - header.moveset_data_start, &reason)) throw std::runtime_error(reason);
        }
        result.date = header.date;
        if (request.backup || request.backupOnly) {
            Phase(FileJobStage::BackingUp, stop, observer);
            const auto directory = request.output.parent_path() / MOVESET_AUTO_BACKUPDIRECTORY;
            const auto prefix = request.output.stem().wstring() + (request.backup && !request.backupOnly ? L"_[ORIGINAL]_" : L"_[BAK]_");
            auto previous = Backups(directory, prefix);
            if (!(request.backupOnlyIfNone && !previous.empty())) {
                std::vector<Byte> encoded;
                if (!request.backupOnly && fs::exists(request.output)) {
                    std::ifstream original(request.output,std::ios::binary | std::ios::ate);
                    auto length=original.tellg();
                    if (!original || length<0 || static_cast<uint64_t>(length)>(1ULL<<30)) throw std::runtime_error("Cannot read original for backup");
                    encoded.resize(static_cast<size_t>(length)); original.seekg(0);
                    if (!original.read(reinterpret_cast<char*>(encoded.data()),encoded.size())) throw std::runtime_error("Cannot read original for backup");
                }
                else encoded=Encode(bytes,TKMovesetCompressionType_LZ4);
                auto name = prefix + Helpers::string_to_wstring(Helpers::formatDateTime(Helpers::getCurrentTimestamp(), true));
                fs::create_directories(directory);
                auto target = directory / (name + L"" MOVESET_FILENAME_EXTENSION);
                for (unsigned index = 2; fs::exists(target); ++index)
                    target = directory / (name + L" (" + std::to_wstring(index) + L")" MOVESET_FILENAME_EXTENSION);
                Phase(FileJobStage::BackingUp, stop, observer);
                if (!FileIO::WriteAtomically(target, encoded)) throw std::runtime_error("Cannot install moveset backup");
                result.backup = target;
                // Retire old backups only after the replacement exists; directories/symlinks are excluded.
                const auto keep = (std::max)(1u, request.maxBackups);
                while (previous.size() >= keep) {
                    std::error_code error;
                    fs::remove(previous.front(), error);
                    previous.erase(previous.begin());
                }
            }
        }
        if (!request.backupOnly) {
            Phase(FileJobStage::Compressing, stop, observer);
            auto encoded = Encode(bytes, codec);
            Phase(FileJobStage::Installing, stop, observer);
            if (!FileIO::WriteAtomically(request.output, encoded)) throw std::runtime_error("Cannot replace output file");
        }
        result.ok = true;
    } catch (const Cancelled&) { result.cancelled = true; }
    catch (const std::exception& error) { result.error = error.what(); }
    catch (...) { result.error = "Unknown file operation error"; }
    return result;
}
FileJob::~FileJob() { if (m_worker.joinable()) m_worker.join(); }
bool FileJob::Start(FileJobRequest request, FileJobObserver observer) {
    if (Busy()) return false;
    if (m_worker.joinable()) m_worker.join();
    {
        std::lock_guard lock(m_mutex);
        m_busy = true; m_stage = FileJobStage::Reading; m_result.reset();
    }
    try {
        m_worker = std::jthread([this, request = std::move(request), observer = std::move(observer)](std::stop_token stop) mutable {
            auto result = RunFileJob(std::move(request), stop, [&](FileJobStage stage) {
                { std::lock_guard lock(m_mutex); m_stage = stage; }
                if (observer) observer(stage);
            });
            std::lock_guard lock(m_mutex);
            m_stage = result.ok ? FileJobStage::Completed : result.cancelled ? FileJobStage::Cancelled : FileJobStage::Failed;
            m_result = std::move(result); m_busy = false;
        });
    } catch (...) {
        std::lock_guard lock(m_mutex); m_busy = false; m_stage = FileJobStage::Failed;
        throw;
    }
    return true;
}
bool FileJob::Busy() const { std::lock_guard lock(m_mutex); return m_busy; }
FileJobStage FileJob::Stage() const { std::lock_guard lock(m_mutex); return m_stage; }
std::optional<FileJobResult> FileJob::Poll() { std::lock_guard lock(m_mutex); auto result = std::move(m_result); m_result.reset(); return result; }
void FileJob::Cancel() { if (m_worker.joinable()) m_worker.request_stop(); }

void FileJob::Wait() { if (m_worker.joinable()) m_worker.join(); }
