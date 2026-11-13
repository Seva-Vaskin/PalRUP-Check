#include "RecordCoder.hpp"

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
        state_.add_record.encode(*add, out_);
        return;
    }
    if (const auto *imp = std::get_if<ImportRecord>(&record)) {
        put_tag(RecordTag::Import);
        state_.import_record.encode(*imp, out_);
        return;
    }
    put_tag(RecordTag::Delete);
    state_.delete_record.encode(std::get<DeleteRecord>(record), out_);
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

bool RecordDecoder::decode(Record &out) {
    while (!state_.finished) {
        switch (static_cast<RecordTag>(state_.tag_model.decode(in_))) {
            case RecordTag::Add:
                out = state_.add_record.decode(in_);
                return true;
            case RecordTag::Import:
                out = state_.import_record.decode(in_);
                return true;
            case RecordTag::Delete:
                out = state_.delete_record.decode(in_);
                return true;
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
    return false;
}
