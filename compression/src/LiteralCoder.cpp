#include "LiteralCoder.hpp"

#include <algorithm>

#include "utils.hpp"

LiteralEncoder::LiteralEncoder(const LiteralParams &params, uint32_t variable_count) : state_(params, variable_count) {
}

void LiteralEncoder::encode(const std::span<const Literal> literals, BitWriter &out) {
    const size_t n = literals.size();
    std::vector<uint64_t> codes(n);
    for (size_t k = 0; k < n; ++k) {
        codes[k] = zigzag_encode(literals[k].value);
    }
    std::ranges::sort(codes);

    if (n > 0 && codes.front() == 0) {
        throw OperationError("literal encoder: literal 0");
    }
    for (size_t k = 1; k < n; ++k) {
        if (codes[k] == codes[k - 1]) {
            throw OperationError("literal encoder: duplicate literal in clause");
        }
    }

    std::vector<uint64_t> single_prefix(n + 1);
    for (size_t k = 0; k < n; ++k) {
        const uint32_t cost = k == 0
                                  ? state_.first_table.cost(codes[0], state_.first_model, state_.match_zone)
                                  : state_.gap_table.cost(codes[k] - codes[k - 1], state_.gap_model,
                                                          state_.match_zone);
        single_prefix[k + 1] = single_prefix[k] + cost;
    }

    for (size_t i = 0; i < n;) {
        VitterEncoder &model = i == 0 ? state_.first_model : state_.gap_model;
        const auto remaining = std::span<const uint64_t>(codes).subspan(i);
        const auto prefix = std::span<const uint64_t>(single_prefix).subspan(i);

        if (const auto match = state_.matches.best(remaining, prefix, model)) {
            state_.matches.encode(*match, model, out);
            i += match->length;
        } else if (i == 0) {
            state_.first_table.encode(codes[0], model, out, state_.match_zone);
            ++i;
        } else {
            state_.gap_table.encode(codes[i] - codes[i - 1], model, out, state_.match_zone);
            ++i;
        }
    }

    if (n == 0) {
        state_.first_model.encode(state_.first_end, out);
    } else {
        state_.gap_model.encode(state_.gap_end, out);
    }

    state_.matches.insert(codes);
}

LiteralDecoder::LiteralDecoder(const LiteralParams &params, const uint32_t variable_count)
    : state_(params, variable_count) {
}

std::vector<Literal> LiteralDecoder::decode(BitReader &in) {
    std::vector<uint64_t> codes;
    for (;;) {
        const bool first = codes.empty();
        VitterDecoder &model = first ? state_.first_model : state_.gap_model;
        const uint32_t end = first ? state_.first_end : state_.gap_end;

        const uint32_t symbol = model.decode(in);
        if (symbol == end) {
            break;
        }
        if (symbol < state_.match_zone) {
            state_.matches.decode(symbol, in, codes);
        } else if (first) {
            const uint64_t code = state_.first_table.decode_extra(symbol - state_.match_zone, in);
            if (code > Params::kMaxLiteralCode) {
                throw OperationError("literal decoder: corrupt stream (literal out of range)");
            }
            codes.push_back(code);
        } else {
            const uint64_t gap = state_.gap_table.decode_extra(symbol - state_.match_zone, in);
            if (gap > Params::kMaxLiteralCode - codes.back()) {
                throw OperationError("literal decoder: corrupt stream (literal out of range)");
            }
            codes.push_back(codes.back() + gap);
        }
        if (codes.size() > Params::kMaxListLength) {
            throw OperationError("literal decoder: corrupt stream (clause too long)");
        }
    }

    state_.matches.insert(codes);

    std::vector<Literal> literals;
    literals.reserve(codes.size());
    for (const uint64_t code: codes) {
        literals.push_back(Literal{static_cast<int32_t>(zigzag_decode(code))});
    }
    return literals;
}
