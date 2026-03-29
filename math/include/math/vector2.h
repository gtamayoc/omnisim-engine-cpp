#ifndef OMNISIM_MATH_VECTOR2_H
#define OMNISIM_MATH_VECTOR2_H

#include <cstddef>

namespace omnisim::math {

struct Vector2 {
    double x{0.0};
    double y{0.0};

    [[nodiscard]] constexpr Vector2 operator+(const Vector2& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y};
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y};
    }

    [[nodiscard]] constexpr Vector2 operator*(double scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    constexpr Vector2& operator+=(const Vector2& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Vector2& operator-=(const Vector2& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr Vector2& operator*=(double scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }
};

[[nodiscard]] double dot(const Vector2& lhs, const Vector2& rhs) noexcept;
[[nodiscard]] double magnitude(const Vector2& value) noexcept;
[[nodiscard]] Vector2 normalized(const Vector2& value, double epsilon = 1e-9) noexcept;

}  // namespace omnisim::math

#endif  // OMNISIM_MATH_VECTOR2_H
