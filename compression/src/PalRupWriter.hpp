#pragma once

#include "types.hpp"
#include "ByteWriter.hpp"

class PalRupWriter {
public:
    explicit PalRupWriter(ByteWriter &out) noexcept : out_(out) {
    }

    void write(const Record &record) const noexcept;

private:
    ByteWriter &out_;

    void write_id(ClauseId id) const noexcept;
    void write_literals(const std::vector<Literal> &literals) const noexcept;
    void write_ids(const std::vector<ClauseId> &ids) const noexcept;
};

