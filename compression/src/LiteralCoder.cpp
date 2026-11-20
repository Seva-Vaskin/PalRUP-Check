#include "LiteralCoder.hpp"

#include <algorithm>

#include "utils.hpp"

LiteralEncoder::LiteralEncoder(const LiteralParams &params, uint32_t variable_count) : state_(params, variable_count) {
}

void LiteralEncoder::encode(const std::span<const Literal> literals, BitWriter &out) {
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
        const uint32_t cost = k == 0
                                  ? state_.first_table.cost(scratch_codes_[0], state_.first_model, state_.match_zone)
                                  : state_.gap_table.cost(scratch_codes_[k] - scratch_codes_[k - 1], state_.gap_model,
                                                          state_.match_zone);
        scratch_single_prefix_[k + 1] = scratch_single_prefix_[k] + cost;
    }

    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.gap_model;
        const auto remaining = std::span<const uint64_t>(scratch_codes_).subspan(i);
        const auto prefix = std::span<const uint64_t>(scratch_single_prefix_).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
        } else if (i == 0) {
            state_.first_table.encode(scratch_codes_[0], model, out, state_.match_zone);
            ++i;
        } else {
            state_.gap_table.encode(scratch_codes_[i] - scratch_codes_[i - 1], model, out, state_.match_zone);
            ++i;
        }
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.gap_model.encode(state_.gap_end, out);
    }

    state_.matches.insert(scratch_codes_);
}

LiteralDecoder::LiteralDecoder(const LiteralParams &params, const uint32_t variable_count)
    : state_(params, variable_count) {
}

void LiteralDecoder::decode(BitReader &in, std::vector<Literal> &out) {
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
        } else if (first) {
            const uint64_t code = state_.first_table.decode_extra(symbol - state_.match_zone, in);
            if (code > Params::kMaxLiteralCode) {
                throw OperationError("literal decoder: corrupt stream (literal out of range)");
            }
            scratch_codes_.push_back(code);
        } else {
            const uint64_t gap = state_.gap_table.decode_extra(symbol - state_.match_zone, in);
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

    out.clear();
    for (const uint64_t code: scratch_codes_) {
        out.push_back(Literal{static_cast<int32_t>(zigzag_decode(code))});
    }
}
