#pragma once
#include <cmath>
namespace resume_policy {
inline bool seekable(double position, double duration) {
    return std::isfinite(position) && std::isfinite(duration) && position>10 &&
        (duration<=0 || position<duration-10);
}
inline bool recordable(double position,double duration) {
    return std::isfinite(position) && std::isfinite(duration) && position>=60 && duration>=0;
}
}
