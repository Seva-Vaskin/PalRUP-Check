#pragma once

#include <cassert>
#include <cstdint>

#include "io/ByteReader.hpp"

class BitReader {
public:
    explicit BitReader(ByteReader &in) noexcept : in_(in) {
    }

    BitReader(const BitReader &) = delete;

    BitReader &operator=(const BitReader &) = delete;

    std::uint32_t get_bit() noexcept {
        if (cnt_ == 0) [[unlikely]] {
            const int c = in_.get();
            byte_ = c == EOF ? 0u : static_cast<std::uint32_t>(c);
            cnt_ = 8;
        }
        --cnt_;
        return (byte_ >> cnt_) & 1u;
    }

    std::uint32_t get_bits(unsigned count) noexcept {
        assert(count <= 32);
        std::uint32_t value = 0;
        while (count-- > 0) {
            value = (value << 1) | get_bit();
        }
        return value;
    }

private:
    ByteReader &in_;
    std::uint32_t byte_ = 0;
    unsigned cnt_ = 0;
};
