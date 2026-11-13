#pragma once

#include "HintCoder.hpp"
#include "LiteralCoder.hpp"
#include "Params.hpp"
#include "types.hpp"

template<typename VitterModel, typename LiteralCoder, typename HintCoder>
struct AddState {
    uint64_t thread_count;
    uint64_t next_id = 0;

    DistanceTable jump_table;
    VitterModel jump_model;

    LiteralCoder literals;
    HintCoder hints;

    explicit AddState(const Params &params)
        : thread_count(params.thread_count),
          jump_table(params.add_record.jump_table, 0, Params::kMaxIdDelta),
          jump_model(jump_table.bucket_count()),
          literals(params.literals, params.variable_count),
          hints(params.hints) {
    }
};

class AddRecordEncoder {
public:
    explicit AddRecordEncoder(const Params &params);

    [[nodiscard]] bool needs_update(ClauseId id) const noexcept;

    void encode_update(ClauseId id, BitWriter &out);

    void encode(const AddRecord &record, BitWriter &out);

private:
    AddState<VitterEncoder, LiteralEncoder, HintEncoder> state_;
};

class AddRecordDecoder {
public:
    explicit AddRecordDecoder(const Params &params);

    void decode_update(BitReader &in);

    [[nodiscard]] AddRecord decode(BitReader &in);

private:
    AddState<VitterDecoder, LiteralDecoder, HintDecoder> state_;
};
