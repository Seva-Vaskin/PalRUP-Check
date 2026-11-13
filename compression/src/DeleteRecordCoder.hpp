#pragma once
#include "Params.hpp"
#include "types.hpp"

template<typename VitterModel>
struct DeleteState {
    uint32_t thread_count;

    DistanceTable gaps_table;
    uint32_t end;
    VitterModel gaps_and_threads_model;


    DistanceTable first_table;
    VitterModel first_model;

    explicit DeleteState(const DeleteRecordParams &params, const uint32_t thread_count)
        : thread_count(thread_count),

          gaps_table(params.gaps_table, 0, Params::kMaxIdDelta),
          end(thread_count + gaps_table.bucket_count()),
          gaps_and_threads_model(end + 1),

          first_table(params.first_table, 0, Params::kMaxIdDelta),
          first_model(first_table.bucket_count()) {
    }
};


class DeleteRecordEncoder {
public:
    explicit DeleteRecordEncoder(const Params &params);

    void encode(const DeleteRecord &record, BitWriter &out);

private:
    DeleteState<VitterEncoder> state_;
};

class DeleteRecordDecoder {
public:
    explicit DeleteRecordDecoder(const Params &params);

    [[nodiscard]] DeleteRecord decode(BitReader &in);

private:
    DeleteState<VitterDecoder> state_;
};
