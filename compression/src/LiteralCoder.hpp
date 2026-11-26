#pragma once

#include <span>
#include <vector>

#include "MatchCoder.hpp"
#include "Params.hpp"
#include "RecencyCache.hpp"
#include "types.hpp"

// Literals of one clause as zigzag codes, sorted ascending:
//
//   [0, match_zone)            MATCH  copy of an earlier code run
//   [match_zone, value_zone)   HIT    recency rank of the code in the literal cache
//   [value_zone, end)          VALUE  first_model: code
//                                     gap_model:   code - prev, however prev was coded
//   end                        END    first_model: empty clause; gap_model: clause is over
template<typename VitterModel, typename MatchCoder>
struct LiteralState {
    MatchCoder matches;
    uint32_t match_zone;
    uint64_t max_code;

    DistanceTable rank_table;
    uint32_t value_zone;

    DistanceTable first_table;
    uint32_t first_end;
    VitterModel first_model;

    DistanceTable gap_table;
    uint32_t gap_end;
    VitterModel gap_model;

    LiteralState(const LiteralParams &params, const uint32_t variable_count)
        : matches(params.lz),
          match_zone(matches.zone_size()),
          max_code(2 * uint64_t{variable_count}),

          rank_table(params.rank_table, 0, (uint64_t{1} << params.cache_bits) - 1),
          value_zone(match_zone + rank_table.bucket_count()),

          first_table(params.first_table, 1, max_code),
          first_end(value_zone + first_table.bucket_count()),
          first_model(first_end + 1),

          gap_table(params.gap_table, 1, max_code - 1),
          gap_end(value_zone + gap_table.bucket_count()),
          gap_model(gap_end + 1) {
    }
};

class LiteralEncoder {
public:
    explicit LiteralEncoder(const LiteralParams &params, uint32_t variable_count);

    void encode(std::span<const Literal> literals, RecencyCache &cache, BitWriter &out);

private:
    LiteralState<VitterEncoder, MatchEncoder> state_;
    std::vector<uint64_t> scratch_codes_;
    std::vector<uint64_t> scratch_single_prefix_;

    [[nodiscard]] uint32_t value_cost(size_t k, const VitterEncoder &model) const noexcept;
};

class LiteralDecoder {
public:
    explicit LiteralDecoder(const LiteralParams &params, uint32_t variable_count);

    void decode(BitReader &in, RecencyCache &cache, std::vector<Literal> &out);

private:
    LiteralState<VitterDecoder, MatchDecoder> state_;
    std::vector<uint64_t> scratch_codes_;
};
