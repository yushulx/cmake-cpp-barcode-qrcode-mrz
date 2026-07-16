#include "test_support.h"
#include <iostream>

int main()
{
    try { testMatching(); testMetrics(); testBarberParser(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "All benchmark tests passed\n";
    return 0;
}
