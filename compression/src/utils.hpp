#pragma once

constexpr uint64_t rapidhashNano(const void *key, size_t len) noexcept {
    return rapidhashNano_withSeed(key, len, 0);
}