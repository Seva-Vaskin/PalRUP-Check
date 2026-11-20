#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

class ByteReader {
public:
    explicit ByteReader(std::FILE *file) noexcept;

    ~ByteReader();

    ByteReader(const ByteReader &) = delete;

    ByteReader &operator=(const ByteReader &) = delete;

    [[nodiscard]] int get() noexcept;

    [[nodiscard]] std::span<const uint8_t> buffer() const noexcept;

    void consume(size_t count) noexcept;

    void close() noexcept;

    [[nodiscard]] bool has_error() const noexcept;

private:
    static constexpr size_t kSize = 4096;

    uint8_t buf_[kSize]{};
    std::FILE *file_;
    size_t pos_ = 0;
    size_t size_ = 0;
    bool closed_with_error_ = false;
};
