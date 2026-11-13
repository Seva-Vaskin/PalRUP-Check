#include "DeleteRecordCoder.hpp"

#include <algorithm>

#include "utils.hpp"

namespace {
    struct Entry {
        uint64_t thread;
        uint64_t local_id;

        Entry(const ClauseId id, const uint64_t thread_count) {
            thread = id.value % thread_count;
            local_id = id.value / thread_count;
        }
    };

    struct Group {
        uint64_t thread;
        uint64_t begin;
        uint64_t end;
    };

    void encode_value(VitterEncoder &model, const uint32_t offset, const DistanceTable &table,
                      const uint64_t value, BitWriter &out) noexcept {
        const uint32_t bucket = table.bucket_of(value);
        model.encode(offset + bucket, out);
        table.encode_extra(value, out);
    }

    [[nodiscard]] uint64_t decode_value(VitterDecoder &model, const DistanceTable &table, BitReader &in) {
        const uint32_t bucket = model.decode(in);
        return table.decode_extra(bucket, in);
    }
}

DeleteRecordEncoder::DeleteRecordEncoder(const Params &params) : state_(params.delete_record, params.thread_count) {
}

void DeleteRecordEncoder::encode(const DeleteRecord &record, BitWriter &out) {
    if (record.ids.size() > Params::kMaxListLength) {
        throw OperationError("delete encoder: Delete has more than kMaxListLength ids");
    }
    std::vector<Entry> entries;
    entries.reserve(record.ids.size());
    for (auto &id: record.ids) {
        entries.emplace_back(id, state_.thread_count);
    }
    std::ranges::sort(entries, {}, [](const Entry &e) {
        return std::make_pair(e.thread, e.local_id);
    });

    std::vector<Group> groups;
    for (uint64_t i = 0; i < entries.size(); i++) {
        const auto &entry = entries[i];
        if (i == 0 || entries[i - 1].thread != entry.thread) {
            groups.push_back(Group{
                .thread = entry.thread,
                .begin = i,
                .end = i + 1,
            });
        } else if (entries[i - 1].local_id == entry.local_id) {
            throw OperationError("delete encoder: duplicate id in Delete");
        } else {
            groups.back().end = i + 1;
        }
    }
    std::ranges::sort(groups, {}, [&entries](const Group &g) {
        return std::make_pair(entries[g.begin].local_id, g.thread);
    });

    uint64_t prev_first = 0;
    for (const Group &g: groups) {
        state_.gaps_and_threads_model.encode(g.thread, out);
        const auto first = entries[g.begin].local_id;
        CHECK(first >= prev_first);
        encode_value(state_.first_model, 0, state_.first_table, first - prev_first, out);
        prev_first = first;
        for (auto i = g.begin + 1; i < g.end; i++) {
            auto gap = entries[i].local_id - entries[i - 1].local_id - 1;
            encode_value(state_.gaps_and_threads_model, state_.thread_count, state_.gaps_table, gap, out);
        }
    }
    state_.gaps_and_threads_model.encode(state_.end, out);
}

DeleteRecordDecoder::DeleteRecordDecoder(const Params &params) : state_(params.delete_record, params.thread_count) {
}

DeleteRecord DeleteRecordDecoder::decode(BitReader &in) {
    DeleteRecord record;
    uint64_t prev_first = 0;
    uint64_t thread = 0;
    uint64_t local_id = 0;
    bool in_group = false;

    for (;;) {
        const uint32_t symbol = state_.gaps_and_threads_model.decode(in);

        if (symbol == state_.end) {
            // END
            return record;
        }
        if (symbol < state_.thread_count) {
            // START of group
            thread = symbol;
            local_id = prev_first + decode_value(state_.first_model, state_.first_table, in);
            prev_first = local_id;
            in_group = true;
        } else {
            // GAP: symbol in [thread_count, end)
            if (!in_group) {
                throw OperationError("delete decoder: corrupt stream (gap before any group)");
            }
            const auto bucket = static_cast<uint32_t>(symbol - state_.thread_count);
            local_id += state_.gaps_table.decode_extra(bucket, in) + 1;
        }

        if (record.ids.size() == Params::kMaxListLength) {
            throw OperationError("delete decoder: corrupt stream (list too long)");
        }
        record.ids.push_back(ClauseId{local_id * state_.thread_count + thread});
    }
}
