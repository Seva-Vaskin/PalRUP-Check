#pragma once

#include <cstdint>

#include "ByteWriter.hpp"

class BitWriter {
public:
    explicit BitWriter(ByteWriter &out) noexcept : out_(out) {
    }

    ~BitWriter() { flush(); }

    BitWriter(const BitWriter &) = delete;

    BitWriter &operator=(const BitWriter &) = delete;

    void put_bit(std::uint32_t bit) noexcept { put_bits(bit, 1); }

    void put_bits(std::uint32_t value, uint32_t count) noexcept {
        acc_ = (acc_ << count) | value;
        cnt_ += count;
        while (cnt_ >= 8) {
            cnt_ -= 8;
            out_.put(static_cast<std::uint8_t>(acc_ >> cnt_));
        }
    }

    void flush() noexcept {
        if (cnt_ != 0) {
            out_.put(static_cast<std::uint8_t>(acc_ << (8 - cnt_)));
            cnt_ = 0;
        }
        out_.flush();
    }

private:
    ByteWriter &out_;
    std::uint64_t acc_ = 0;
    unsigned cnt_ = 0;
};
