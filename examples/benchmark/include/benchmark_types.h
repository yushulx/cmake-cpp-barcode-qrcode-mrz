#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bench {

struct Point { int x = 0; int y = 0; };

struct GroundTruth {
    std::string annotation_id;
    std::string format;
    std::string text;
    std::vector<Point> polygon;
    std::optional<double> ppe;
    bool decode_eligible = false;
    std::string exclusion_reason;
};

struct ManifestRecord {
    std::string sample_id;
    std::string relative_path;
    std::string annotation_file;
    std::string image_sha256;
    int width = 0;
    int height = 0;
    std::vector<GroundTruth> ground_truth;
};

struct ImageBuffer {
    std::vector<std::uint8_t> rgb;
    int width = 0;
    int height = 0;
    int stride = 0;
};

struct DecodedBarcode {
    std::string canonical_format;
    std::vector<std::uint8_t> raw_bytes;
    std::string text;
    std::optional<double> confidence;
};

struct DecodeRun {
    std::vector<DecodedBarcode> results;
    std::chrono::nanoseconds decode_time{0};
    std::optional<std::string> error;
};

enum class Outcome {
    Correct, NotFound, WrongText, WrongFormat, ExtraResult,
    UnsupportedFormat, AmbiguousGroundTruth, DecoderError,
    LicenseOrInitializationError, InputPipelineError
};

struct MatchItem {
    std::optional<std::size_t> truth_index;
    std::optional<std::size_t> prediction_index;
    Outcome outcome = Outcome::NotFound;
};

std::string toString(Outcome value);

} // namespace bench
