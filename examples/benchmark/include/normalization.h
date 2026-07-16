#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace bench {

std::string canonicalFormat(std::string_view value);
std::string normalizedPayload(std::string_view format, std::string_view payload);
bool isSpecificBarberFormat(std::string_view value);
bool isPayloadStructurallyValid(std::string_view format, std::string_view payload);
bool isFormatSupported(std::string_view decoder, std::string_view canonical_format);
const std::unordered_set<std::string>& zxingSupportedFormats();
const std::unordered_set<std::string>& dbrSupportedFormats();

} // namespace bench
