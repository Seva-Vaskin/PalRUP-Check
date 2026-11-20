#include "ByteWriter.hpp"

ByteWriter::ByteWriter(std::FILE *file) noexcept : file_(file) {
}

ByteWriter::~ByteWriter() { close(); }

void ByteWriter::put(const uint8_t byte) noexcept {
    if (pos_ == kSize) [[unlikely]] {
        flush();
    }
    buf_[pos_++] = byte;
}

void ByteWriter::flush() noexcept {
    if (pos_ == 0) return;
    CHECK(file_ != nullptr);
    std::fwrite(buf_, 1, pos_, file_);
    pos_ = 0;
}

void ByteWriter::close() noexcept {
    if (file_ == nullptr) {
        return;
    }
    flush();
    const bool write_error = std::ferror(file_) != 0;
    const bool close_error = std::fclose(file_) != 0;
    closed_with_error_ = write_error || close_error;
    file_ = nullptr;
}

bool ByteWriter::has_error() const noexcept {
    return file_ != nullptr ? std::ferror(file_) != 0 : closed_with_error_;
}
