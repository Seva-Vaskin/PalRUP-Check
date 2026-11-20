#include "BitReader.hpp"

#include <bit>
#include <cstring>

namespace {
    [[noreturn]] void throw_end_of_stream() {
        throw OperationError("bit stream: unexpected end of stream");
    }
}

BitReader::BitReader(ByteReader &in) noexcept: in_(in) {
}

uint32_t BitReader::get_bit() {
    if (cnt_ == 0) [[unlikely]] {
        refill();
        if (cnt_ == 0) [[unlikely]] {
            throw_end_of_stream();
        }
    }
    const auto bit = static_cast<uint32_t>(buf_ >> (kBufferBits - 1));
    buf_ <<= 1;
    --cnt_;
    return bit;
}

uint64_t BitReader::get_bits(const unsigned count) {
    CHECK(count <= kBufferBits);
    if (count > kRefillLimit) [[unlikely]] {
        const uint64_t high = get_bits(count - 32);
        return (high << 32) | get_bits(32);
    }
    if (count == 0) [[unlikely]] {
        return 0;
    }
    if (cnt_ < count) [[unlikely]] {
        refill();
        if (cnt_ < count) [[unlikely]] {
            throw_end_of_stream();
        }
    }
    const uint64_t value = buf_ >> (kBufferBits - count);
    buf_ <<= count;
    cnt_ -= count;
    return value;
}

void BitReader::refill() noexcept {
    CHECK(cnt_ <= kRefillLimit);
    const std::span<const uint8_t> bytes = in_.buffer();
    if (bytes.size() >= 8) [[likely]] {
        uint64_t word;
        std::memcpy(&word, bytes.data(), 8);
        if constexpr (std::endian::native == std::endian::little) {
            word = std::byteswap(word);
        }
        const unsigned free_bits = kBufferBits - cnt_;
        const unsigned taken = free_bits / 8 * 8;
        buf_ |= (word >> (kBufferBits - taken)) << (free_bits - taken);
        cnt_ += taken;
        in_.consume(taken / 8);
        return;
    }
    while (cnt_ <= kRefillLimit) {
        const int c = in_.get();
        if (c == EOF) {
            return;
        }
        buf_ |= static_cast<uint64_t>(c) << (kRefillLimit - cnt_);
        cnt_ += 8;
    }
}
