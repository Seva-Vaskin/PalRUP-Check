#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "error.hpp"

class ByteWriter {
public:
    explicit ByteWriter(std::FILE *file) noexcept;

    ~ByteWriter();

    ByteWriter(const ByteWriter &) = delete;

    ByteWriter &operator=(const ByteWriter &) = delete;

    void put(uint8_t byte) noexcept;

    void flush() noexcept;

    void close() noexcept;

    [[nodiscard]] bool has_error() const noexcept;

private:
    static constexpr size_t kSize = 4096;

    uint8_t buf_[kSize]{};
    std::FILE *file_;
    size_t pos_ = 0;
    bool closed_with_error_ = false;
};
