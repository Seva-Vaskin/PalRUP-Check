#include "LiteralCoder.hpp"

#include <algorithm>
#include <ranges>

#include "utils.hpp"

namespace {
    void move_clause_to_front(RecencyCache &cache, const std::span<const uint64_t> sorted_codes) {
        for (const uint64_t code: sorted_codes | std::views::reverse) {
            if (!cache.make_most_recent_if_cached(code)) {
                cache.insert(code);
            }
        }
    }
}

LiteralEncoder::LiteralEncoder(const LiteralParams &params, uint32_t variable_count) : state_(params, variable_count) {
}

void LiteralEncoder::encode(const std::span<const Literal> literals, RecencyCache &cache, BitWriter &out) {
    const size_t n = literals.size();
    scratch_codes_.resize(n);
    for (size_t k = 0; k < n; ++k) {
        scratch_codes_[k] = zigzag_encode(literals[k].value);
    }
    std::ranges::sort(scratch_codes_);

    if (n > 0 && scratch_codes_.front() == 0) {
        throw OperationError("literal encoder: literal 0");
    }
    for (size_t k = 1; k < n; ++k) {
        if (scratch_codes_[k] == scratch_codes_[k - 1]) {
            throw OperationError("literal encoder: duplicate literal in clause");
        }
    }

    scratch_single_prefix_.resize(n + 1);
    scratch_single_prefix_[0] = 0;
    for (size_t k = 0; k < n; ++k) {
        const VitterEncoder &model = k == 0 ? state_.first_model : state_.gap_model;
        uint32_t cost = value_cost(k, model);
        if (const auto rank = cache.rank_of(scratch_codes_[k])) {
            cost = std::min(cost, state_.rank_table.cost(*rank, model, state_.match_zone));
        }
        scratch_single_prefix_[k + 1] = scratch_single_prefix_[k] + cost;
    }

    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.gap_model;
        const auto remaining = std::span<const uint64_t>(scratch_codes_).subspan(i);
        const auto prefix = std::span<const uint64_t>(scratch_single_prefix_).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
            continue;
        }

        const auto rank = cache.rank_of(scratch_codes_[i]);
        if (rank && state_.rank_table.cost(*rank, model, state_.match_zone) <= value_cost(i, model)) {
            state_.rank_table.encode(*rank, model, out, state_.match_zone);
        } else if (i == 0) {
            state_.first_table.encode(scratch_codes_[0], model, out, state_.value_zone);
        } else {
            state_.gap_table.encode(scratch_codes_[i] - scratch_codes_[i - 1], model, out, state_.value_zone);
        }
        ++i;
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.gap_model.encode(state_.gap_end, out);
    }

    state_.matches.insert(scratch_codes_);
    move_clause_to_front(cache, scratch_codes_);
}

uint32_t LiteralEncoder::value_cost(const size_t k, const VitterEncoder &model) const noexcept {
    if (k == 0) {
        return state_.first_table.cost(scratch_codes_[0], model, state_.value_zone);
    }
    return state_.gap_table.cost(scratch_codes_[k] - scratch_codes_[k - 1], model, state_.value_zone);
}

LiteralDecoder::LiteralDecoder(const LiteralParams &params, const uint32_t variable_count)
    : state_(params, variable_count) {
}

void LiteralDecoder::decode(BitReader &in, RecencyCache &cache, std::vector<Literal> &out) {
    scratch_codes_.clear();
    for (;;) {
        const bool first = scratch_codes_.empty();
        VitterDecoder &model = first ? state_.first_model : state_.gap_model;
        const uint32_t end = first ? state_.first_end : state_.gap_end;

        const uint32_t symbol = model.decode(in);
        if (symbol == end) {
            break;
        }
        if (symbol < state_.match_zone) {
            state_.matches.decode(symbol, in, scratch_codes_);
        } else if (symbol < state_.value_zone) {
            const uint64_t rank = state_.rank_table.decode_extra(symbol - state_.match_zone, in);
            if (rank >= cache.size()) {
                throw OperationError("literal decoder: corrupt stream (cache rank out of range)");
            }
            scratch_codes_.push_back(cache.key_at_rank(static_cast<uint32_t>(rank)));
        } else if (first) {
            const uint64_t code = state_.first_table.decode_extra(symbol - state_.value_zone, in);
            if (code > Params::kMaxLiteralCode) {
                throw OperationError("literal decoder: corrupt stream (literal out of range)");
            }
            scratch_codes_.push_back(code);
        } else {
            const uint64_t gap = state_.gap_table.decode_extra(symbol - state_.value_zone, in);
            if (gap > Params::kMaxLiteralCode - scratch_codes_.back()) {
                throw OperationError("literal decoder: corrupt stream (literal out of range)");
            }
            scratch_codes_.push_back(scratch_codes_.back() + gap);
        }
        if (scratch_codes_.size() > Params::kMaxListLength) {
            throw OperationError("literal decoder: corrupt stream (clause too long)");
        }
    }

    state_.matches.insert(scratch_codes_);
    move_clause_to_front(cache, scratch_codes_);

    out.clear();
    for (const uint64_t code: scratch_codes_) {
        out.push_back(Literal{static_cast<int32_t>(zigzag_decode(code))});
    }
}
