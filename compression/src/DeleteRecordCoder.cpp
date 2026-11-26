#include "DeleteRecordCoder.hpp"

#include <algorithm>

DeleteRecordEncoder::DeleteRecordEncoder(const Params &params) : state_(params.delete_record, params.thread_count) {
}

void DeleteRecordEncoder::encode(const DeleteRecord &record, RecencyCache &cache, BitWriter &out) {
    if (record.ids.size() > Params::kMaxListLength) {
        throw OperationError("delete encoder: Delete has more than kMaxListLength ids");
    }
    scratch_entries_.clear();
    for (const ClauseId id: record.ids) {
        scratch_entries_.push_back(Entry{
            .thread = static_cast<uint32_t>(id.value % state_.thread_count),
            .local_id = id.value / state_.thread_count,
        });
    }
    std::ranges::sort(scratch_entries_, {}, [](const Entry &e) {
        return std::make_pair(e.thread, e.local_id);
    });

    scratch_groups_.clear();
    for (uint64_t i = 0; i < scratch_entries_.size(); i++) {
        const auto &entry = scratch_entries_[i];
        if (i == 0 || scratch_entries_[i - 1].thread != entry.thread) {
            scratch_groups_.push_back(Group{
                .thread = entry.thread,
                .begin = i,
                .end = i + 1,
            });
        } else if (scratch_entries_[i - 1].local_id == entry.local_id) {
            throw OperationError("delete encoder: duplicate id in Delete");
        } else {
            scratch_groups_.back().end = i + 1;
        }
    }
    std::ranges::sort(scratch_groups_, {}, [this](const Group &g) {
        return std::make_pair(scratch_entries_[g.begin].local_id, g.thread);
    });

    uint64_t prev_first = 0;
    for (const Group &g: scratch_groups_) {
        state_.gaps_and_threads_model.encode(g.thread, out);
        const auto first = scratch_entries_[g.begin].local_id;
        CHECK(first >= prev_first);
        state_.first_table.encode(first - prev_first, state_.first_model, out, 0);
        prev_first = first;
        for (auto i = g.begin + 1; i < g.end; i++) {
            const auto gap = scratch_entries_[i].local_id - scratch_entries_[i - 1].local_id - 1;
            state_.gaps_table.encode(gap, state_.gaps_and_threads_model, out, state_.thread_count);
        }
    }
    state_.gaps_and_threads_model.encode(state_.end, out);

    for (const ClauseId id: record.ids) {
        cache.remove(id.value);
    }
}

DeleteRecordDecoder::DeleteRecordDecoder(const Params &params) : state_(params.delete_record, params.thread_count) {
}

void DeleteRecordDecoder::decode(BitReader &in, RecencyCache &cache, DeleteRecord &out) {
    out.ids.clear();
    uint64_t prev_first = 0;
    uint64_t thread = 0;
    uint64_t local_id = 0;
    bool in_group = false;

    for (;;) {
        const uint32_t symbol = state_.gaps_and_threads_model.decode(in);

        if (symbol == state_.end) {
            // END
            for (const ClauseId id: out.ids) {
                cache.remove(id.value);
            }
            return;
        }
        if (symbol < state_.thread_count) {
            // START of group
            thread = symbol;
            local_id = prev_first + state_.first_table.decode(state_.first_model, in);
            prev_first = local_id;
            in_group = true;
        } else {
            // GAP: symbol in [thread_count, end)
            if (!in_group) {
                throw OperationError("delete decoder: corrupt stream (gap before any group)");
            }
            const auto bucket = symbol - state_.thread_count;
            local_id += state_.gaps_table.decode_extra(bucket, in) + 1;
        }

        if (out.ids.size() == Params::kMaxListLength) {
            throw OperationError("delete decoder: corrupt stream (list too long)");
        }
        out.ids.push_back(ClauseId{local_id * state_.thread_count + thread});
    }
}
