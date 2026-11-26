#include "RecordCoder.hpp"

#include <algorithm>

void prefill_with_formula_ids(RecencyCache &cache, const uint64_t clause_count) {
    const uint64_t count = std::min<uint64_t>(clause_count, cache.capacity());
    const uint64_t first = clause_count - count + 1;
    for (uint64_t k = 0; k < count; ++k) {
        cache.insert(first + k);
    }
}

RecordEncoder::RecordEncoder(const Params &params, BitWriter &out) : out_(out), state_(params) {
}

void RecordEncoder::encode(const Record &record) {
    CHECK(!state_.finished);
    if (const auto *add = std::get_if<AddRecord>(&record)) {
        if (state_.add_record.needs_update(add->id)) {
            put_tag(RecordTag::UpdateId);
            state_.add_record.encode_update(add->id, out_);
        }
        put_tag(RecordTag::Add);
        state_.add_record.encode(*add, state_.clause_id_cache, out_);
        return;
    }
    if (const auto *imp = std::get_if<ImportRecord>(&record)) {
        put_tag(RecordTag::Import);
        state_.import_record.encode(*imp, state_.clause_id_cache, out_);
        return;
    }
    put_tag(RecordTag::Delete);
    state_.delete_record.encode(std::get<DeleteRecord>(record), state_.clause_id_cache, out_);
}

void RecordEncoder::put_tag(const RecordTag tag) noexcept {
    state_.tag_model.encode(static_cast<uint32_t>(tag), out_);
}

void RecordEncoder::finish() noexcept {
    CHECK(!state_.finished);
    put_tag(RecordTag::Eof);
    state_.finished = true;
}

RecordDecoder::RecordDecoder(const Params &params, BitReader &in) : in_(in), state_(params) {
}

const Record *RecordDecoder::decode() {
    while (!state_.finished) {
        switch (static_cast<RecordTag>(state_.tag_model.decode(in_))) {
            case RecordTag::Add:
                state_.add_record.decode(in_, state_.clause_id_cache, std::get<AddRecord>(add_slot_));
                return &add_slot_;
            case RecordTag::Import:
                state_.import_record.decode(in_, state_.clause_id_cache, std::get<ImportRecord>(import_slot_));
                return &import_slot_;
            case RecordTag::Delete:
                state_.delete_record.decode(in_, state_.clause_id_cache, std::get<DeleteRecord>(delete_slot_));
                return &delete_slot_;
            case RecordTag::UpdateId:
                state_.add_record.decode_update(in_);
                break;
            case RecordTag::Eof:
                state_.finished = true;
                break;
            default:
                throw OperationError("record decoder: corrupt stream (unknown tag)");
        }
    }
    return nullptr;
}
