#pragma once

#include "benchmark_types.h"
#include <string_view>

namespace bench {

std::vector<MatchItem> matchResults(const std::vector<GroundTruth>& truth,
                                    const std::vector<DecodedBarcode>& predictions,
                                    std::string_view decoder);

} // namespace bench
