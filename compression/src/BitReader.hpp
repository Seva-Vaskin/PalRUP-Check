#pragma once

#include "ByteReader.hpp"
#include "error.hpp"

class BitReader {
public:
    explicit BitReader(ByteReader &in) noexcept;

    BitReader(const BitReader &) = delete;

    BitReader &operator=(const BitReader &) = delete;

    uint32_t get_bit();

    uint64_t get_bits(unsigned count);

private:
    ByteReader &in_;
    uint32_t byte_ = 0;
    unsigned cnt_ = 0;
};
