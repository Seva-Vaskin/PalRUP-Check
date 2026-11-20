#pragma once

#include "error.hpp"

#include <span>
#include <cstdint>
#include <vector>


class LZHistory {
public:
    explicit LZHistory(uint8_t window_log);

    void insert(std::span<const uint64_t> tokens) noexcept;

    [[nodiscard]] uint64_t total() const noexcept { return total_; }

    [[nodiscard]] uint64_t capacity() const noexcept { return ring_.size(); }

    [[nodiscard]] uint64_t token(uint64_t position) const noexcept { return ring_[position & mask_]; }


    [[nodiscard]] uint64_t size() const noexcept { return std::min(total_, capacity()); }

private:
    std::vector<uint64_t> ring_;
    uint64_t mask_;
    uint64_t total_ = 0;
};



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

        [[nodiscard]] constexpr bool valid() const noexcept {
            bool ok = true;
            ok &= MIN_WINDOW_LOG <= window_log && window_log <= MAX_WINDOW_LOG;
            ok &= MIN_HASH_BITS <= hash_bits && hash_bits <= MAX_HASH_BITS;
            ok &= MIN_MATCH <= max_match && max_match <= MAX_MATCH;
            ok &= MIN_MATCH <= nice_match && nice_match <= max_match;
            ok &= max_search_depth >= 1;
            return ok;
        }

        void fields(this auto &self, auto &&f) {
            f(self.window_log);
            f(self.hash_bits);
            f(self.max_match);
            f(self.nice_match);
            f(self.max_search_depth);
        }
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

    LZHistory history_;
    std::vector<uint64_t> head_;
    std::vector<uint64_t> prev_;

    static constexpr Config check_config(const Config &config);

    [[nodiscard]] uint32_t match_length(uint64_t pos, std::span<const uint64_t> tokens, uint64_t length_limit) const;

    void update_hash(uint64_t start_pos);
};
