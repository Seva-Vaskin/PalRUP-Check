#include "AddRecordCoder.hpp"

#include "utils.hpp"

AddRecordEncoder::AddRecordEncoder(const Params &params) : state_(params) {
}

bool AddRecordEncoder::needs_update(const ClauseId id) const noexcept {
    return id.value != state_.next_id;
}

void AddRecordEncoder::encode_update(const ClauseId id, BitWriter &out) {
    const uint64_t jump = zigzag_encode(static_cast<int64_t>(id.value - state_.next_id));
    state_.jump_table.encode(jump, state_.jump_model, out);
    state_.next_id = id.value;
}

void AddRecordEncoder::encode(const AddRecord &record, BitWriter &out) {
    CHECK(!needs_update(record.id));
    state_.literals.encode(record.literals, out);
    state_.hints.encode(record.hints, record.id.value, out);
    state_.next_id = record.id.value + state_.thread_count;
}

AddRecordDecoder::AddRecordDecoder(const Params &params) : state_(params) {
}

void AddRecordDecoder::decode_update(BitReader &in) {
    const uint64_t jump = state_.jump_table.decode(state_.jump_model, in);
    state_.next_id += static_cast<uint64_t>(zigzag_decode(jump));
}

AddRecord AddRecordDecoder::decode(BitReader &in) {
    AddRecord record;
    record.id = ClauseId{state_.next_id};
    record.literals = state_.literals.decode(in);
    record.hints = state_.hints.decode(record.id.value, in);
    state_.next_id = record.id.value + state_.thread_count;
    return record;
}
