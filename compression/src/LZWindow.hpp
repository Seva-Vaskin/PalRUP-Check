#pragma once

#include "error.hpp"

#include <span>
#include <cstdint>
#include <vector>

class LZWindow {
public:
    static constexpr uint8_t MIN_WINDOW_LOG = 1;
    static constexpr uint8_t MAX_WINDOW_LOG = 24;

    static constexpr uint8_t MIN_HASH_BITS = 1;
    static constexpr uint8_t MAX_HASH_BITS = 24;

    static constexpr uint64_t NO_POSITION = UINT64_MAX;
    static constexpr uint64_t MIN_MATCH = 2;
    static constexpr uint32_t MAX_MATCH = 1u << 16;
    static_assert(MIN_MATCH == 2); // hash mechanism depends on MIN_MATCH=2

    struct Config {
        uint8_t window_log;
        uint8_t hash_bits;
        uint32_t max_match;
        uint32_t nice_match;
        uint32_t max_search_depth;

        [[nodiscard]] constexpr bool valid() const noexcept;

        void fields(this auto &self, auto &&f);
    };

    struct Match {
        uint32_t length;
        uint32_t distance;
    };

    explicit LZWindow(const Config &config);

    [[nodiscard]] std::span<Match> find_matches(std::span<const uint64_t> tokens, std::span<Match> output) const;

    void insert(std::span<const uint64_t> tokens);

private:
    Config config_;
    uint32_t window_mask_;
    uint32_t hash_mask_;
    uint64_t total_ = 0;

    std::vector<uint64_t> window_;
    std::vector<uint64_t> head_;
    std::vector<uint64_t> prev_;

    static constexpr Config check_config(const Config &config);

    [[nodiscard]] uint32_t match_length(uint64_t pos, std::span<const uint64_t> tokens, uint64_t length_limit) const;

};
