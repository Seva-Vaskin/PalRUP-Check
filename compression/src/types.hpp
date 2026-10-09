#pragma once

#include <vector>

struct ClauseId {
    uint64_t value;
};

struct Literal {
    int32_t value;
};

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

