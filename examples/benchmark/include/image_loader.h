#pragma once
#include "benchmark_types.h"
#include <filesystem>
#include <string>

namespace bench {
bool loadImage(const std::filesystem::path& path, ImageBuffer& output, std::string& error);
bool probeImage(const std::filesystem::path& path, int& width, int& height, std::string& error);
}
