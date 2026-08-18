#include "benchmark_types.h"
#include "matcher.h"
#include "result_writer.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

bench::GroundTruth parseGroundTruth(const json& value)
{
    bench::GroundTruth gt;
    gt.annotation_id = value.value("annotation_id", "");
    gt.format = value.value("format", "");
    gt.text = value.value("text", "");
    gt.decode_eligible = value.value("decode_eligible", false);
    gt.exclusion_reason = value.value("exclusion_reason", "");
    if (value.contains("ppe") && !value["ppe"].is_null())
        gt.ppe = value["ppe"].get<double>();
    if (value.contains("polygon") && value["polygon"].is_array()) {
        for (const auto& point : value["polygon"]) {
            if (!point.is_array() || point.size() < 2) continue;
            gt.polygon.push_back({point[0].get<int>(), point[1].get<int>()});
        }
    }
    return gt;
}

bench::DecodedBarcode parsePrediction(const json& value)
{
    bench::DecodedBarcode prediction;
    prediction.canonical_format = value.value("format", "");
    prediction.text = value.value("text", "");
    if (value.contains("confidence") && !value["confidence"].is_null())
        prediction.confidence = value["confidence"].get<double>();
    return prediction;
}

json matchJson(const bench::MatchItem& match)
{
    return {
        {"truth_index", match.truth_index ? json(*match.truth_index) : json(nullptr)},
        {"prediction_index", match.prediction_index ? json(*match.prediction_index) : json(nullptr)},
        {"outcome", bench::toString(match.outcome)}
    };
}

} // namespace

int main(int argc, char** argv)
{
    try {
        std::string input_path;
        std::string output_dir;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + key);
            if (key == "--results") input_path = argv[++i];
            else if (key == "--output") output_dir = argv[++i];
            else throw std::runtime_error("unexpected argument: " + key);
        }
        if (input_path.empty() || output_dir.empty())
            throw std::runtime_error("usage: rematch_results --results FILE --output DIR");

        std::ifstream in(input_path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot read results: " + input_path);

        const fs::path output = output_dir;
        fs::create_directories(output);
        const fs::path jsonl = output / "results.jsonl";
        const bool inplace = fs::equivalent(fs::absolute(input_path), fs::absolute(jsonl));
        const fs::path written = inplace ? output / "results.rematched.jsonl" : jsonl;
        {
            std::ofstream out(written, std::ios::binary);
            if (!out) throw std::runtime_error("cannot write " + written.string());
            std::string line;
            std::size_t count = 0;
            while (std::getline(in, line)) {
                if (line.empty()) continue;
                auto value = json::parse(line);
                std::vector<bench::GroundTruth> truth;
                for (const auto& item : value.at("ground_truth"))
                    truth.push_back(parseGroundTruth(item));
                std::vector<bench::DecodedBarcode> predictions;
                for (const auto& item : value.at("predictions"))
                    predictions.push_back(parsePrediction(item));
                json matches = json::array();
                if (!value["error"].is_null()) {
                    matches = value.at("matches");
                } else {
                    const auto rematched = bench::matchResults(truth, predictions, value.at("decoder").get<std::string>());
                    for (const auto& match : rematched) matches.push_back(matchJson(match));
                }
                value["matches"] = matches;
                out << value.dump() << '\n';
                ++count;
            }
            std::cout << "rematched " << count << " records\n";
        }
        if (inplace) {
            std::error_code error;
            fs::remove(jsonl, error);
            fs::rename(written, jsonl, error);
            if (error) {
                fs::copy_file(written, jsonl, fs::copy_options::overwrite_existing);
                fs::remove(written);
            }
        }
        const auto summary = output / "summary.json";
        bench::generateSummary(jsonl, summary);
        bench::generateResultsJson(jsonl, summary, output / "results.json");
        std::cout << "wrote " << jsonl << ", " << summary << " and " << (output / "results.json") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
