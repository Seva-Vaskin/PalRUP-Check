#pragma once

#include <optional>
#include <span>
#include <vector>

#include "DistanceTable.hpp"
#include "LZ.hpp"
#include "Params.hpp"

template<typename VitterModel>
struct MatchState {
    DistanceTable length_table;
    DistanceTable distance_table;
    VitterModel distance_model;

    explicit MatchState(const LZParams &params)
        : length_table(params.length_table, LZWindow::MIN_MATCH, params.window.max_match),
          distance_table(params.distance_table, 1, uint64_t{1} << params.window.window_log),
          distance_model(distance_table.bucket_count()) {
    }

    [[nodiscard]] uint32_t zone_size() const noexcept { return length_table.bucket_count(); }
};

class MatchEncoder {
public:
    explicit MatchEncoder(const LZParams &params);

    [[nodiscard]] uint32_t zone_size() const noexcept { return state_.zone_size(); }

    [[nodiscard]] std::optional<LZWindow::Match> best(std::span<const uint64_t> tokens,
                                                      std::span<const uint64_t> single_prefix,
                                                      const VitterEncoder &model) const;

    void encode(LZWindow::Match match, VitterEncoder &model, BitWriter &out);

    void insert(std::span<const uint64_t> line);

private:
    MatchState<VitterEncoder> state_;
    LZWindow window_;
    uint64_t max_search_depth_;
};

class MatchDecoder {
public:
    explicit MatchDecoder(const LZParams &params);

    [[nodiscard]] uint32_t zone_size() const noexcept { return state_.zone_size(); }

    void decode(uint32_t symbol, BitReader &in, std::vector<uint64_t> &out);

    void insert(std::span<const uint64_t> line);

private:
    MatchState<VitterDecoder> state_;
    LZHistory history_;
};