#include "ImportRecordCoder.hpp"

#include "utils.hpp"

ImportRecordEncoder::ImportRecordEncoder(const Params &params)
    : state_(params.import_record, params.literals, params.variable_count) {
}

void ImportRecordEncoder::encode(const ImportRecord &record, RecencyCache &cache, BitWriter &out) {
    const uint64_t delta = zigzag_encode(static_cast<int64_t>(record.id.value - state_.prev_id));
    state_.id_table.encode(delta, state_.id_model, out);
    state_.prev_id = record.id.value;
    state_.literals.encode(record.literals, out);
    cache.insert(record.id.value);
}

ImportRecordDecoder::ImportRecordDecoder(const Params &params)
    : state_(params.import_record, params.literals, params.variable_count) {
}

void ImportRecordDecoder::decode(BitReader &in, RecencyCache &cache, ImportRecord &out) {
    const uint64_t delta = state_.id_table.decode(state_.id_model, in);
    out.id = ClauseId{state_.prev_id + static_cast<uint64_t>(zigzag_decode(delta))};
    state_.prev_id = out.id.value;
    state_.literals.decode(in, out.literals);
    cache.insert(out.id.value);
}
