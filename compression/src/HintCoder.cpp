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
    std::vector<uint64_t> tokens(n), single_prefix(n + 1);
    for (size_t k = 0; k < n; ++k) {
        tokens[k] = hints[k].value;
    }

    single_prefix.resize(n + 1);
    single_prefix[0] = 0;
    for (size_t k = 0; k < n; ++k) {
        const DistanceTable &table = k == 0 ? state_.first_table : state_.rest_table;
        const VitterEncoder &model = k == 0 ? state_.first_model : state_.rest_model;
        const uint64_t base = k == 0 ? id : tokens[k - 1];
        single_prefix[k + 1] = single_prefix[k] + table.cost(delta(base, tokens[k]), model, state_.match_zone);
    }

    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.rest_model;
        const auto remaining = std::span<const uint64_t>(tokens).subspan(i);
        const auto prefix = std::span<const uint64_t>(single_prefix).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
        } else {
            const DistanceTable &table = i == 0 ? state_.first_table : state_.rest_table;
            const uint64_t base = i == 0 ? id : tokens[i - 1];
            table.encode(delta(base, tokens[i]), model, out, state_.match_zone);
            ++i;
        }
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.rest_model.encode(state_.rest_end, out);
    }

    state_.matches.insert(tokens);
}

HintDecoder::HintDecoder(const HintParams &params) : state_(params) {
}

std::vector<ClauseId> HintDecoder::decode(const uint64_t id, BitReader &in) {
    std::vector<uint64_t> tokens;
    for (;;) {
        const bool first = tokens.empty();
        VitterDecoder &model = first ? state_.first_model : state_.rest_model;
        const uint32_t end = first ? state_.first_end : state_.rest_end;

        const uint32_t symbol = model.decode(in);
        if (symbol == end) {
            break;
        }
        if (symbol < state_.match_zone) {
            state_.matches.decode(symbol, in, tokens);
        } else {
            const DistanceTable &table = first ? state_.first_table : state_.rest_table;
            const uint64_t base = first ? id : tokens.back();
            tokens.push_back(undelta(base, table.decode_extra(symbol - state_.match_zone, in)));
        }
        if (tokens.size() > Params::kMaxListLength) {
            throw OperationError("hint decoder: corrupt stream (too many hints)");
        }
    }

    state_.matches.insert(tokens);

    std::vector<ClauseId> hints;
    hints.reserve(tokens.size());
    for (const uint64_t token: tokens) {
        hints.push_back(ClauseId{token});
    }
    return hints;
}
