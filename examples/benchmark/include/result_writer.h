#pragma once

#include "benchmark_types.h"
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

namespace bench {

struct RawResultRecord {
    std::string protocol;
    std::string manifest_sha256;
    ManifestRecord sample;
    std::string decoder;
    std::string decoder_version;
    std::string config_sha256;
    int repetition = 0;
    std::int64_t image_load_ns = 0;
    DecodeRun run;
    std::vector<MatchItem> matches;
};

std::string recordKey(std::string_view sample_id, std::string_view decoder, int repetition);
std::set<std::string> completedKeys(const std::filesystem::path& jsonl);
void appendResult(const std::filesystem::path& jsonl, const RawResultRecord& record);
void generateSummary(const std::filesystem::path& jsonl, const std::filesystem::path& output);
void generateResultsJson(const std::filesystem::path& jsonl,
                         const std::filesystem::path& summary,
                         const std::filesystem::path& output);

} // namespace bench
