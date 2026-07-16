#include "result_writer.h"
#include "metrics.h"
#include "normalization.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace bench {
using json = nlohmann::json;
namespace {

std::string hex(const std::vector<std::uint8_t>& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string output(bytes.size() * 2, '0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    return output;
}

json groundTruth(const GroundTruth& gt)
{
    json polygon = json::array();
    for (const auto& p : gt.polygon) polygon.push_back({p.x, p.y});
    return {{"annotation_id",gt.annotation_id},{"format",gt.format},{"text",gt.text},
            {"ppe",gt.ppe ? json(*gt.ppe) : json(nullptr)},{"polygon",polygon},
            {"decode_eligible",gt.decode_eligible},{"exclusion_reason",gt.exclusion_reason}};
}

} // namespace

std::string recordKey(std::string_view sample_id, std::string_view decoder, int repetition)
{
    return std::string(sample_id) + "|" + std::string(decoder) + "|" + std::to_string(repetition);
}

std::set<std::string> completedKeys(const std::filesystem::path& jsonl)
{
    std::set<std::string> keys;
    std::ifstream in(jsonl, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto value = json::parse(line);
        keys.insert(recordKey(value.at("sample_id").get<std::string>(), value.at("decoder").get<std::string>(), value.at("repetition").get<int>()));
    }
    return keys;
}

void appendResult(const std::filesystem::path& jsonl, const RawResultRecord& record)
{
    std::filesystem::create_directories(jsonl.parent_path());
    const auto lock_path = std::filesystem::path(jsonl.string() + ".lock");
    bool locked = false;
    for (int attempt = 0; attempt < 600; ++attempt) {
        std::error_code error;
        if (std::filesystem::create_directory(lock_path,error)) { locked = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!locked) throw std::runtime_error("timed out acquiring result lock: " + lock_path.string());
    struct LockGuard {
        std::filesystem::path path;
        ~LockGuard() { std::error_code ignored; std::filesystem::remove(path,ignored); }
    } guard{lock_path};

    // Re-check only the file tail inside the cross-process lock. This keeps
    // overlapping resumed processes safe without rereading the complete file.
    const auto key = recordKey(record.sample.sample_id,record.decoder,record.repetition);
    struct ResultCache {
        std::uintmax_t consumed_bytes = 0;
        std::set<std::string> keys;
    };
    static std::map<std::string,ResultCache> caches;
    auto& cache=caches[std::filesystem::absolute(jsonl).string()];
    std::error_code size_error;
    const auto current_size=std::filesystem::exists(jsonl)?std::filesystem::file_size(jsonl,size_error):0;
    if(size_error||current_size<cache.consumed_bytes){cache={};}
    if(current_size>cache.consumed_bytes){
        std::ifstream tail(jsonl,std::ios::binary);
        tail.seekg(static_cast<std::streamoff>(cache.consumed_bytes));
        std::string line;
        while(std::getline(tail,line)){
            if(line.empty())continue;
            const auto value=json::parse(line);
            cache.keys.insert(recordKey(value.at("sample_id").get<std::string>(),
                                        value.at("decoder").get<std::string>(),
                                        value.at("repetition").get<int>()));
        }
        cache.consumed_bytes=current_size;
    }
    if(cache.keys.count(key))return;

    json truth = json::array();
    for (const auto& gt : record.sample.ground_truth) truth.push_back(groundTruth(gt));
    json predictions = json::array();
    for (const auto& prediction : record.run.results) {
        predictions.push_back({{"format", prediction.canonical_format}, {"text", prediction.text},
            {"raw_bytes_hex", hex(prediction.raw_bytes)},
            {"confidence", prediction.confidence ? json(*prediction.confidence) : json(nullptr)}});
    }
    json matches = json::array();
    for (const auto& match : record.matches) matches.push_back({
        {"truth_index", match.truth_index ? json(*match.truth_index) : json(nullptr)},
        {"prediction_index", match.prediction_index ? json(*match.prediction_index) : json(nullptr)},
        {"outcome", toString(match.outcome)}});
    json value = {
        {"protocol",record.protocol},{"manifest_sha256",record.manifest_sha256},
        {"sample_id",record.sample.sample_id},{"relative_path",record.sample.relative_path},
        {"annotation_file",record.sample.annotation_file},{"image_sha256",record.sample.image_sha256},
        {"width",record.sample.width},{"height",record.sample.height},{"ground_truth",truth},
        {"decoder",record.decoder},{"decoder_version",record.decoder_version},
        {"config_sha256",record.config_sha256},{"repetition",record.repetition},
        {"image_load_ns",record.image_load_ns},{"decode_ns",record.run.decode_time.count()},
        {"error",record.run.error ? json(*record.run.error) : json(nullptr)},
        {"predictions",predictions},{"matches",matches}
    };
    std::ofstream out;
    for(int attempt=0;attempt<100&&!out.is_open();++attempt){
        out.clear();
        out.open(jsonl,std::ios::binary|std::ios::app);
        if(!out.is_open())std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!out.is_open()) throw std::runtime_error("cannot append result: " + jsonl.string());
    out << value.dump() << '\n';
    out.close();
    cache.keys.insert(key);
    cache.consumed_bytes=std::filesystem::file_size(jsonl);
}

void generateSummary(const std::filesystem::path& jsonl, const std::filesystem::path& output)
{
    struct Counts {
        std::size_t records=0, eligible=0, correct=0, unsupported=0, errors=0;
        std::size_t common_eligible=0, common_correct=0, image_all_read=0;
        std::int64_t decode_ns=0;
        std::map<std::string,std::size_t> outcomes;
        std::map<std::string,std::map<std::string,std::size_t>> by_format,by_source;
        std::vector<std::int64_t> timings;
    };
    std::map<std::string, Counts> totals;
    std::ifstream in(jsonl, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read results: " + jsonl.string());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto value = json::parse(line);
        auto& c = totals[value.at("decoder").get<std::string>()];
        ++c.records; c.decode_ns += value.value("decode_ns", 0LL);
        c.timings.push_back(value.value("decode_ns",0LL));
        if (!value["error"].is_null()) ++c.errors;
        bool all_read=value["error"].is_null();
        for (const auto& match : value.at("matches")) {
            const auto outcome = match.at("outcome").get<std::string>();
            ++c.outcomes[outcome];
            if (outcome != "extra_result") ++c.eligible;
            if (outcome == "correct") ++c.correct;
            if (outcome == "unsupported_format") ++c.unsupported;
            if(outcome!="correct"&&outcome!="extra_result")all_read=false;
            if(!match["truth_index"].is_null()){
                const auto truth_index=match["truth_index"].get<std::size_t>();
                const auto format=value["ground_truth"][truth_index].value("format","");
                ++c.by_format[format][outcome];
                ++c.by_source[value.value("annotation_file","")][outcome];
                if(isFormatSupported("zxing-cpp",format)&&isFormatSupported("dynamsoft-dbr",format)){
                    ++c.common_eligible;
                    if(outcome=="correct")++c.common_correct;
                }
            }
        }
        if(all_read)++c.image_all_read;
    }
    json decoders = json::object();
    for (const auto& [name,c] : totals) {
        const auto interval = wilsonInterval(c.correct, c.eligible);
        const auto common_interval=wilsonInterval(c.common_correct,c.common_eligible);
        const auto supported_denominator = c.eligible - c.unsupported;
        auto outcomeCount=[&](const char* key){auto it=c.outcomes.find(key);return it==c.outcomes.end()?std::size_t{0}:it->second;};
        const auto false_predictions=outcomeCount("wrong_text")+outcomeCount("wrong_format")+outcomeCount("extra_result");
        const double precision=c.correct+false_predictions?double(c.correct)/(c.correct+false_predictions):0.0;
        const double recall=c.eligible?double(c.correct)/c.eligible:0.0;
        auto percentile=[&](double q){
            if(c.timings.empty())return 0.0;
            auto values=c.timings;std::sort(values.begin(),values.end());
            const auto index=static_cast<std::size_t>(q*static_cast<double>(values.size()-1));
            return static_cast<double>(values[index])/1e6;
        };
        decoders[name] = {
            {"records",c.records},{"eligible_instances",c.eligible},{"correct",c.correct},
            {"unsupported",c.unsupported},{"errors",c.errors},{"outcomes",c.outcomes},
            {"coverage_adjusted_recall",c.eligible ? double(c.correct)/c.eligible : 0.0},
            {"coverage_adjusted_recall_ci95",{interval.first,interval.second}},
            {"common_format_eligible",c.common_eligible},{"common_format_correct",c.common_correct},
            {"common_format_recall",c.common_eligible?double(c.common_correct)/c.common_eligible:0.0},
            {"common_format_recall_ci95",{common_interval.first,common_interval.second}},
            {"supported_format_recall",supported_denominator ? double(c.correct)/supported_denominator : 0.0},
            {"precision",precision},{"f1",precision+recall?2.0*precision*recall/(precision+recall):0.0},
            {"image_all_read_rate",c.records?double(c.image_all_read)/c.records:0.0},
            {"by_format",c.by_format},{"by_source",c.by_source},
            {"mean_decode_ms",c.records ? double(c.decode_ns)/c.records/1e6 : 0.0},
            {"median_decode_ms",percentile(0.5)},{"p90_decode_ms",percentile(0.90)},
            {"p95_decode_ms",percentile(0.95)},{"p99_decode_ms",percentile(0.99)},
            {"total_decode_ms",double(c.decode_ns)/1e6}
        };
    }
    json summary = {
        {"title","ZXing-C++ vs. Dynamsoft Barcode Reader"},
        {"dataset","BarBeR public dataset"},
        {"disclosure","This benchmark was implemented and published by Dynamsoft, the developer of Dynamsoft Barcode Reader. It uses the public third-party BarBeR dataset. To make the comparison auditable, the protocol, source code, decoder configurations, environment details, dataset manifest, HTML report, and per-image raw results are provided. BarBeR's standardized annotations were generated with assistance from proprietary Datalogic software. Difficult undecodable barcode regions were manually localized and are excluded from decoding accuracy when no reliable payload is available."},
        {"decoders",decoders}
    };
    std::filesystem::create_directories(output.parent_path());
    std::ofstream(output) << std::setw(2) << summary << '\n';
}

void generateResultsJson(const std::filesystem::path& jsonl,
                         const std::filesystem::path& summary,
                         const std::filesystem::path& output)
{
    json records = json::array();
    std::ifstream in(jsonl, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read results: " + jsonl.string());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) records.push_back(json::parse(line));
    }
    std::ifstream summary_in(summary, std::ios::binary);
    if (!summary_in) throw std::runtime_error("cannot read summary: " + summary.string());
    json value = {
        {"summary", json::parse(summary_in)},
        {"records", records}
    };
    std::filesystem::create_directories(output.parent_path());
    std::ofstream(output) << std::setw(2) << value << '\n';
}

} // namespace bench
