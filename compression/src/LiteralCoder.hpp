#pragma once

#include "MatchCoder.hpp"
#include "Params.hpp"
#include "types.hpp"

template<typename VitterModel, typename MatchCoder>
struct LiteralState {
    MatchCoder matches;
    uint32_t match_zone;
    uint64_t max_code;

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

          first_table(params.first_table, 1, max_code),
          first_end(match_zone + first_table.bucket_count()),
          first_model(first_end + 1),

          gap_table(params.gap_table, 1, max_code - 1),
          gap_end(match_zone + gap_table.bucket_count()),
          gap_model(gap_end + 1) {
    }
};

class LiteralEncoder {
public:
    explicit LiteralEncoder(const LiteralParams &params, uint32_t variable_count);

    void encode(std::span<const Literal> literals, BitWriter &out);

private:
    LiteralState<VitterEncoder, MatchEncoder> state_;
};

class LiteralDecoder {
public:
    explicit LiteralDecoder(const LiteralParams &params, uint32_t variable_count);

    [[nodiscard]] std::vector<Literal> decode(BitReader &in);

private:
    LiteralState<VitterDecoder, MatchDecoder> state_;
};
