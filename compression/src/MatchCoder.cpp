#include "MatchCoder.hpp"

#include <algorithm>

#include "error.hpp"

MatchEncoder::MatchEncoder(const LZParams &params)
    : state_(params),
      window_(params.window),
      max_search_depth_(params.window.max_search_depth) {
}

std::optional<LZWindow::Match> MatchEncoder::best(const std::span<const uint64_t> tokens,
                                                  const std::span<const uint64_t> single_prefix,
                                                  const VitterEncoder &model) const {
    CHECK(single_prefix.size() == tokens.size() + 1);
    std::optional<LZWindow::Match> best;
    int64_t best_saving = 0;
    std::vector<LZWindow::Match> candidates(max_search_depth_);
    for (const LZWindow::Match &m: window_.find_matches(tokens, candidates)) {
        const auto singles = static_cast<int64_t>(single_prefix[m.length] - single_prefix[0]);
        const int64_t match = state_.length_table.cost(m.length, model)
                              + state_.distance_table.cost(m.distance, state_.distance_model);
        if (singles - match > best_saving) {
            best_saving = singles - match;
            best = m;
        }
    }
    return best;
}

void MatchEncoder::encode(const LZWindow::Match match, VitterEncoder &model, BitWriter &out) {
    state_.length_table.encode(match.length, model, out);
    state_.distance_table.encode(match.distance, state_.distance_model, out);
}

void MatchEncoder::insert(const std::span<const uint64_t> line) {
    window_.insert(line);
}

MatchDecoder::MatchDecoder(const LZParams &params)
    : state_(params),
      history_(params.window.window_log) {
}

void MatchDecoder::decode(const uint32_t symbol, BitReader &in, std::vector<uint64_t> &out) {
    const uint64_t length = state_.length_table.decode_extra(symbol, in);
    const uint64_t distance = state_.distance_table.decode(state_.distance_model, in);

    const uint64_t reach = std::min(history_.total(), history_.capacity());
    if (length > state_.length_table.max_value() || distance > reach || length > distance) {
        throw OperationError("match decoder: corrupt stream (match out of range)");
    }

    const uint64_t start = history_.total() - distance;
    for (uint64_t k = 0; k < length; ++k) {
        out.push_back(history_.token(start + k));
    }
}

void MatchDecoder::insert(const std::span<const uint64_t> line) {
    history_.insert(line);
}