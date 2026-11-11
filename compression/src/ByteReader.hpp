#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

class ByteReader {
public:
    explicit ByteReader(std::FILE *file) noexcept;

    ~ByteReader();

    ByteReader(const ByteReader &) = delete;

    ByteReader &operator=(const ByteReader &) = delete;

    [[nodiscard]] int get() noexcept;

    void close() noexcept;

    [[nodiscard]] bool good() const noexcept;

private:
    static constexpr size_t kSize = 4096;

    uint8_t buf_[kSize]{};
    std::FILE *file_;
    size_t pos_ = 0;
    size_t size_ = 0;
    bool closed_ok_ = false;
};
