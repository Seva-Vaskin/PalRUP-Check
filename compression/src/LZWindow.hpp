#pragma once

#include "error.hpp"

#include <span>
#include <cstdint>
#include <vector>

class LZWindow {
public:
    struct Config {
        uint8_t window_log;
        uint8_t hash_bits;
        uint32_t max_match;
        uint32_t nice_match;
        uint32_t max_search_depth;
    };

    static constexpr uint8_t min_window_log = 1;
    static constexpr uint8_t max_window_log = 24;

    static constexpr uint8_t min_hash_bits = 1;
    static constexpr uint8_t max_hash_bits = 24;

    static constexpr bool valid_window_log(const uint8_t window_log) noexcept {
        return min_window_log <= window_log && window_log <= max_window_log;
    }

    static constexpr bool valid_hash_bits(const uint8_t hash_bits) noexcept {
        return min_hash_bits <= hash_bits && hash_bits <= max_hash_bits;
    }

    struct Match {
        uint32_t length;
        uint32_t distance;
    };

private:
    Config config_;
    uint32_t window_mask_;
    uint32_t hash_mask_;
    uint64_t total_ = 0;

    std::vector<uint64_t> window_;
    std::vector<uint64_t> head_;
    std::vector<uint64_t> prev_;

    static constexpr Config check_config(const Config &config) {
        CHECK(valid_window_log(config.window_log));
        CHECK(valid_hash_bits(config.hash_bits));
        return config;
    }

    static constexpr uint64_t NO_POSITION = UINT64_MAX;
    static constexpr uint64_t MIN_MATCH = 2;
    static_assert(MIN_MATCH == 2); // hash mechanism depends on MIN_MATCH=2


    [[nodiscard]] uint32_t match_length(uint64_t pos, std::span<const uint64_t> tokens, uint64_t length_limit) const;

public:
    explicit LZWindow(const Config &config)
        : config_(check_config(config)),
          window_mask_((1u << config.window_log) - 1u),
          hash_mask_((1u << config.hash_bits) - 1u),
          window_(1u << config.window_log),
          head_(1u << config.hash_bits, NO_POSITION),
          prev_(1u << config.window_log) {
    }

    [[nodiscard]] std::span<Match> find_matches(std::span<const uint64_t> tokens, std::span<Match> output) const;

    void insert(std::span<const uint64_t> tokens);
};
