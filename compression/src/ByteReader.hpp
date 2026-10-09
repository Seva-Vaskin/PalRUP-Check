#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

class ByteReader {
public:
    explicit ByteReader(std::FILE *file) noexcept : file_(file) {
    }

    ~ByteReader() { close(); }

    ByteReader(const ByteReader &) = delete;

    ByteReader &operator=(const ByteReader &) = delete;

    [[nodiscard]] int get() noexcept {
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

    void close() noexcept {
        if (file_ == nullptr) {
            return;
        }
        closed_ok_ = std::ferror(file_) == 0;
        closed_ok_ = std::fclose(file_) == 0 && closed_ok_;
        file_ = nullptr;
        pos_ = size_ = 0;
    }

    [[nodiscard]] bool good() const noexcept {
        return file_ != nullptr ? std::ferror(file_) == 0 : closed_ok_;
    }

private:
    static constexpr std::size_t kSize = 4096;

    std::uint8_t buf_[kSize]{};
    std::FILE *file_;
    std::size_t pos_ = 0;
    std::size_t size_ = 0;
    bool closed_ok_ = false;
};
