#pragma once

#include "benchmark_types.h"
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace bench {

struct AuditSummary {
    std::size_t annotation_files = 0;
    std::size_t manifest_images = 0;
    std::size_t source_images = 0;
    std::size_t annotations = 0;
    std::size_t eligible_annotations = 0;
    std::size_t missing_payload_or_generic = 0;
    std::size_t invalid_annotations = 0;
    std::size_t missing_images = 0;
    std::size_t unannotated_images = 0;
    std::size_t duplicate_image_records = 0;
    std::size_t excluded_images_without_ground_truth = 0;
    std::size_t benchmark_annotations = 0;
    int dataset_max_barcodes = 0;
};

class BarberDataset {
public:
    AuditSummary audit(const std::filesystem::path& image_root,
                       const std::filesystem::path& annotation_root,
                       const std::filesystem::path& output_dir);

    static std::vector<ManifestRecord> readManifest(const std::filesystem::path& path);
    static void writeManifest(const std::filesystem::path& path,
                              const std::vector<ManifestRecord>& records);
};

} // namespace bench
