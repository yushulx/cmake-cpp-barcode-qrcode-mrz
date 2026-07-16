#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace bench {

std::string sha256(std::string_view bytes);
std::string sha256File(const std::filesystem::path& path);

} // namespace bench
