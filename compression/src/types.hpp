#pragma once

#include <variant>
#include <vector>

struct ClauseId {
    uint64_t value;

    bool operator==(const ClauseId &) const = default;
};

struct Literal {
    int32_t value;

    bool operator==(const Literal &) const = default;
};

struct AddRecord {
    ClauseId id;
    std::vector<Literal> literals;
    std::vector<ClauseId> hints;

    bool operator==(const AddRecord &) const = default;
};

struct ImportRecord {
    ClauseId id;
    std::vector<Literal> literals;

    bool operator==(const ImportRecord &) const = default;
};

struct DeleteRecord {
    std::vector<ClauseId> ids;

    bool operator==(const DeleteRecord &) const = default;
};

using Record = std::variant<AddRecord, ImportRecord, DeleteRecord>;
