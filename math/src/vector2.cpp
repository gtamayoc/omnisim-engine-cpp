#include "math/vector2.h"

#include <cmath>

namespace omnisim::math {

double dot(const Vector2& lhs, const Vector2& rhs) noexcept {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

double magnitude(const Vector2& value) noexcept {
    return std::sqrt(dot(value, value));
}

Vector2 normalized(const Vector2& value, const double epsilon) noexcept {
    const double length = magnitude(value);
    if (length <= epsilon) {
        return {};
    }
    return {value.x / length, value.y / length};
}

}  // namespace omnisim::math
