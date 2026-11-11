#include "ByteReader.hpp"

ByteReader::ByteReader(std::FILE *file) noexcept: file_(file) {
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

void ByteReader::close() noexcept {
    if (file_ == nullptr) {
        return;
    }
    closed_ok_ = std::ferror(file_) == 0;
    closed_ok_ = std::fclose(file_) == 0 && closed_ok_;
    file_ = nullptr;
    pos_ = size_ = 0;
}

bool ByteReader::good() const noexcept {
    return file_ != nullptr ? std::ferror(file_) == 0 : closed_ok_;
}
