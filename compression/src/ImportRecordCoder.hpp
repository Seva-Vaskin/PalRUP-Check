#pragma once

#include "LiteralCoder.hpp"
#include "Params.hpp"
#include "RecencyCache.hpp"
#include "types.hpp"

template<typename VitterModel, typename LiteralCoder>
struct ImportState {
    uint64_t prev_id = 0;

    DistanceTable id_table;
    VitterModel id_model;

    LiteralCoder literals;

    explicit ImportState(const ImportRecordParams &params, const LiteralParams &literal_params, uint32_t variable_count)
        : id_table(params.id_table, 0, Params::kMaxIdDelta),
          id_model(id_table.bucket_count()),
          literals(literal_params, variable_count) {
    }
};

class ImportRecordEncoder {
public:
    explicit ImportRecordEncoder(const Params& params);;

    void encode(const ImportRecord &record, RecencyCache &cache, BitWriter &out);

private:
    ImportState<VitterEncoder, LiteralEncoder> state_;
};

class ImportRecordDecoder {
public:
    explicit ImportRecordDecoder(const Params& params);

    void decode(BitReader &in, RecencyCache &cache, ImportRecord &out);

private:
    ImportState<VitterDecoder, LiteralDecoder> state_;
};
