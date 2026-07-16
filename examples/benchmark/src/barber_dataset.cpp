#include "barber_dataset.h"

#include "hash.h"
#include "image_loader.h"
#include "normalization.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace bench {
using json = nlohmann::json;
namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

json groundTruthJson(const GroundTruth& gt)
{
    json polygon = json::array();
    for (const auto& p : gt.polygon) polygon.push_back({p.x, p.y});
    return {
        {"annotation_id", gt.annotation_id}, {"format", gt.format}, {"text", gt.text},
        {"polygon", polygon}, {"ppe", gt.ppe ? json(*gt.ppe) : json(nullptr)},
        {"decode_eligible", gt.decode_eligible}, {"exclusion_reason", gt.exclusion_reason}
    };
}

json manifestJson(const ManifestRecord& record)
{
    json truth = json::array();
    for (const auto& gt : record.ground_truth) truth.push_back(groundTruthJson(gt));
    return {
        {"sample_id", record.sample_id}, {"relative_path", record.relative_path},
        {"annotation_file", record.annotation_file}, {"image_sha256", record.image_sha256},
        {"width", record.width}, {"height", record.height}, {"ground_truth", truth}
    };
}

ManifestRecord parseManifestRecord(const json& item)
{
    ManifestRecord r;
    r.sample_id = item.at("sample_id").get<std::string>();
    r.relative_path = item.at("relative_path").get<std::string>();
    r.annotation_file = item.at("annotation_file").get<std::string>();
    r.image_sha256 = item.value("image_sha256", "");
    r.width = item.value("width", 0); r.height = item.value("height", 0);
    for (const auto& value : item.at("ground_truth")) {
        GroundTruth gt;
        gt.annotation_id = value.at("annotation_id").get<std::string>();
        gt.format = value.at("format").get<std::string>();
        gt.text = value.value("text", "");
        gt.decode_eligible = value.value("decode_eligible", false);
        gt.exclusion_reason = value.value("exclusion_reason", "");
        if (value.contains("ppe") && !value["ppe"].is_null()) gt.ppe = value["ppe"].get<double>();
        for (const auto& p : value.value("polygon", json::array())) gt.polygon.push_back({p.at(0), p.at(1)});
        r.ground_truth.push_back(std::move(gt));
    }
    return r;
}

std::vector<ManifestRecord> chooseSmoke(const std::vector<ManifestRecord>& input)
{
    std::vector<std::size_t> order(input.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return input[a].sample_id < input[b].sample_id; });

    std::vector<ManifestRecord> output;
    std::unordered_set<std::string> selected;
    auto add = [&](auto predicate, std::size_t target) {
        for (auto i : order) {
            if (output.size() >= target) break;
            if (selected.count(input[i].sample_id) || !predicate(input[i])) continue;
            output.push_back(input[i]); selected.insert(input[i].sample_id);
        }
    };
    auto has2d = [](const ManifestRecord& r) {
        return std::any_of(r.ground_truth.begin(), r.ground_truth.end(), [](const auto& g) {
            const auto f = canonicalFormat(g.format); return f == "QR_CODE" || f == "DATA_MATRIX" || f == "AZTEC" || f == "PDF_417";
        });
    };
    auto has1d = [&](const ManifestRecord& r) { return !has2d(r); };
    auto eligible = [](const ManifestRecord& r) { return std::any_of(r.ground_truth.begin(), r.ground_truth.end(), [](const auto& g) { return g.decode_eligible; }); };
    add([&](const auto& r) { return eligible(r) && r.ground_truth.size() > 1 && has1d(r); }, 1);
    add([&](const auto& r) { return eligible(r) && r.ground_truth.size() > 1 && has2d(r); }, 2);
    add([&](const auto& r) { return eligible(r) && has1d(r) && std::any_of(r.ground_truth.begin(), r.ground_truth.end(), [](const auto& g) { return g.decode_eligible && !g.text.empty() && g.text.front() == '0'; }); }, 3);
    add([&](const auto& r) { return eligible(r) && std::any_of(r.ground_truth.begin(), r.ground_truth.end(), [](const auto& g) { return g.decode_eligible && g.ppe && *g.ppe < 2.0; }); }, 4);
    add([&](const auto& r) { return eligible(r) && has1d(r); }, 6);
    add([&](const auto& r) { return eligible(r) && has2d(r); }, 10);
    add([&](const auto& r) { return eligible(r); }, 10);
    return output;
}

} // namespace

