#pragma once

#include <span>
#include <vector>

#include "MatchCoder.hpp"
#include "Params.hpp"
#include "types.hpp"

template<typename VitterModel, typename MatchCoder>
struct HintState {
    MatchCoder matches;
    uint32_t match_zone;

    DistanceTable first_table;
    uint32_t first_end;
    VitterModel first_model;

    DistanceTable rest_table;
    uint32_t rest_end;
    VitterModel rest_model;

    explicit HintState(const HintParams &params)
        : matches(params.lz),
          match_zone(matches.zone_size()),

          first_table(params.first_table, 0, Params::kMaxIdDelta),
          first_end(match_zone + first_table.bucket_count()),
          first_model(first_end + 1),

          rest_table(params.rest_table, 0, Params::kMaxIdDelta),
          rest_end(match_zone + rest_table.bucket_count()),
          rest_model(rest_end + 1) {
    }
};

class HintEncoder {
public:
    explicit HintEncoder(const HintParams &params);

    void encode(std::span<const ClauseId> hints, uint64_t id, BitWriter &out);

private:
    HintState<VitterEncoder, MatchEncoder> state_;
};

class HintDecoder {
public:
    explicit HintDecoder(const HintParams &params);

    [[nodiscard]] std::vector<ClauseId> decode(uint64_t id, BitReader &in);

private:
    HintState<VitterDecoder, MatchDecoder> state_;
};