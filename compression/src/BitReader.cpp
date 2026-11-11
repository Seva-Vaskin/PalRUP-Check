#include "BitReader.hpp"

BitReader::BitReader(ByteReader &in) noexcept: in_(in) {
}

uint32_t BitReader::get_bit() noexcept {
    if (cnt_ == 0) [[unlikely]] {
        const int c = in_.get();
        byte_ = c == EOF ? 0u : static_cast<uint32_t>(c);
        cnt_ = 8;
    }
    --cnt_;
    return (byte_ >> cnt_) & 1u;
}

uint64_t BitReader::get_bits(unsigned count) noexcept {
    CHECK(count <= 64);
    uint64_t value = 0;
    while (count-- > 0) {
        value = (value << 1) | get_bit();
    }
    return value;
}
