#pragma once

#include "benchmark_types.h"
#include <memory>
#include <string>

namespace bench {

class IDecoderAdapter {
public:
    virtual ~IDecoderAdapter() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual DecodeRun decode(const ImageBuffer& image) = 0;
    // Optional human-readable note about the effective runtime configuration.
    // Used for disclosures when a license forces a fallback (e.g. Scandit multi-scan).
    virtual std::string configuration() const { return {}; }
};

std::unique_ptr<IDecoderAdapter> createZxingDecoder(int max_symbols);
std::unique_ptr<IDecoderAdapter> createDynamsoftDecoder(const std::string& template_path,
                                                        const std::string& template_name,
                                                        const std::string& license_key,
                                                        int max_symbols);
#ifdef BENCH_HAS_SCANDIT
std::unique_ptr<IDecoderAdapter> createScanditDecoder(const std::string& settings_path,
                                                      const std::string& license_key,
                                                      const std::string& writable_path,
                                                      int max_symbols);
#endif

} // namespace bench
