#pragma once
#include "ByteReader.hpp"
#include "types.hpp"

class PalRupReader {
public:
    explicit PalRupReader(ByteReader &in) noexcept : in_(in) {
    }

    bool read(Record &out) const;

private:
    ByteReader &in_;

    [[nodiscard]] ClauseId read_id() const;

    void read_literals(std::vector<Literal> &out) const;

    void read_ids(std::vector<ClauseId> &out) const;
};
