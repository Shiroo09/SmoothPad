#pragma once

#include "Types.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

namespace smoothpad {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kEpsilon = 1e-12;

[[nodiscard]] inline double Clamp(double v, double lo, double hi) noexcept {
  return std::clamp(v, lo, hi);
}

[[nodiscard]] inline double Lerp(double a, double b, double t) noexcept {
  return a + (b - a) * t;
}

[[nodiscard]] inline double Smoothstep(double edge0, double edge1, double x) noexcept {
  if (edge1 <= edge0) {
    return x >= edge1 ? 1.0 : 0.0;
  }
  const double t = Clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] inline double Hermite(double t) noexcept {
  t = Clamp(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] inline double Sigmoid(double x) noexcept {
  return 1.0 / (1.0 + std::exp(-x));
}

[[nodiscard]] inline bool NearlyEqual(double a, double b, double eps = 1e-9) noexcept {
  return std::abs(a - b) <= eps;
}

[[nodiscard]] inline double Dot(Vec2 a, Vec2 b) noexcept {
  return a.x * b.x + a.y * b.y;
}

[[nodiscard]] inline double Cross(Vec2 a, Vec2 b) noexcept {
  return a.x * b.y - a.y * b.x;
}

[[nodiscard]] inline double AngleBetween(Vec2 a, Vec2 b) noexcept {
  const double la = a.length();
  const double lb = b.length();
  if (la <= kEpsilon || lb <= kEpsilon) {
    return 0.0;
  }
  const double c = Clamp(Dot(a, b) / (la * lb), -1.0, 1.0);
  return std::acos(c);
}

[[nodiscard]] inline double Percentile(std::span<const double> sorted, double p) {
  if (sorted.empty()) {
    return 0.0;
  }
  const double idx = Clamp(p, 0.0, 1.0) * static_cast<double>(sorted.size() - 1);
  const auto lo = static_cast<std::size_t>(std::floor(idx));
  const auto hi = static_cast<std::size_t>(std::ceil(idx));
  if (lo == hi) {
    return sorted[lo];
  }
  const double t = idx - static_cast<double>(lo);
  return Lerp(sorted[lo], sorted[hi], t);
}

}  // namespace smoothpad
