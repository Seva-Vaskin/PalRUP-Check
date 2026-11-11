#pragma once

#include "ByteWriter.hpp"

class BitWriter {
public:
    explicit BitWriter(ByteWriter &out) noexcept;

    ~BitWriter();

    BitWriter(const BitWriter &) = delete;

    BitWriter &operator=(const BitWriter &) = delete;

    void put_bit(uint32_t bit) noexcept;

    void put_bits(uint64_t value, uint32_t count) noexcept;

    void flush() noexcept;

    void close() noexcept;

private:
    ByteWriter &out_;
    uint64_t acc_ = 0;
    unsigned cnt_ = 0;

    void put_chunk(uint64_t value, uint32_t count) noexcept;
};
