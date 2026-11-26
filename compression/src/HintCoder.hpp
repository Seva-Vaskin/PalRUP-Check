#pragma once

#include <span>
#include <vector>

#include "MatchCoder.hpp"
#include "Params.hpp"
#include "ClauseIdCache.hpp"
#include "types.hpp"

// Hints of one Add, order kept:
//
//   [0, match_zone)            MATCH  copy of an earlier hint run
//   [match_zone, delta_zone)   HIT    recency rank of the hint in the clause-id cache
//   [delta_zone, end)          DELTA  first_model: zigzag(id - hint)
//                                     rest_model:  zigzag(prev - hint)
//   end                        END    first_model: no hints; rest_model: hints are over

template<typename VitterModel, typename MatchCoder>
struct HintState {
    MatchCoder matches;
    uint32_t match_zone;

    DistanceTable rank_table;
    uint32_t delta_zone;

    DistanceTable first_table;
    uint32_t first_end;
    VitterModel first_model;

    DistanceTable rest_table;
    uint32_t rest_end;
    VitterModel rest_model;

    HintState(const HintParams &params, const CacheParams &cache)
        : matches(params.lz),
          match_zone(matches.zone_size()),

          rank_table(cache.rank_table, 0, (uint64_t{1} << cache.cache_bits) - 1),
          delta_zone(match_zone + rank_table.bucket_count()),

          first_table(params.first_table, 0, Params::kMaxIdDelta),
          first_end(delta_zone + first_table.bucket_count()),
          first_model(first_end + 1),

          rest_table(params.rest_table, 0, Params::kMaxIdDelta),
          rest_end(delta_zone + rest_table.bucket_count()),
          rest_model(rest_end + 1) {
    }
};

class HintEncoder {
public:
    HintEncoder(const HintParams &params, const CacheParams &cache);

    void encode(std::span<const ClauseId> hints, uint64_t id, ClauseIdCache &cache, BitWriter &out);

private:
    HintState<VitterEncoder, MatchEncoder> state_;
    std::vector<uint64_t> scratch_tokens_;
    std::vector<uint64_t> scratch_single_prefix_;
    std::vector<bool> scratch_cached_;
    std::vector<uint64_t> scratch_deferred_;
};

class HintDecoder {
public:
    HintDecoder(const HintParams &params, const CacheParams &cache);

    void decode(uint64_t id, ClauseIdCache &cache, BitReader &in, std::vector<ClauseId> &out);

private:
    HintState<VitterDecoder, MatchDecoder> state_;
    std::vector<uint64_t> scratch_tokens_;
    std::vector<uint64_t> scratch_deferred_;
};