void BarberDataset::writeManifest(const std::filesystem::path& path,
                                  const std::vector<ManifestRecord>& records)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write manifest: " + path.string());
    for (const auto& record : records) out << manifestJson(record).dump() << '\n';
}

std::vector<ManifestRecord> BarberDataset::readManifest(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read manifest: " + path.string());
    std::vector<ManifestRecord> records;
    std::string line;
    while (std::getline(in, line)) if (!line.empty()) records.push_back(parseManifestRecord(json::parse(line)));
    return records;
}

AuditSummary BarberDataset::audit(const std::filesystem::path& image_root,
                                  const std::filesystem::path& annotation_root,
                                  const std::filesystem::path& output_dir)
{
    if (!std::filesystem::is_directory(image_root)) throw std::runtime_error("image root is not a directory: " + image_root.string());
    if (!std::filesystem::is_directory(annotation_root)) throw std::runtime_error("annotation root is not a directory: " + annotation_root.string());

    AuditSummary summary;
    std::vector<std::filesystem::path> json_files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(annotation_root))
        if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".json") json_files.push_back(entry.path());
    std::sort(json_files.begin(), json_files.end());
    summary.annotation_files = json_files.size();

    std::unordered_map<std::string, std::filesystem::path> images;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(image_root)) {
        if (!entry.is_regular_file()) continue;
        auto relative = std::filesystem::relative(entry.path(), image_root);
        images.emplace(lower(relative.generic_string()), relative);
        images.emplace(lower(entry.path().filename().string()), relative);
        ++summary.source_images;
    }

    json source_files = json::array();
    std::vector<ManifestRecord> records;
    std::unordered_set<std::string> referenced;
    std::unordered_map<std::string, std::size_t> image_hash_counts;

    for (const auto& source : json_files) {
        std::cerr << "Auditing " << source.filename().string() << "..." << std::flush;
        const auto source_relative = std::filesystem::relative(source, annotation_root).generic_string();
        const auto source_hash = sha256File(source);
        source_files.push_back({{"relative_path", source_relative}, {"sha256", source_hash}, {"bytes", std::filesystem::file_size(source)}});
        std::ifstream input(source, std::ios::binary);
        json doc; input >> doc;
        if (!doc.contains("_via_img_metadata") || !doc["_via_img_metadata"].is_object())
            throw std::runtime_error("missing _via_img_metadata: " + source.string());

        std::vector<std::pair<std::string, json>> metadata;
        for (auto it = doc["_via_img_metadata"].begin(); it != doc["_via_img_metadata"].end(); ++it)
            metadata.emplace_back(it.key(), it.value());
        std::sort(metadata.begin(), metadata.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [via_id, item] : metadata) {
            ManifestRecord record;
            const auto filename = item.value("filename", "");
            auto found = images.find(lower(filename));
            if (found == images.end()) {
                ++summary.missing_images;
                record.relative_path = filename;
            } else {
                record.relative_path = found->second.generic_string();
                referenced.insert(lower(record.relative_path));
                const auto absolute = image_root / found->second;
                record.image_sha256 = sha256File(absolute);
                ++image_hash_counts[record.image_sha256];
                std::string probe_error;
                if (!probeImage(absolute, record.width, record.height, probe_error)) ++summary.invalid_annotations;
            }
            record.annotation_file = source_relative;
            record.sample_id = "sha256:" + sha256(source_relative + "\0" + filename + "\0" + record.image_sha256);

            const auto regions = item.value("regions", json::array());
            int region_index = 0;
            for (const auto& region : regions) {
                GroundTruth gt;
                gt.annotation_id = record.sample_id + ":" + std::to_string(region_index++);
                const auto attrs = region.value("region_attributes", json::object());
                gt.format = canonicalFormat(attrs.value("Type", ""));
                gt.text = attrs.value("String", "");
                if (attrs.contains("PPE") && attrs["PPE"].is_number()) gt.ppe = attrs["PPE"].get<double>();
                const auto shape = region.value("shape_attributes", json::object());
                const auto xs = shape.value("all_points_x", json::array());
                const auto ys = shape.value("all_points_y", json::array());
                if (xs.size() == ys.size()) for (std::size_t i = 0; i < xs.size(); ++i) gt.polygon.push_back({xs[i].get<int>(), ys[i].get<int>()});

                if (found == images.end()) gt.exclusion_reason = "missing_image";
                else if (!isSpecificBarberFormat(gt.format)) gt.exclusion_reason = "missing_reliable_payload";
                else if (gt.text.empty() || gt.text == "-1" || (gt.ppe && *gt.ppe < 0.0)) gt.exclusion_reason = "missing_reliable_payload";
                else if (!isPayloadStructurallyValid(gt.format, gt.text)) gt.exclusion_reason = "invalid_payload";
                else gt.decode_eligible = true;

                ++summary.annotations;
                if (gt.decode_eligible) ++summary.eligible_annotations;
                else if (gt.exclusion_reason == "missing_reliable_payload") ++summary.missing_payload_or_generic;
                else ++summary.invalid_annotations;
                record.ground_truth.push_back(std::move(gt));
            }
            summary.dataset_max_barcodes = std::max(summary.dataset_max_barcodes, static_cast<int>(record.ground_truth.size()));
            records.push_back(std::move(record));
        }
        std::cerr << " done\n";
    }

    // BarBeR contains 25 repeated VIA records for the same image path (mostly
    // overlap between Dubska and Szentandrasi-Dubska QR). A benchmark image
    // must be decoded once, so select one source record deterministically.
    // The original JSON files and their hashes remain untouched/auditable.
    std::cerr << "Resolving overlapping annotation records..." << std::flush;
    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) {
        return std::tie(a.relative_path, a.annotation_file, a.sample_id) <
               std::tie(b.relative_path, b.annotation_file, b.sample_id);
    });
    json overlaps = json::array();
    std::vector<ManifestRecord> unique_records;
    for (auto& record : records) {
        if (!unique_records.empty() && unique_records.back().relative_path == record.relative_path) {
            auto signature = [](const ManifestRecord& value) {
                std::multiset<std::pair<std::string,std::string>> result;
                for (const auto& gt : value.ground_truth) result.emplace(gt.format, gt.text);
                return result;
            };
            overlaps.push_back({{"relative_path",record.relative_path},
                {"selected_annotation_file",unique_records.back().annotation_file},
                {"discarded_annotation_file",record.annotation_file},
                {"same_format_payload_multiset",signature(unique_records.back())==signature(record)}});
            continue;
        }
        unique_records.push_back(std::move(record));
    }
    records = std::move(unique_records);
    std::cerr << " done\n";
    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.sample_id < b.sample_id; });
    summary.annotations = 0; summary.eligible_annotations = 0;
    summary.missing_payload_or_generic = 0; summary.invalid_annotations = 0;
    summary.dataset_max_barcodes = 0;
    image_hash_counts.clear();
    for (const auto& record : records) for (const auto& gt : record.ground_truth) {
        ++summary.annotations;
        if (gt.decode_eligible) ++summary.eligible_annotations;
        else if (gt.exclusion_reason == "missing_reliable_payload") ++summary.missing_payload_or_generic;
        else ++summary.invalid_annotations;
    }
    for (const auto& record : records) {
        summary.dataset_max_barcodes = (std::max)(summary.dataset_max_barcodes, static_cast<int>(record.ground_truth.size()));
        if (!record.image_sha256.empty()) ++image_hash_counts[record.image_sha256];
    }
    const auto audited_image_records = records.size();
    summary.duplicate_image_records = 0;
    for (const auto& [hash, count] : image_hash_counts) if (count > 1) summary.duplicate_image_records += count - 1;
    std::unordered_set<std::string> unique_images;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(image_root))
        if (entry.is_regular_file()) unique_images.insert(lower(std::filesystem::relative(entry.path(), image_root).generic_string()));
    for (const auto& image : unique_images) if (!referenced.count(image)) ++summary.unannotated_images;

    // The benchmark manifest contains only unique images with reliable ground
    // truth. The inventory below retains the complete audit counts.
    std::vector<ManifestRecord> benchmark_records;
    std::unordered_set<std::string> benchmark_hashes;
    json excluded_images = json::array();
    json duplicate_images = json::array();
    for (auto& record : records) {
        std::erase_if(record.ground_truth, [](const GroundTruth& gt) { return !gt.decode_eligible; });
        if (record.ground_truth.empty()) {
            ++summary.excluded_images_without_ground_truth;
            excluded_images.push_back({{"relative_path", record.relative_path},
                {"annotation_file", record.annotation_file}, {"reason", "no_reliable_ground_truth"}});
            continue;
        }
        if (!benchmark_hashes.insert(record.image_sha256).second) {
            duplicate_images.push_back({{"relative_path", record.relative_path},
                {"annotation_file", record.annotation_file}, {"image_sha256", record.image_sha256},
                {"reason", "exact_duplicate_image"}});
            continue;
        }
        benchmark_records.push_back(std::move(record));
    }
    summary.manifest_images = benchmark_records.size();
    summary.benchmark_annotations = 0;
    summary.dataset_max_barcodes = 0;
    for (const auto& record : benchmark_records) {
        summary.benchmark_annotations += record.ground_truth.size();
        summary.dataset_max_barcodes = (std::max)(summary.dataset_max_barcodes, static_cast<int>(record.ground_truth.size()));
    }

    std::filesystem::create_directories(output_dir);
    writeManifest(output_dir / "benchmark_manifest.jsonl", benchmark_records);
    writeManifest(output_dir / "smoke_manifest.jsonl", chooseSmoke(benchmark_records));
    json inventory = {
        {"dataset", "BarBeR"}, {"image_root", std::filesystem::absolute(image_root).generic_string()},
        {"annotation_root", std::filesystem::absolute(annotation_root).generic_string()},
        {"source_annotation_files", source_files},
        {"overlapping_annotation_records", overlaps},
        {"excluded_images", excluded_images},
        {"exact_duplicate_images", duplicate_images},
        {"summary", {{"annotation_files", summary.annotation_files}, {"manifest_images", summary.manifest_images},
            {"audited_image_records", audited_image_records}, {"source_images", summary.source_images},
            {"annotations", summary.annotations},
            {"eligible_annotations", summary.eligible_annotations},
            {"missing_payload_or_generic", summary.missing_payload_or_generic},
            {"invalid_annotations", summary.invalid_annotations}, {"missing_images", summary.missing_images},
            {"unannotated_images", summary.unannotated_images},
            {"duplicate_image_records", summary.duplicate_image_records},
            {"excluded_images_without_ground_truth", summary.excluded_images_without_ground_truth},
            {"benchmark_annotations", summary.benchmark_annotations},
            {"dataset_max_barcodes", summary.dataset_max_barcodes}}}
    };
    std::ofstream(output_dir / "barber_source_files.json") << std::setw(2) << inventory << '\n';
    if (!std::filesystem::exists(output_dir / "annotation_review.json"))
        std::ofstream(output_dir / "annotation_review.json") << "{\n  \"version\": 1,\n  \"reviews\": []\n}\n";
    return summary;
}

} // namespace bench
