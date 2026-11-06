#include "DistanceCoder.hpp"

#include <bit>

#include "error.hpp"
#include "VitterCoder.hpp"

namespace {
    [[nodiscard]] uint64_t get_escape(BitReader &in) {
        const uint32_t width = in.get_bits(7);
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

DistanceTable::DistanceTable(const Config &config) : config_(config), buckets_() {
    CHECK(config.min_value <= config.max_value);
    CHECK(config.max_value < (uint64_t{1} << 63));
    CHECK(config.groups_per_level >= 1);

    uint64_t value = config.min_value;

    for (uint64_t i = 0; i < config.exact_until && value <= config.max_value; ++i, ++value) {
        buckets_.push_back(Bucket{.start = value, .extra_bits = 0});
    }

    for (uint32_t extra = 1; value <= config.max_value; ++extra) {
        const uint64_t width = uint64_t{1} << extra;
        for (uint32_t g = 0; g < config.groups_per_level && value <= config.max_value; ++g) {
            const uint64_t span_length = std::min(width, config.max_value - value + 1);
            buckets_.push_back(Bucket{
                .start = value, .extra_bits = static_cast<uint32_t>(std::bit_width(span_length - 1))
            });
            value += span_length;
        }
    }

    CHECK(std::has_single_bit(config.groups_per_level)); // bucket_of needs a power of two
    group_shift_ = static_cast<uint32_t>(std::countr_zero(config.groups_per_level));
    exact_count_ = static_cast<uint32_t>(std::min<
        uint64_t>(config.exact_until, config.max_value - config.min_value + 1));
    log_start_ = config.min_value + exact_count_;

    CHECK(symbol_count() <= VitterEncoder::kMaxAlphabetSize);
}

uint64_t DistanceTable::min_value() const noexcept { return config_.min_value; }

uint64_t DistanceTable::max_value() const noexcept { return config_.max_value; }

uint32_t DistanceTable::symbol_count() const noexcept { return static_cast<uint32_t>(buckets_.size()) + 1; }

uint32_t DistanceTable::escape_bucket() const noexcept { return static_cast<uint32_t>(buckets_.size()); }

const DistanceTable::Bucket &DistanceTable::bucket(const uint32_t bucket_id) const noexcept {
    return buckets_[bucket_id];
}

uint32_t DistanceTable::bucket_of(const uint64_t value) const noexcept {
    if (value < log_start_) {
        return static_cast<uint32_t>(value - config_.min_value);
    }
    const uint64_t d = value - log_start_;
    const uint64_t q = d >> (group_shift_ + 1);
    const auto k = static_cast<uint32_t>(std::bit_width(q + 1)) - 1;
    const uint64_t covered = ((uint64_t{1} << k) - 1) << (group_shift_ + 1);
    const uint64_t j = (d - covered) >> (1 + k);
    return exact_count_ + (k << group_shift_) + static_cast<uint32_t>(j);
}

DistanceEncoder::DistanceEncoder(const DistanceTable::Config &config) : table_(config), model_(table_.symbol_count()) {
}

void DistanceEncoder::encode(const uint64_t value, BitWriter &out) noexcept {
    if (value > table_.max_value()) [[unlikely]] {
        model_.encode(table_.escape_bucket(), out);
        put_escape(value - table_.max_value() - 1, out);
    }
    CHECK(value >= table_.min_value());
    const uint32_t bucket_id = table_.bucket_of(value);
    model_.encode(bucket_id, out);
    const auto &b = table_.bucket(bucket_id);
    out.put_bits(value - b.start, b.extra_bits);
}

DistanceDecoder::DistanceDecoder(const DistanceTable::Config &config) : table_(config), model_(table_.symbol_count()) {
}

uint64_t DistanceDecoder::decode(BitReader &in) {
    const uint32_t bucket_id = model_.decode(in);
    if (bucket_id == table_.escape_bucket()) [[unlikely]] {
        return table_.max_value() + 1 + get_escape(in);
    }
    const DistanceTable::Bucket &b = table_.bucket(bucket_id);
    return b.start + in.get_bits(b.extra_bits);
}
