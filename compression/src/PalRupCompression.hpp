#pragma once

#include <cstdint>
#include <cstdio>
#include <span>

#include "BitReader.hpp"
#include "BitWriter.hpp"
#include "ByteReader.hpp"
#include "ByteWriter.hpp"
#include "Params.hpp"
#include "RecordCoder.hpp"
#include "types.hpp"

class PalRupCompressor {
public:
    struct CompressOptions {
        uint32_t variable_count;
        uint64_t clause_count;
        uint32_t thread_count;
    };

    PalRupCompressor(const char *path, const CompressOptions &options);

    PalRupCompressor(std::FILE *file, const CompressOptions &options);

    ~PalRupCompressor();

    PalRupCompressor(const PalRupCompressor &) = delete;

    PalRupCompressor &operator=(const PalRupCompressor &) = delete;

    void append_add(uint64_t id, std::span<const int32_t> literals, std::span<const uint64_t> hints);

    void append_import(uint64_t id, std::span<const int32_t> literals);

    void append_delete(std::span<const uint64_t> ids);

    void close();

    static void compress_file(const char *proof_path, const char *cnf_path, const char *output_path,
                              uint32_t thread_count);

private:
    ByteWriter bytes_;
    Params params_;
    BitWriter bits_;
    RecordEncoder encoder_;
    bool closed_ = false;
};

class PalRupDecompressor {
public:
    explicit PalRupDecompressor(const char *path);

    explicit PalRupDecompressor(std::FILE *file);

    PalRupDecompressor(const PalRupDecompressor &) = delete;

    PalRupDecompressor &operator=(const PalRupDecompressor &) = delete;

    [[nodiscard]] bool read(Record &out);

    void close();

    static void decompress_file(const char *input_path, const char *output_path);

private:
    ByteReader bytes_;
    BitReader bits_;
    RecordDecoder decoder_;
};
