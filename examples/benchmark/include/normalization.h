#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace bench {

std::string canonicalFormat(std::string_view value);
// GS1-128 is Code 128 carrying GS1 data: same bars, same code sets, same check
// character, with an FNC1 in the first data position as the only difference. AIM
// distinguishes them by symbology identifier (]C0 vs ]C1), a data convention
// rather than a different symbology, so both names form one format family.
std::string formatFamily(std::string_view value);
std::string normalizedPayload(std::string_view format, std::string_view payload);
bool isUnreliablePlaceholder(std::string_view payload);
bool isSpecificBarberFormat(std::string_view value);
bool isPayloadStructurallyValid(std::string_view format, std::string_view payload);
bool isFormatSupported(std::string_view decoder, std::string_view canonical_format);
const std::unordered_set<std::string>& zxingSupportedFormats();
const std::unordered_set<std::string>& dbrSupportedFormats();
const std::unordered_set<std::string>& scanditSupportedFormats();

} // namespace bench
