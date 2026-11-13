#include "RecordCoder.hpp"

#include "utils.hpp"

RecordEncoder::RecordEncoder(const Params &params, BitWriter &out)
    : out_(out),
      last_id_(params.clause_count),
      tag_model_(Params::kRecordTagCount),
      delete_record_encoder_(params) {
}

void RecordEncoder::encode(const Record &record) {
    CHECK(!finished_);
    if (const auto *del = std::get_if<DeleteRecord>(&record)) {
        tag_model_.encode(static_cast<std::uint32_t>(RecordTag::Delete), out_);
        delete_record_encoder_.encode(*del, out_);
        return;
    }
    // TODO(step 4b): Import. TODO(step 4c): Add.
    throw OperationError("record encoder: Add/Import not implemented yet");
}

void RecordEncoder::put_tag(RecordTag tag) noexcept {
    tag_model_.encode(static_cast<uint32_t>(tag), out_);
}


void RecordEncoder::finish() noexcept {
    CHECK(!finished_);
    put_tag(RecordTag::Eof);
    finished_ = true;
}

RecordDecoder::RecordDecoder(const Params &params, BitReader &in)
    : in_(in),
      last_id_(params.clause_count),
      tag_(kRecordTagCount),
      delete_count_(delete_count_config(params)),
      delete_delta_(delete_delta_config(params)) {
}

bool RecordDecoder::decode(Record &out) {
    if (finished_) {
        return false;
    }
    switch (static_cast<RecordTag>(tag_.decode(in_))) {
        case RecordTag::Delete:
            out = decode_delete();
            return true;
        case RecordTag::Eof:
            finished_ = true;
            return false;
        case RecordTag::Add:
        case RecordTag::Import:
        case RecordTag::UpdateId:
            // TODO(step 4b): Import. TODO(step 4c): Add, UpdateId.
            throw OperationError("record decoder: Add/Import not implemented yet");
    }
    throw OperationError("record decoder: corrupt stream (unknown tag)");
}

// uint64_t RecordDecoder::get_length(DistanceDecoder &model) const {
//     const uint64_t n = model.decode(in_);
//     if (n > kMaxListLength) {
//         throw OperationError("record decoder: corrupt stream (list too long)");
//     }
//     return n;
// }

DeleteRecord RecordDecoder::decode_delete() {
    DeleteRecord record;
    const uint64_t n = get_length(delete_count_);
    uint64_t base = last_id_;
    for (uint64_t i = 0; i < n; ++i) {
        const uint64_t code = delete_delta_.decode(in_);
        base = base - zigzag_decode(code);
        record.ids.push_back(ClauseId{base});
    }
    return record;
}
