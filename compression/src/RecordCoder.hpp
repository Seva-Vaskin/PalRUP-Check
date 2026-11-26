#pragma once

#include "AddRecordCoder.hpp"
#include "DeleteRecordCoder.hpp"
#include "ImportRecordCoder.hpp"
#include "Params.hpp"
#include "RecencyCache.hpp"
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

void prefill_with_formula_ids(RecencyCache &cache, uint64_t clause_count);

template<typename VitterModel, typename AddCoder, typename ImportCoder, typename DeleteCoder>
struct RecordState {
    bool finished = false;

    RecencyCache clause_id_cache;
    RecencyCache literal_cache;

    VitterModel tag_model;
    AddCoder add_record;
    ImportCoder import_record;
    DeleteCoder delete_record;

    explicit RecordState(const Params &params)
        : clause_id_cache(params.cache.cache_bits),
          literal_cache(params.literals.cache_bits),
          tag_model(Params::kRecordTagCount),
          add_record(params),
          import_record(params),
          delete_record(params) {
        prefill_with_formula_ids(clause_id_cache, params.clause_count);
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

    [[nodiscard]] const Record *decode();

private:
    BitReader &in_;
    RecordState<VitterDecoder, AddRecordDecoder, ImportRecordDecoder, DeleteRecordDecoder> state_;

    Record add_slot_ = AddRecord{};
    Record import_slot_ = ImportRecord{};
    Record delete_slot_ = DeleteRecord{};
};
