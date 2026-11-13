#pragma once


#include "ByteReader.hpp"

#include <cstdint>

struct CnfHeader {
    uint32_t variable_count;
    uint64_t clause_count;

    [[nodiscard]] static CnfHeader read(ByteReader &in);
};


