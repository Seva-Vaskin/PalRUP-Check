#pragma once

#include "ByteReader.hpp"
#include "ByteWriter.hpp"
#include "error.hpp"


inline void write_vbl(ByteWriter &out, uint64_t value) noexcept {
    while (value >= 0x80) {
        out.put(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    out.put(static_cast<uint8_t>(value));
}

[[nodiscard]] inline uint64_t read_vbl(ByteReader &in) {
    uint64_t value = 0;
    for (unsigned shift = 0;; shift += 7) {
        const int c = in.get();
        if (c == EOF) {
            throw OperationError("vbl: unexpected end of stream");
        }
        const uint64_t payload = static_cast<uint64_t>(c) & 0x7F;
        if (shift == 63 && payload > 1) {
            throw OperationError("vbl: overflows 64 bits");
        }
        value |= payload << shift;
        if ((c & 0x80) == 0) {
            return value;
        }
        if (shift == 63) {
            throw OperationError("vbl: longer than 10 bytes");
        }
    }
}