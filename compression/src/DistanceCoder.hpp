#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "VitterCoder.hpp"

class DistanceTable {
public:
    struct Config {
        uint64_t min_value;
        uint64_t exact_until;
        uint32_t groups_per_level;
        uint64_t max_value;
    };

    struct Bucket {
        uint64_t start;
        uint32_t extra_bits;
    };

    explicit DistanceTable(const Config &config);

    [[nodiscard]] uint64_t min_value() const noexcept;

    [[nodiscard]] uint64_t max_value() const noexcept;

    [[nodiscard]] uint32_t symbol_count() const noexcept;

    [[nodiscard]] uint32_t escape_bucket() const noexcept;

    [[nodiscard]] const Bucket &bucket(uint32_t bucket_id) const noexcept;

    [[nodiscard]] uint32_t bucket_of(uint64_t value) const noexcept;

private:
    Config config_;
    std::vector<Bucket> buckets_;
    uint64_t log_start_;
    uint32_t exact_count_;
    uint32_t group_shift_;
};


class DistanceEncoder {
public:
    explicit DistanceEncoder(const DistanceTable::Config &config);;

    void encode(uint64_t value, BitWriter &out) noexcept;

private:
    DistanceTable table_;
    VitterEncoder model_;
};

class DistanceDecoder {
public:
    explicit DistanceDecoder(const DistanceTable::Config &config);

    [[nodiscard]] uint64_t decode(BitReader &in);

private:
    DistanceTable table_;
    VitterDecoder model_;
};