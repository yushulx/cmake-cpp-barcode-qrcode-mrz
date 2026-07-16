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
};

std::unique_ptr<IDecoderAdapter> createZxingDecoder(int max_symbols);
std::unique_ptr<IDecoderAdapter> createDynamsoftDecoder(const std::string& template_path,
                                                        const std::string& template_name,
                                                        const std::string& license_key,
                                                        int max_symbols);

} // namespace bench
