#include "BitReader.hpp"

BitReader::BitReader(ByteReader &in) noexcept: in_(in) {
}

uint32_t BitReader::get_bit() {
    if (cnt_ == 0) [[unlikely]] {
        const int c = in_.get();
        if (c == EOF) [[unlikely]] {
            throw OperationError("bit stream: unexpected end of stream (truncated file?)");
        }
        byte_ = static_cast<uint32_t>(c);
        cnt_ = 8;
    }
    --cnt_;
    return (byte_ >> cnt_) & 1u;
}

uint64_t BitReader::get_bits(unsigned count) {
    CHECK(count <= 64);
    uint64_t value = 0;
    while (count-- > 0) {
        value = (value << 1) | get_bit();
    }
    return value;
}
