#include "LZWindow.hpp"

#include <algorithm>

namespace {
    [[nodiscard]]
    constexpr uint64_t rapid_mix(
        uint64_t a,
        uint64_t b) noexcept {
        const auto p = static_cast<unsigned __int128>(a) * b;
        return static_cast<uint64_t>(p)
               ^ static_cast<uint64_t>(p >> 64);
    }

    //
    // /**
    //  * @brief Hashes exactly four uint64_t values into a 64-bit hash.
    //  *
    //  * Ported and specialized from rapidhashNano V3 by Nicolas De Carli.
    //  * Original source (permalink):
    //  * https://github.com/Nicoshev/rapidhash/blob/bc4b4baa48a15ff52ff4725e1ccdcda62815221c/rapidhash.h
    //  * Copyright (C) 2025 Nicolas De Carli. MIT License.
    //  */
    // [[nodiscard]]
    // constexpr uint64_t hash4(
    //     const uint64_t x0,
    //     const uint64_t x1,
    //     const uint64_t x2,
    //     const uint64_t x3) noexcept {
    //     constexpr uint64_t s1 = 0x8bb84b93962eacc9ULL;
    //     constexpr uint64_t s2 = 0x4b33a62ed433d4a3ULL;
    //     constexpr uint64_t s7 = 0xaaaaaaaaaaaaaaaaULL;
    //
    //     uint64_t seed = rapid_mix(s2, s1);
    //     seed = rapid_mix(x0 ^ s2, x1 ^ seed);
    //
    //     const auto p =
    //             static_cast<unsigned __int128>((x2 ^ 32ULL) ^ s1)
    //             * (x3 ^ seed);
    //
    //     const auto lo = static_cast<uint64_t>(p);
    //     const auto hi = static_cast<uint64_t>(p >> 64);
    //
    //     return rapid_mix(lo ^ s7, hi ^ s1 ^ 32ULL);
    // }
    //
    // [[nodiscard]]
    // constexpr uint32_t hash4_masked(
    //     const uint64_t x0,
    //     const uint64_t x1,
    //     const uint64_t x2,
    //     const uint64_t x3,
    //     const uint32_t hash_mask
    // ) {
    //     return static_cast<uint32_t>(hash4(x0, x1, x2, x3) & hash_mask);
    // }


    /**
     * @brief Hashes exactly two uint64_t values into a 64-bit hash.
     *
     * Ported and specialized from rapidhashNano V3 by Nicolas De Carli.
     * Original source:
     * https://github.com/Nicoshev/rapidhash/blob/bc4b4baa48a15ff52ff4725e1ccdcda62815221c/rapidhash.h
     *
     * Copyright (C) 2025 Nicolas De Carli. MIT License.
     */
    [[nodiscard]]
    constexpr std::uint64_t hash2(
        std::uint64_t x0,
        std::uint64_t x1) noexcept {
        constexpr std::uint64_t s1 = 0x8bb84b93962eacc9ULL;
        constexpr std::uint64_t s2 = 0x4b33a62ed433d4a3ULL;
        constexpr std::uint64_t s7 = 0xaaaaaaaaaaaaaaaaULL;

        constexpr std::uint64_t seed = rapid_mix(s2, s1) ^ 16ULL;

        const auto p =
                static_cast<unsigned __int128>(x0 ^ s1)
                * (x1 ^ seed);

        const auto lo = static_cast<std::uint64_t>(p);
        const auto hi = static_cast<std::uint64_t>(p >> 64);

        return rapid_mix(lo ^ s7, hi ^ s1 ^ 16ULL);
    }
}

uint32_t LZWindow::match_length(const uint64_t pos,
                                const std::span<const uint64_t> tokens,
                                const uint64_t length_limit) const {
    uint32_t length = 0;
    while (length < length_limit && window_[(pos + length) & window_mask_] == tokens[length]) {
        length++;
    }
    return length;
}

std::span<LZWindow::Match> LZWindow::find_matches(std::span<const uint64_t> tokens, std::span<Match> output) const {
    if (tokens.size() < MIN_MATCH || output.empty()) {
        return output.first(0);
    }

    const uint64_t window_size = window_.size();
    const uint64_t length_limit = std::min<uint64_t>(config_.max_match, tokens.size());

    uint32_t match_count = 0;
    uint32_t best_match = MIN_MATCH - 1;

    uint64_t cur = head_[hash2(tokens[0], tokens[1]) & hash_mask_];

    for (uint32_t probe = 0; probe < config_.max_search_depth; probe++) {
        if (cur == NO_POSITION) break;

        uint64_t distance = total_ - cur;
        if (distance > window_size) break;

        const uint32_t length = match_length(cur, tokens, std::min(distance, length_limit));

        if (length > best_match) {
            best_match = length;
            CHECK(match_count < output.size());

            output[match_count++] = Match{.length = length, .distance = static_cast<uint32_t>(distance)};
            if (length >= config_.nice_match || length >= length_limit) break;
        }

        const uint64_t next = prev_[cur & window_mask_];
        if (next >= cur) break;
        cur = next;
    }

    return output.first(match_count);
}

void LZWindow::insert(const std::span<const uint64_t> tokens) {
    uint64_t p = total_ >= MIN_MATCH - 1 ? total_ - MIN_MATCH + 1 : 0;
    for (const uint64_t token: tokens) {
        window_[total_ & window_mask_] = token;
        ++total_;
    }

    for (; p + MIN_MATCH <= total_; p++) {
        const auto h = hash2(window_[p & window_mask_], window_[(p + 1) & window_mask_]) & hash_mask_;
        prev_[p & window_mask_] = head_[h];
        head_[h] = p;
    }
}
