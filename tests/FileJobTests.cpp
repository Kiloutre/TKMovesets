#include "constants.h"
#include <windows.h>
#include "FileJob.hpp"
#include "MovesetFile.hpp"
#include "Structs_t7.h"
#include "FileIO.hpp"
#include "Compression.hpp"
#include <future>
#include <fstream>
#include <stdexcept>

std::vector<Byte> SemanticFixture();
namespace {
namespace fs = std::filesystem;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Directory {
    fs::path path = fs::temp_directory_path() / ("tkmovesets-filejobs-" + std::to_string(GetCurrentProcessId()));
    Directory() { Check(fs::create_directory(path), "cannot create job directory"); }
    ~Directory() { std::error_code error; fs::remove_all(path, error); }
};
FileJobRequest Save(const fs::path& output) {
    FileJobRequest request; request.output = output; request.bytes = SemanticFixture(); request.codec = TKMovesetCompressionType_LZMA;
    return request;
}
std::vector<Byte> Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}
std::vector<Byte> T7RetailConvertibleFixture();
void RunFileJobTests() {
    Directory directory;
    auto source = directory.path / "source.tkmvst", output = directory.path / "result.tkmvst";
    Check(FileIO::WriteAtomically(source, SemanticFixture()), "job source write failed");
    const auto sourceBefore = Read(source);
    {
        const auto t7=directory.path/"t7.tkmvst", t8=directory.path/"t8.tkmvst";
        auto bytes=T7RetailConvertibleFixture();
        for(auto codec:{TKMovesetCompressionType_None,TKMovesetCompressionType_LZ4,TKMovesetCompressionType_LZMA,TKMovesetCompressionType_CompactZstd}) {
            FileJobRequest save; save.output=t7; save.bytes=bytes; save.codec=codec;
            Check(RunFileJob(save).ok,"Cannot prepare compressed T7 source");
            const auto before=Read(t7);
            FileJobRequest request; request.kind=FileJobKind::ConvertToT8; request.source=t7; request.output=t8;
            auto conversion=RunFileJob(request);
            Check(conversion.ok,conversion.error.c_str());
            MovesetFile result(t8); result.Validate();
            Check(result.OriginalCompression()==codec && Read(t7)==before,"T8 conversion changed source or lost compression");
            request.output=t7;
            Check(!RunFileJob(request).ok && Read(t7)==before,"Conversion overwrote its T7 source");
            const auto hardlink=directory.path/"t7-link.tkmvst";
            fs::create_hard_link(t7,hardlink); request.output=hardlink;
            Check(!RunFileJob(request).ok && Read(t7)==before,"Conversion overwrote a hard-linked source");
            fs::remove(hardlink);
        }
        const auto previous=Read(t8);
        // A T7 moveset without moves cannot be converted (the self-contained converter maps everything else).
        auto unconvertible=T7RetailConvertibleFixture();
        {
            const auto& h=*reinterpret_cast<const TKMovesetHeader*>(unconvertible.data());
            reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(unconvertible.data()+h.moveset_data_start)->table.entries[12].listCount=0;
        }
        Check(FileIO::WriteAtomically(t7,unconvertible),"Cannot prepare unsupported T7 source");
        FileJobRequest request; request.kind=FileJobKind::ConvertToT8; request.source=t7; request.output=t8;
        Check(!RunFileJob(request).ok && Read(t8)==previous,"Unsupported conversion replaced a valid output");
    }
    FileJobRequest convert; convert.kind = FileJobKind::ConvertToT7; convert.source = source; convert.output = output;
    convert.codec = TKMovesetCompressionType_LZ4;
    auto result = RunFileJob(convert); Check(result.ok && result.output == output, "file conversion job failed");
    MovesetFile converted(output); converted.Validate();
    Check(Read(source) == sourceBefore, "conversion job changed source");
    auto previous = Read(output);
    {
        std::stop_source cancellation;
        auto cancelled = RunFileJob(Save(output), cancellation.get_token(), [&](FileJobStage phase) {
            if (phase == FileJobStage::Installing) cancellation.request_stop();
        });
        Check(cancelled.cancelled && !cancelled.ok && Read(output) == previous, "cancelled job changed output");
    }
    {
        auto failed = RunFileJob(Save(output), {}, [&](FileJobStage phase) {
            if (phase == FileJobStage::Installing) throw std::bad_alloc();
        });
        Check(!failed.ok && !failed.error.empty() && Read(output) == previous, "failed job changed output");
    }
    {
        FileJob worker;
        std::promise<void> reached, release;
        auto atCompression = reached.get_future();
        auto continueWork = release.get_future().share();
        auto request = Save(output);
        const auto snapshot = request.bytes;
        Check(worker.Start(std::move(request), [&](FileJobStage phase) {
            if (phase == FileJobStage::Compressing) { reached.set_value(); continueWork.wait(); }
        }), "job did not start");
        const bool arrived = atCompression.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        const bool responsive = worker.Busy() && worker.Stage() == FileJobStage::Compressing;
        const bool busyRejected = !worker.Start(Save(source));
        release.set_value(); worker.Wait();
        auto complete = worker.Poll();
        Check(arrived && responsive && busyRejected, "job blocked owner or accepted overlapping work");
        Check(complete && complete->ok && !worker.Busy() && !worker.Poll(), "job result lifecycle failed");
        MovesetFile saved(output);
        Check(saved.Bytes().size() == snapshot.size() && !memcmp(saved.Bytes().data(), snapshot.data(), snapshot.size()), "worker changed owned snapshot");
        Check(Read(source) == sourceBefore, "busy rejection changed another target");
        Check(worker.Start(Save(output)), "completed worker could not restart");
        worker.Wait(); Check(worker.Poll()->ok, "restarted save failed");
    }
    {
        FileJob worker;
        std::promise<void> reached, release;
        auto ready = reached.get_future();
        auto gate = release.get_future().share();
        Check(worker.Start(Save(output), [&](FileJobStage phase) {
            if (phase == FileJobStage::Validating) { reached.set_value(); gate.wait(); }
        }), "cancel worker did not start");
        const bool arrived = ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        worker.Cancel(); release.set_value(); worker.Wait();
        auto result = worker.Poll();
        Check(arrived && result && result->cancelled && !result->ok, "worker cancellation failed");
    }
    {
        Check(FileIO::WriteAtomically(output,sourceBefore), "cannot prepare original backup target");
        auto request = Save(output); reinterpret_cast<TKMovesetHeader*>(request.bytes.data())->date ^= 1; request.backup = true; request.backupOnlyIfNone = true; request.maxBackups = 2;
        result = RunFileJob(request); Check(result.ok && fs::exists(result.backup), "initial backup not created");
        auto initialBackup = result.backup;
        Check(Read(initialBackup)==sourceBefore,"backup did not preserve original stored bytes");
        Check(MovesetFile(output).Header().date != reinterpret_cast<const TKMovesetHeader*>(sourceBefore.data())->date,"test did not save a different revision");
        result = RunFileJob(request); Check(result.ok && result.backup.empty() && fs::exists(initialBackup), "initial-only backup duplicated");
        request.backupOnly = true; request.backupOnlyIfNone = false;
        for (int i = 0; i < 4; ++i) Check(RunFileJob(request).ok, "automatic backup failed");
        const auto folder = output.parent_path() / MOVESET_AUTO_BACKUPDIRECTORY;
        size_t count = 0; for (auto& entry : fs::directory_iterator(folder)) if (entry.is_regular_file()) ++count;
        Check(count == 3 && Read(initialBackup)==sourceBefore, "recovery retention removed original or kept wrong count");
        auto protectedFile = folder / "unrelated.tkmvst";
        Check(FileIO::WriteAtomically(protectedFile, sourceBefore), "cannot create unrelated backup-folder file");
        Check(RunFileJob(request).ok && Read(protectedFile) == sourceBefore, "retention removed unrelated file");
        // Backups of a moveset named like a backup ("result_[BAK]_<date>") share the bare prefix but are
        // not this moveset's backups: retention and backupOnlyIfNone must ignore them.
        const std::vector<fs::path> foreign = {
            folder / "result_[BAK]_01-11-26_10h00_[BAK]_02-11-26_11h00.tkmvst",
            folder / "result_[BAK]_01-11-26_10h00_[BAK]_03-11-26_12h00 (2).tkmvst",
            folder / "result_[BAK]_notes.tkmvst"};
        for (const auto& path : foreign) Check(FileIO::WriteAtomically(path, sourceBefore), "cannot create foreign backup");
        for (int i = 0; i < 3; ++i) Check(RunFileJob(request).ok, "automatic backup failed");
        for (const auto& path : foreign) Check(Read(path) == sourceBefore, "retention removed another moveset's backup");
        const auto other = directory.path / "other.tkmvst";
        const auto otherForeign = folder / "other_[ORIGINAL]_01-11-26_10h00_[ORIGINAL]_02-11-26_11h00.tkmvst";
        Check(FileIO::WriteAtomically(otherForeign, sourceBefore), "cannot create foreign original backup");
        auto first = Save(other); first.backup = true; first.backupOnlyIfNone = true;
        result = RunFileJob(first);
        Check(result.ok && fs::exists(result.backup) && Read(otherForeign) == sourceBefore, "another moveset's backup suppressed the initial backup");
    }
    {
        auto bad = Save(output); bad.bytes.resize(3);
        Check(!RunFileJob(bad).ok, "bad save snapshot accepted");
        convert.source = directory.path / "missing";
        Check(!RunFileJob(convert).ok, "missing conversion source accepted");
        // Accepted saves finish during owner shutdown; no detached thread outlives the job.
        FileJob worker; Check(worker.Start(Save(output)), "shutdown save did not start");
    }
    {
        // Version-gated header fields past header_size are property/block bytes: unvalidated, possibly
        // unterminated. The accessors must fall back instead of exposing them.
        TKMovesetHeader h; strcpy_s(h.version_string, "1.0"); strcpy_s(h.target_character, "Jin");
        memset(h.orig_character_name, 'A', sizeof(h.orig_character_name)); h.game_specific_flags = 5; h.conversion_origin = 7;
        h.header_size = 208;
        Check(h.GetOrigCharacterName() == h.target_character && h.GetGameSpecificFlags() == 0 && h.GetConversionOrigin() == 0,
            "header accessor read past header_size");
        h.header_size = 240; h.orig_character_name[31] = 0;
        Check(h.GetOrigCharacterName() == h.orig_character_name && h.GetGameSpecificFlags() == 0, "240-byte header fields misread");
        h.header_size = 256;
        Check(h.GetGameSpecificFlags() == 5 && h.GetConversionOrigin() == 7, "current header fields not read");
    }
    MovesetFile finalFile(output); finalFile.Validate();
}
