#include "metrics.h"
#include <algorithm>
#include <cmath>

namespace bench {
std::pair<double,double> wilsonInterval(std::uint64_t k,std::uint64_t n)
{
    if(!n)return {0,0};
    constexpr double z=1.959963984540054;
    const double p=double(k)/n,den=1+z*z/n;
    const double center=(p+z*z/(2*n))/den;
    const double margin=z*std::sqrt((p*(1-p)+z*z/(4*n))/n)/den;
    return {std::max(0.0,center-margin),std::min(1.0,center+margin)};
}
}
