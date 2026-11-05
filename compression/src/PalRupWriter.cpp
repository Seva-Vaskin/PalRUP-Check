#include "PalRupWriter.hpp"

#include "utils.hpp"
#include "vbl.hpp"


void PalRupWriter::write(const Record &record) const noexcept {
    if (const auto *add = std::get_if<AddRecord>(&record)) {
        out_.put('a');
        write_id(add->id);
        write_literals(add->literals);
        write_ids(add->hints);
    } else if (const auto *imp = std::get_if<ImportRecord>(&record)) {
        out_.put('i');
        write_id(imp->id);
        write_literals(imp->literals);
    } else {
        const auto &del = std::get<DeleteRecord>(record);
        out_.put('d');
        write_ids(del.ids);
    }
}

void PalRupWriter::write_id(const ClauseId id) const noexcept {
    write_vbl(out_, zigzag_encode(static_cast<int64_t>(id.value)));
}

void PalRupWriter::write_literals(const std::vector<Literal> &literals) const noexcept {
    for (const Literal lit: literals) {
        write_vbl(out_, zigzag_encode(lit.value));
    }
    write_vbl(out_, 0);
}

void PalRupWriter::write_ids(const std::vector<ClauseId> &ids) const noexcept {
    for (const ClauseId id: ids) {
        write_id(id);
    }
    write_vbl(out_, 0);
}

