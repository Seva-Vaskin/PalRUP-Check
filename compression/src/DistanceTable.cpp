#include "DistanceTable.hpp"

#include <bit>

#include "error.hpp"
#include "VitterCoder.hpp"

namespace {
    [[nodiscard]] uint64_t get_escape(BitReader &in) {
        const auto width = static_cast<uint32_t>(in.get_bits(7));
        if (width > 64) {
            throw OperationError("corrupt stream: escape width above 64");
        }
        if (width < 2) {
            return width;
        }
        return (uint64_t{1} << (width - 1)) | in.get_bits(width - 1);
    }

    void put_escape(const uint64_t v, BitWriter &out) noexcept {
        const auto width = static_cast<uint32_t>(std::bit_width(v));
        out.put_bits(width, 7);
        if (width >= 2) {
            out.put_bits(v, width - 1);
        }
    }
}

bool DistanceTable::Config::valid() const noexcept {
    bool ok = true;
    ok &= exact_until <= kMaxExactUntil;
    ok &= std::has_single_bit(groups_per_level) && groups_per_level <= kMaxGroupsPerLevel;
    return ok;
}

DistanceTable::DistanceTable(const Config &config, const uint64_t min_value,
                             const uint64_t max_value) : buckets_(), min_value_(min_value),
                                                         max_value_(max_value) {
    CHECK(min_value_ <= max_value);
    CHECK(max_value_ < (uint64_t{1} << 63));
    CHECK(config.groups_per_level >= 1);

    uint64_t value = min_value_;

    for (uint64_t i = 0; i < config.exact_until && value <= max_value_; ++i, ++value) {
        buckets_.push_back(Bucket{.start = value, .extra_bits = 0});
    }

    for (uint32_t extra = 1; value <= max_value_; ++extra) {
        const uint64_t width = uint64_t{1} << extra;
        for (uint32_t g = 0; g < config.groups_per_level && value <= max_value_; ++g) {
            const uint64_t span_length = std::min(width, max_value_ - value + 1);
            buckets_.push_back(Bucket{
                .start = value, .extra_bits = static_cast<uint32_t>(std::bit_width(span_length - 1))
            });
            value += span_length;
        }
    }

    CHECK(std::has_single_bit(config.groups_per_level)); // bucket_of needs a power of two
    group_shift_ = static_cast<uint32_t>(std::countr_zero(config.groups_per_level));
    exact_count_ = static_cast<uint32_t>(std::min<
        uint64_t>(config.exact_until, max_value_ - min_value_ + 1));
    log_start_ = min_value_ + exact_count_;

    CHECK(bucket_count() <= VitterEncoder::kMaxAlphabetSize);
}

uint64_t DistanceTable::min_value() const noexcept { return min_value_; }

uint64_t DistanceTable::max_value() const noexcept { return max_value_; }

uint32_t DistanceTable::bucket_count() const noexcept { return static_cast<uint32_t>(buckets_.size()) + 1; }

uint32_t DistanceTable::escape_bucket() const noexcept { return static_cast<uint32_t>(buckets_.size()); }

const DistanceTable::Bucket &DistanceTable::bucket(const uint32_t bucket_id) const noexcept {
    return buckets_[bucket_id];
}

uint32_t DistanceTable::bucket_of(const uint64_t value) const noexcept {
    if (value > max_value_) [[unlikely]] {
        return escape_bucket();
    }
    if (value < log_start_) {
        return static_cast<uint32_t>(value - min_value_);
    }
    const uint64_t d = value - log_start_;
    const uint64_t q = d >> (group_shift_ + 1);
    const auto k = static_cast<uint32_t>(std::bit_width(q + 1)) - 1;
    const uint64_t covered = ((uint64_t{1} << k) - 1) << (group_shift_ + 1);
    const uint64_t j = (d - covered) >> (1 + k);
    return exact_count_ + (k << group_shift_) + static_cast<uint32_t>(j);
}

void DistanceTable::encode_extra(const uint64_t value, BitWriter &out) const {
    auto bucket = bucket_of(value);
    if (bucket == escape_bucket()) [[unlikely]] {
        put_escape(value - max_value() - 1, out);
        return;
    }
    CHECK(value >= min_value());
    const auto &b = buckets_[bucket];
    out.put_bits(value - b.start, b.extra_bits);
}

void DistanceTable::encode(uint64_t value, VitterEncoder &model, BitWriter &out, uint32_t offset) const {
    const uint32_t bucket = bucket_of(value);
    model.encode(offset + bucket, out);
    encode_extra(value, out);
}

uint64_t DistanceTable::decode(VitterDecoder &model, BitReader &in) const {
    const uint32_t bucket = model.decode(in);
    return decode_extra(bucket, in);
}

uint64_t DistanceTable::decode_extra(uint32_t bucket, BitReader &in) const {
    CHECK(bucket < bucket_count());
    if (bucket == escape_bucket()) [[unlikely]] {
        return max_value() + 1 + get_escape(in);
    }
    const DistanceTable::Bucket &b = buckets_[bucket];
    return b.start + in.get_bits(b.extra_bits);
}
