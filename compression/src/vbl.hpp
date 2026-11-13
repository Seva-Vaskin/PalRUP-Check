#pragma once

#include "BitReader.hpp"
#include "BitWriter.hpp"
#include "ByteReader.hpp"
#include "ByteWriter.hpp"
#include "error.hpp"

static void write_vbl(uint64_t value, auto &&put) {
    while (value >= 0x80) {
        put(value | 0x80);
        value >>= 7;
    }
    put(value);
}

inline void write_vbl(ByteWriter &out, uint64_t value) noexcept {
    write_vbl(value, [&out](uint64_t v) {
        out.put(static_cast<uint8_t>(v));
    });
}

inline void write_vbl(BitWriter &out, uint64_t value) noexcept {
    write_vbl(value, [&out](uint64_t v) {
        out.put_bits(v, 8);
    });
}

static uint64_t read_vbl(auto &&get) {
    uint64_t value = 0;
    for (unsigned shift = 0;; shift += 7) {
        const int c = get();
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

[[nodiscard]] inline uint64_t read_vbl(BitReader &in) {
    return read_vbl([&in] {
        return in.get_bits(8);
    });
}

[[nodiscard]] inline uint64_t read_vbl(ByteReader &in) {
    return read_vbl([&in] {
        return in.get();
    });
}
