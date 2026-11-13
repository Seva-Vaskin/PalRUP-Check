#pragma once

#include "DeleteRecordCoder.hpp"
#include "Params.hpp"
#include "types.hpp"
#include "DistanceTable.hpp"
#include "VitterCoder.hpp"

enum class RecordTag : uint32_t {
    Add,
    Delete,
    Import,
    UpdateId,
    Eof,
};

class RecordEncoder {
public:
    RecordEncoder(const Params &params, BitWriter &out);

    void encode(const Record &record);

    void finish() noexcept;

private:
    BitWriter &out_;
    uint64_t last_id_;
    bool finished_ = false;

    VitterEncoder tag_model_;
    DeleteRecordEncoder delete_record_encoder_;

    void put_tag(RecordTag tag) noexcept;

    void encode_delete(const DeleteRecord &record) noexcept;
};

class RecordDecoder {
public:
    RecordDecoder(const Params &params, BitReader &in);

    [[nodiscard]] bool decode(Record &out);

private:
    BitReader &in_;
    uint64_t last_id_;
    bool finished_ = false;

    VitterDecoder tag_;

    [[nodiscard]] DeleteRecord decode_delete();
};
