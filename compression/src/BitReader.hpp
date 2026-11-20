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
    static constexpr unsigned kBufferBits = 64;
    static constexpr unsigned kRefillLimit = kBufferBits - 8;

    ByteReader &in_;
    uint64_t buf_ = 0;
    unsigned cnt_ = 0;

    void refill() noexcept;
};