#pragma once
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <string>
#include <vector>
#include "MovesetStructs.h"

enum class FileJobKind { ConvertToT7, ConvertToT8, Save };
enum class FileJobStage { Idle, Reading, Converting, Validating, Compressing, BackingUp, Installing, Completed, Failed, Cancelled };
const char* FileJobStageText(FileJobStage stage);
struct FileJobRequest {
    FileJobKind kind = FileJobKind::Save;
    std::filesystem::path source, output;
    std::vector<Byte> bytes;
    std::optional<TKMovesetCompressionType_> codec;
    bool backup = false, backupOnly = false, backupOnlyIfNone = false;
    unsigned maxBackups = 5;
};
struct FileJobResult {
    bool ok = false, cancelled = false;
    std::filesystem::path output, backup;
    std::string error;
    uint64_t date = 0;
};
using FileJobObserver = std::function<void(FileJobStage)>;
FileJobResult RunFileJob(FileJobRequest request, std::stop_token stop = {}, FileJobObserver observer = {});
class FileJob {
    mutable std::mutex m_mutex;
    bool m_busy = false;
    FileJobStage m_stage = FileJobStage::Idle;
    std::optional<FileJobResult> m_result;
    std::jthread m_worker;
public:
    ~FileJob();
    bool Start(FileJobRequest request, FileJobObserver observer = {});
    bool Busy() const;
    FileJobStage Stage() const;
    std::optional<FileJobResult> Poll();
    void Cancel();
    void Wait();
};
