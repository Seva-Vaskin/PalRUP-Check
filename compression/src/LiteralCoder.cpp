#include "LiteralCoder.hpp"

#include <algorithm>

#include "utils.hpp"

LiteralEncoder::LiteralEncoder(const LiteralParams &params, uint32_t variable_count) : state_(params, variable_count) {
}

void LiteralEncoder::encode(std::span<const Literal> literals, BitWriter &out) {
    std::vector<uint64_t> codes(literals.size());
    for (size_t i = 0; i < literals.size(); i++) {
        codes[i] = zigzag_encode(literals[i].value);
    }
    std::ranges::sort(codes);

    if (codes.empty()) {
        state_.first_model.encode(state_.first_end, out); // empty clause
        return;
    }

    if (codes.front() == 0 || codes.back() > state_.max_code) {
        throw OperationError("literal encoder: literal 0 or above variable_count");
    }

    state_.first_table.encode(codes.front(), state_.first_model, out, 0);
    for (size_t i = 1; i < codes.size(); i++) {
        if (codes[i] == codes[i - 1]) {
            throw OperationError("literal encoder: duplicate literal in clause");
        }
        state_.gap_table.encode(codes[i] - codes[i - 1], state_.gap_model, out, 0);
    }
    state_.gap_model.encode(state_.gap_end, out);
}

LiteralDecoder::LiteralDecoder(const LiteralParams &params, uint32_t variable_count) : state_(params, variable_count) {
}

std::vector<Literal> LiteralDecoder::decode(BitReader &in) {
    std::vector<Literal> literals;

    const uint32_t first = state_.first_model.decode(in);
    if (first == state_.first_end) {
        // empty clause
        return literals;
    }
    uint64_t code = state_.first_table.decode_extra(first, in);
    if (code > state_.max_code) {
        throw OperationError("literal decoder: corrupt stream (literal out of range)");
    }
    literals.push_back(Literal{static_cast<int32_t>(zigzag_decode(code))});

    for (;;) {
        const uint32_t symbol = state_.gap_model.decode(in);
        if (symbol == state_.gap_end) {
            return literals;
        }
        const uint64_t gap = state_.gap_table.decode_extra(symbol, in);
        if (gap > state_.max_code - code) {
            throw OperationError("literal decoder: corrupt stream (literal out of range)");
        }
        code += gap;
        literals.push_back(Literal{static_cast<int32_t>(zigzag_decode(code))});
    }
}
