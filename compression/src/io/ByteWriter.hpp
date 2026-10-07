#pragma once

#include <cstddef>
#include <cstdint>

class ByteWriter {
public:
    explicit ByteWriter(std::FILE *file) noexcept : file_(file) {
    }

    ~ByteWriter() { close(); }

    ByteWriter(const ByteWriter &) = delete;

    ByteWriter &operator=(const ByteWriter &) = delete;

    void put(std::uint8_t byte) noexcept {
        if (pos_ == kSize) [[unlikely]] {
            flush();
        }
        buf_[pos_++] = byte;
    }

    void flush() noexcept {
        if (pos_ != 0 && file_ != nullptr) {
            std::fwrite(buf_, 1, pos_, file_);
        }
        pos_ = 0;
    }

    void close() noexcept {
        if (file_ == nullptr) {
            return;
        }
        flush();
        closed_ok_ = std::ferror(file_) == 0;
        closed_ok_ = std::fclose(file_) == 0 && closed_ok_;
        file_ = nullptr;
    }

    [[nodiscard]] bool good() const noexcept {
        return file_ != nullptr ? std::ferror(file_) == 0 : closed_ok_;
    }

private:
    static constexpr std::size_t kSize = 4096;

    std::uint8_t buf_[kSize]{};
    std::FILE *file_;
    std::size_t pos_ = 0;
    bool closed_ok_ = false;
};
