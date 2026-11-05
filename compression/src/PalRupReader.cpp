#include "PalRupReader.hpp"

#include "error.hpp"
#include "utils.hpp"
#include "vbl.hpp"

bool PalRupReader::read(Record &out) const {
    switch (in_.get()) {
        case EOF:
            return false;
        case 'a': {
            AddRecord r{.id = read_id()};
            read_literals(r.literals);
            read_ids(r.hints);
            out = std::move(r);
            return true;
        }
        case 'i': {
            ImportRecord r{.id = read_id()};
            read_literals(r.literals);
            out = std::move(r);
            return true;
        }
        case 'd': {
            DeleteRecord r;
            read_ids(r.ids);
            out = std::move(r);
            return true;
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
