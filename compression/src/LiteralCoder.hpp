#pragma once

#include "Params.hpp"
#include "types.hpp"

template<typename VitterModel>
struct LiteralState {
    uint64_t max_code;

    DistanceTable first_table;
    uint32_t first_end;
    VitterModel first_model;

    DistanceTable gap_table;
    uint32_t gap_end;
    VitterModel gap_model;

    LiteralState(const LiteralParams &params, const uint32_t variable_count)
        : max_code(2 * uint64_t{variable_count}),

          first_table(params.first_table, 1, max_code),
          first_end(first_table.bucket_count()),
          first_model(first_end + 1),

          gap_table(params.gap_table, 1, max_code - 1),
          gap_end(gap_table.bucket_count()),
          gap_model(gap_end + 1) {
    }
};

class LiteralEncoder {
public:
    explicit LiteralEncoder(const LiteralParams &params, uint32_t variable_count);

    void encode(std::span<const Literal> literals, BitWriter &out);

private:
    LiteralState<VitterEncoder> state_;
};

class LiteralDecoder {
public:
    explicit LiteralDecoder(const LiteralParams &params, uint32_t variable_count);

    [[nodiscard]] std::vector<Literal> decode(BitReader &in);

private:
    LiteralState<VitterDecoder> state_;
};
