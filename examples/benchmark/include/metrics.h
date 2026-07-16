#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <utility>
#include <vector>

namespace bench {
std::pair<double,double> wilsonInterval(std::uint64_t successes,std::uint64_t total);
}
