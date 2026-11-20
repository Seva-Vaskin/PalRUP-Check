#include "HintCoder.hpp"

#include "utils.hpp"

namespace {
    [[nodiscard]] uint64_t delta(const uint64_t base, const uint64_t hint) noexcept {
        return zigzag_encode(static_cast<int64_t>(base - hint));
    }

    [[nodiscard]] uint64_t undelta(const uint64_t base, const uint64_t code) noexcept {
        return base - static_cast<uint64_t>(zigzag_decode(code));
    }
}

HintEncoder::HintEncoder(const HintParams &params) : state_(params) {
}

void HintEncoder::encode(const std::span<const ClauseId> hints, const uint64_t id, BitWriter &out) {
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
        const uint32_t cost = table.cost(delta(base, scratch_tokens_[k]), model, state_.match_zone);
        scratch_single_prefix_[k + 1] = scratch_single_prefix_[k] + cost;
    }

    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.rest_model;
        const auto remaining = std::span<const uint64_t>(scratch_tokens_).subspan(i);
        const auto prefix = std::span<const uint64_t>(scratch_single_prefix_).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
        } else {
            const DistanceTable &table = i == 0 ? state_.first_table : state_.rest_table;
            const uint64_t base = i == 0 ? id : scratch_tokens_[i - 1];
            table.encode(delta(base, scratch_tokens_[i]), model, out, state_.match_zone);
            ++i;
        }
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.rest_model.encode(state_.rest_end, out);
    }

    state_.matches.insert(scratch_tokens_);
}

HintDecoder::HintDecoder(const HintParams &params) : state_(params) {
}

void HintDecoder::decode(const uint64_t id, BitReader &in, std::vector<ClauseId> &out) {
    scratch_tokens_.clear();
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
        } else {
            const DistanceTable &table = first ? state_.first_table : state_.rest_table;
            const uint64_t base = first ? id : scratch_tokens_.back();
            scratch_tokens_.push_back(undelta(base, table.decode_extra(symbol - state_.match_zone, in)));
        }
        if (scratch_tokens_.size() > Params::kMaxListLength) {
            throw OperationError("hint decoder: corrupt stream (too many hints)");
        }
    }

    state_.matches.insert(scratch_tokens_);

    out.clear();
    for (const uint64_t token: scratch_tokens_) {
        out.push_back(ClauseId{token});
    }
}
