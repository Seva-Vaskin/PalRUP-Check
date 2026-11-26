#include "BitWriter.hpp"

BitWriter::BitWriter(ByteWriter &out) noexcept: out_(out) {
}

BitWriter::~BitWriter() { flush(); }

void BitWriter::put_bits(const uint64_t value, const uint32_t count) noexcept {
    CHECK(count <= 64);
    if (count > 32) [[unlikely]] {
        put_chunk(value >> 32, count - 32);
        put_chunk(value, 32);
        return;
    }
    put_chunk(value, count);
}

void BitWriter::put_chunk(const uint64_t value, const uint32_t count) noexcept {
    acc_ = (acc_ << count) | (value & ((uint64_t{1} << count) - 1));
    cnt_ += count;
    while (cnt_ >= 8) {
        cnt_ -= 8;
        out_.put(static_cast<uint8_t>(acc_ >> cnt_));
    }
}

void BitWriter::flush() noexcept {
    CHECK(cnt_ < 8);
    if (cnt_ != 0) {
        out_.put(static_cast<uint8_t>(acc_ << (8 - cnt_)));
        cnt_ = 0;
    }
    out_.flush();
}

void BitWriter::close() noexcept {
    flush();
    out_.close();
}
