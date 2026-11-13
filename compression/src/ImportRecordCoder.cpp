//
// Created by Vsevolod Vaskin on 13.11.26.
//

#include "ImportRecordCoder.hpp"

#include "utils.hpp"

ImportRecordEncoder::ImportRecordEncoder(const Params &params)
    : state_(params.import_record, params.literals, params.variable_count) {
}

void ImportRecordEncoder::encode(const ImportRecord &record, BitWriter &out) {
    const uint64_t delta = zigzag_encode(static_cast<int64_t>(record.id.value - state_.prev_id));
    state_.id_table.encode(delta, state_.id_model, out);
    state_.prev_id = record.id.value;
    state_.literals.encode(record.literals, out);
}

ImportRecordDecoder::ImportRecordDecoder(const Params &params)
    : state_(params.import_record, params.literals, params.variable_count) {
}

ImportRecord ImportRecordDecoder::decode(BitReader &in) {
    ImportRecord record;
    const uint64_t delta = state_.id_table.decode(state_.id_model, in);
    record.id = ClauseId{state_.prev_id + static_cast<uint64_t>(zigzag_decode(delta))};
    state_.prev_id = record.id.value;
    record.literals = state_.literals.decode(in);
    return record;
}
