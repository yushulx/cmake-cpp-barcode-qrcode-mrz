#pragma once
#include <stdexcept>
#include <string>

#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(std::string("CHECK failed: ") + #expr + " at " + __FILE__ + ":" + std::to_string(__LINE__)); } while (0)

void testMatching();
void testMetrics();
void testBarberParser();
