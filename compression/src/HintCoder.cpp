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
    if (hints.empty()) {
        state_.first_model.encode(state_.first_end, out);  // no hints
        return;
    }
    state_.first_table.encode(delta(id, hints[0].value), state_.first_model, out);
    for (size_t i = 1; i < hints.size(); ++i) {
        state_.rest_table.encode(delta(hints[i - 1].value, hints[i].value), state_.rest_model, out);
    }
    state_.rest_model.encode(state_.rest_end, out);
}

HintDecoder::HintDecoder(const HintParams &params) : state_(params) {
}

std::vector<ClauseId> HintDecoder::decode(const uint64_t id, BitReader &in) {
    std::vector<ClauseId> hints;

    const uint32_t first = state_.first_model.decode(in);
    if (first == state_.first_end) {  // END: no hints
        return hints;
    }
    uint64_t hint = undelta(id, state_.first_table.decode_extra(first, in));
    hints.push_back(ClauseId{hint});

    for (;;) {
        const uint32_t symbol = state_.rest_model.decode(in);
        if (symbol == state_.rest_end) {
            return hints;
        }
        if (hints.size() == Params::kMaxListLength) {
            throw OperationError("hint decoder: corrupt stream (too many hints)");
        }
        hint = undelta(hint, state_.rest_table.decode_extra(symbol, in));
        hints.push_back(ClauseId{hint});
    }
}