#pragma once

#include "AddRecordCoder.hpp"
#include "DeleteRecordCoder.hpp"
#include "ImportRecordCoder.hpp"
#include "Params.hpp"
#include "types.hpp"
#include "VitterCoder.hpp"

enum class RecordTag : uint32_t {
    Add,
    Delete,
    Import,
    UpdateId,
    Eof,
};

static_assert(static_cast<uint32_t>(RecordTag::Eof) + 1 == Params::kRecordTagCount);

template<typename VitterModel, typename AddCoder, typename ImportCoder, typename DeleteCoder>
struct RecordState {
    bool finished = false;

    VitterModel tag_model;
    AddCoder add_record;
    ImportCoder import_record;
    DeleteCoder delete_record;

    explicit RecordState(const Params &params)
        : tag_model(Params::kRecordTagCount),
          add_record(params),
          import_record(params),
          delete_record(params) {
    }
};

class RecordEncoder {
public:
    RecordEncoder(const Params &params, BitWriter &out);

    void encode(const Record &record);

    void finish() noexcept;

private:
    BitWriter &out_;
    RecordState<VitterEncoder, AddRecordEncoder, ImportRecordEncoder, DeleteRecordEncoder> state_;

    void put_tag(RecordTag tag) noexcept;
};

class RecordDecoder {
public:
    RecordDecoder(const Params &params, BitReader &in);

    [[nodiscard]] bool decode(Record &out);

private:
    BitReader &in_;
    RecordState<VitterDecoder, AddRecordDecoder, ImportRecordDecoder, DeleteRecordDecoder> state_;
};
