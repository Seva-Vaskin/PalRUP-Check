#include "HintCoder.hpp"

#include <algorithm>

#include "utils.hpp"

namespace {
    [[nodiscard]] uint64_t delta(const uint64_t base, const uint64_t hint) noexcept {
        return zigzag_encode(static_cast<int64_t>(base - hint));
    }

    [[nodiscard]] uint64_t undelta(const uint64_t base, const uint64_t code) noexcept {
        return base - static_cast<uint64_t>(zigzag_decode(code));
    }
}

HintEncoder::HintEncoder(const HintParams &params, const CacheParams &cache) : state_(params, cache) {
}

void HintEncoder::encode(const std::span<const ClauseId> hints, const uint64_t id, ClauseIdCache &cache,
                         BitWriter &out) {
    if (hints.size() > Params::kMaxListLength) {
        throw OperationError("hint encoder: Add has more than kMaxListLength hints");
    }
    const size_t n = hints.size();
    scratch_tokens_.resize(n);
    for (size_t k = 0; k < n; ++k) {
        scratch_tokens_[k] = hints[k].value;
    }

    scratch_single_prefix_.resize(n + 1);
    scratch_single_prefix_[0] = 0;
    for (size_t k = 0; k < n; ++k) {
        const DistanceTable &table = k == 0 ? state_.first_table : state_.rest_table;
        const VitterEncoder &model = k == 0 ? state_.first_model : state_.rest_model;
        const uint64_t base = k == 0 ? id : scratch_tokens_[k - 1];
        uint32_t cost = table.cost(delta(base, scratch_tokens_[k]), model, state_.delta_zone);
        if (const auto rank = cache.rank_of(scratch_tokens_[k])) {
            cost = std::min(cost, state_.rank_table.cost(*rank, model, state_.match_zone));
        }
        scratch_single_prefix_[k + 1] = scratch_single_prefix_[k] + cost;
    }

    scratch_deferred_.clear();
    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.rest_model;
        const auto remaining = std::span<const uint64_t>(scratch_tokens_).subspan(i);
        const auto prefix = std::span<const uint64_t>(scratch_single_prefix_).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
            continue;
        }

        const uint64_t hint = scratch_tokens_[i];
        const DistanceTable &table = i == 0 ? state_.first_table : state_.rest_table;
        const uint64_t code = delta(i == 0 ? id : scratch_tokens_[i - 1], hint);
        const auto rank = cache.rank_of(hint);
        if (rank && state_.rank_table.cost(*rank, model, state_.match_zone)
            <= table.cost(code, model, state_.delta_zone)) {
            state_.rank_table.encode(*rank, model, out, state_.match_zone);
        } else {
            table.encode(code, model, out, state_.delta_zone);
        }

        if (rank) {
            cache.make_most_recent(hint);
        } else {
            scratch_deferred_.push_back(hint);
        }
        ++i;
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.rest_model.encode(state_.rest_end, out);
    }

    state_.matches.insert(scratch_tokens_);
    for (const uint64_t hint: scratch_deferred_) {
        cache.insert(hint);
    }
}

HintDecoder::HintDecoder(const HintParams &params, const CacheParams &cache) : state_(params, cache) {
}

void HintDecoder::decode(const uint64_t id, ClauseIdCache &cache, BitReader &in, std::vector<ClauseId> &out) {
    scratch_tokens_.clear();
    scratch_deferred_.clear();
    for (;;) {
        const bool first = scratch_tokens_.empty();
        VitterDecoder &model = first ? state_.first_model : state_.rest_model;
        const uint32_t end = first ? state_.first_end : state_.rest_end;

        const uint32_t symbol = model.decode(in);
        if (symbol == end) {
            break;
        }
        if (symbol < state_.match_zone) {
            state_.matches.decode(symbol, in, scratch_tokens_);
        } else if (symbol < state_.delta_zone) {
            const uint64_t rank = state_.rank_table.decode_extra(symbol - state_.match_zone, in);
            if (rank >= cache.size()) {
                throw OperationError("hint decoder: corrupt stream (cache rank out of range)");
            }
            const uint64_t hint = cache.id_at_rank(static_cast<uint32_t>(rank));
            cache.make_most_recent(hint);
            scratch_tokens_.push_back(hint);
        } else {
            const DistanceTable &table = first ? state_.first_table : state_.rest_table;
            const uint64_t base = first ? id : scratch_tokens_.back();
            const uint64_t hint = undelta(base, table.decode_extra(symbol - state_.delta_zone, in));
            if (!cache.make_most_recent_if_cached(hint)) {
                scratch_deferred_.push_back(hint);
            }
            scratch_tokens_.push_back(hint);
        }
        if (scratch_tokens_.size() > Params::kMaxListLength) {
            throw OperationError("hint decoder: corrupt stream (too many hints)");
        }
    }

    state_.matches.insert(scratch_tokens_);
    for (const uint64_t hint: scratch_deferred_) {
        cache.insert(hint);
    }

    out.clear();
    for (const uint64_t token: scratch_tokens_) {
        out.push_back(ClauseId{token});
    }
}
