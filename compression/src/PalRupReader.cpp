#include "PalRupReader.hpp"

#include "error.hpp"
#include "utils.hpp"
#include "vbl.hpp"

const Record *PalRupReader::read() {
    switch (in_.get()) {
        case EOF:
            return nullptr;
        case 'a': {
            auto &record = std::get<AddRecord>(add_slot_);
            record.id = read_id();
            record.literals.clear();
            record.hints.clear();
            read_literals(record.literals);
            read_ids(record.hints);
            return &add_slot_;
        }
        case 'i': {
            auto &record = std::get<ImportRecord>(import_slot_);
            record.id = read_id();
            record.literals.clear();
            read_literals(record.literals);
            return &import_slot_;
        }
        case 'd': {
            auto &record = std::get<DeleteRecord>(delete_slot_);
            record.ids.clear();
            read_ids(record.ids);
            return &delete_slot_;
        }
        default:
            throw OperationError("proof: unknown record tag");
    }
}

ClauseId PalRupReader::read_id() const {
    return ClauseId{static_cast<uint64_t>(zigzag_decode(read_vbl(in_)))};
}

void PalRupReader::read_literals(std::vector<Literal> &out) const {
    for (;;) {
        const int64_t v = zigzag_decode(read_vbl(in_));
        if (v == 0) {
            return;
        }
        if (v < INT32_MIN || v > INT32_MAX) {
            throw OperationError("proof: literal out of 32-bit range");
        }
        out.push_back(Literal{static_cast<int32_t>(v)});
    }
}

void PalRupReader::read_ids(std::vector<ClauseId> &out) const {
    for (;;) {
        const ClauseId id = read_id();
        if (id.value == 0) {
            return;
        }
        out.push_back(id);
    }
}
