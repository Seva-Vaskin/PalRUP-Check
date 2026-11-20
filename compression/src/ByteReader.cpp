#include "ByteReader.hpp"

#include "error.hpp"

ByteReader::ByteReader(std::FILE *file) noexcept : file_(file) {
}

ByteReader::~ByteReader() { close(); }

int ByteReader::get() noexcept {
    if (pos_ == size_) [[unlikely]] {
        if (file_ == nullptr) {
            return EOF;
        }
        size_ = std::fread(buf_, 1, kSize, file_);
        pos_ = 0;
        if (size_ == 0) {
            return EOF;
        }
    }
    return buf_[pos_++];
}

std::span<const uint8_t> ByteReader::buffer() const noexcept {
    return {buf_ + pos_, size_ - pos_};
}

void ByteReader::consume(const size_t count) noexcept {
    CHECK(count <= size_ - pos_);
    pos_ += count;
}

void ByteReader::close() noexcept {
    if (file_ == nullptr) {
        return;
    }
    const bool read_error = std::ferror(file_) != 0;
    const bool close_error = std::fclose(file_) != 0;
    closed_with_error_ = read_error || close_error;
    file_ = nullptr;
    pos_ = size_ = 0;
}

bool ByteReader::has_error() const noexcept {
    return file_ != nullptr ? std::ferror(file_) != 0 : closed_with_error_;
}
