#pragma once
#include <vector>

#include "VitterCoder.hpp"

class DistanceTable {
public:
    struct Config {
        uint64_t exact_until;
        uint32_t groups_per_level;

        static constexpr uint64_t kMaxExactUntil = uint64_t{1} << 21;
        static constexpr uint32_t kMaxGroupsPerLevel = 4096;

        [[nodiscard]] bool valid() const noexcept;

        void fields(this auto &self, auto &&f) {
            f(self.exact_until);
            f(self.groups_per_level);
        }
    };

    struct Bucket {
        uint64_t start;
        uint32_t extra_bits;
    };

    explicit DistanceTable(const Config &config, uint64_t min_value, uint64_t max_value);

    [[nodiscard]] uint64_t min_value() const noexcept;

    [[nodiscard]] uint64_t max_value() const noexcept;

    [[nodiscard]] uint32_t bucket_count() const noexcept;

    [[nodiscard]] uint32_t escape_bucket() const noexcept;

    [[nodiscard]] const Bucket &bucket(uint32_t bucket_id) const noexcept;

    [[nodiscard]] uint32_t bucket_of(uint64_t value) const noexcept;

    void encode_extra(uint64_t value, BitWriter &out) const;

    void encode(uint64_t value, VitterEncoder& model, BitWriter &out, uint32_t offset = 0) const;

    uint64_t decode(VitterDecoder& model, BitReader &in) const;

    uint64_t decode_extra(uint32_t bucket, BitReader &in) const;

private:
    Config config_;
    std::vector<Bucket> buckets_;
    uint64_t log_start_;
    uint32_t exact_count_;
    uint32_t group_shift_;
    uint64_t min_value_;
    uint64_t max_value_;
};
