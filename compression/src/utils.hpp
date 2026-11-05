#pragma once

#include <cstdint>

[[nodiscard]] constexpr uint64_t zigzag_encode(const int64_t v) noexcept {
    return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63);
}

[[nodiscard]] constexpr int64_t zigzag_decode(const uint64_t u) noexcept {
    return static_cast<int64_t>(u >> 1) ^ -static_cast<int64_t>(u & 1);
}
