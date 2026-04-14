#include "Body.h"
#include <cmath>

double Vec3::length() const {
    return std::sqrt(lengthSq());
}

Vec3 Vec3::normalized() const {
    double len = length();
    if (len < 1e-12) return {0, 0, 0};
    return {x/len, y/len, z/len};
}

double Body::kineticEnergy() const {
    return 0.5 * mass * vel.lengthSq();
}

double Body::speed() const {
    return vel.length();
}
