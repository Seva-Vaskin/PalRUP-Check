#pragma once
#include "ByteReader.hpp"
#include "types.hpp"

class PalRupReader {
public:
    explicit PalRupReader(ByteReader &in) noexcept : in_(in) {
    }

    [[nodiscard]] const Record *read();

private:
    ByteReader &in_;

    Record add_slot_ = AddRecord{};
    Record import_slot_ = ImportRecord{};
    Record delete_slot_ = DeleteRecord{};

    [[nodiscard]] ClauseId read_id() const;

    void read_literals(std::vector<Literal> &out) const;

    void read_ids(std::vector<ClauseId> &out) const;
};
