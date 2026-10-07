#pragma once

#include "core/types.hpp"

#include <vector>

struct AddRecord {
    ClauseId id;
    std::vector<Literal> literals;
    std::vector<ClauseId> hints;
};

struct ImportRecord {
    ClauseId id;
    std::vector<Literal> literals;
};

struct DeleteRecord {
    std::vector<ClauseId> ids;
};

using Record = std::variant<AddRecord, ImportRecord, DeleteRecord>;
