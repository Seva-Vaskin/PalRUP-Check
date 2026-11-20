#include "PalRupCompression.hpp"

#include <cerrno>
#include <string>

#include "CnfHeader.hpp"
#include "PalRupReader.hpp"
#include "PalRupWriter.hpp"

namespace {
    [[nodiscard]] std::FILE *open_file(const char *path, const char *mode) {
        std::FILE *file = std::fopen(path, mode);
        if (file == nullptr) {
            throw OperationError(std::string("cannot open ") + path + ": " + std::strerror(errno));
        }
        return file;
    }

    [[nodiscard]] Params make_params(const PalRupCompressor::CompressOptions &options) {
        Params params;
        params.variable_count = options.variable_count;
        params.clause_count = options.clause_count;
        params.thread_count = options.thread_count;
        params.validate();
        return params;
    }
}

PalRupCompressor::PalRupCompressor(const char *path, const CompressOptions &options)
    : PalRupCompressor(open_file(path, "wb"), options) {
}

PalRupCompressor::PalRupCompressor(std::FILE *file, const CompressOptions &options)
    : bytes_(file),
      params_(make_params(options)),
      bits_(bytes_),
      encoder_(params_, bits_) {
    params_.write(bytes_);
}

void PalRupCompressor::append_add(const uint64_t id, const std::span<const int32_t> literals,
                                  const std::span<const uint64_t> hints) {
    AddRecord record{.id = ClauseId{id}};
    for (const int32_t lit: literals) {
        record.literals.push_back(Literal{lit});
    }
    for (const uint64_t hint: hints) {
        record.hints.push_back(ClauseId{hint});
    }
    encoder_.encode(record);
}

void PalRupCompressor::append_import(const uint64_t id, const std::span<const int32_t> literals) {
    ImportRecord record{.id = ClauseId{id}};
    for (const int32_t lit: literals) {
        record.literals.push_back(Literal{lit});
    }
    encoder_.encode(record);
}

void PalRupCompressor::append_delete(const std::span<const uint64_t> ids) {
    DeleteRecord record;
    for (const uint64_t id: ids) {
        record.ids.push_back(ClauseId{id});
    }
    encoder_.encode(record);
}

void PalRupCompressor::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    encoder_.finish();
    bits_.close();
    if (bytes_.has_error()) {
        throw OperationError("compressor: write error");
    }
}

void PalRupCompressor::compress_file(const char *proof_path, const char *cnf_path, const char *output_path,
                                     const uint32_t thread_count) {
    CompressOptions options{.thread_count = thread_count};
    {
        ByteReader cnf(open_file(cnf_path, "rb"));
        const CnfHeader header = CnfHeader::read(cnf);
        options.variable_count = header.variable_count;
        options.clause_count = header.clause_count;
    }

    ByteReader proof(open_file(proof_path, "rb"));
    PalRupCompressor compressor(output_path, options);
    const PalRupReader reader(proof);
    Record record;
    while (reader.read(record)) {
        compressor.encoder_.encode(record);
    }
    if (proof.has_error()) {
        throw OperationError(std::string("read error: ") + proof_path);
    }
    compressor.close();
}

PalRupDecompressor::PalRupDecompressor(const char *path) : PalRupDecompressor(open_file(path, "rb")) {
}

PalRupDecompressor::PalRupDecompressor(std::FILE *file)
    : bytes_(file),
      bits_(bytes_),
      decoder_(Params::read(bytes_), bits_) {
}

bool PalRupDecompressor::read(Record &out) {
    return decoder_.decode(out);
}

void PalRupDecompressor::close() {
    bytes_.close();
}

void PalRupDecompressor::decompress_file(const char *input_path, const char *output_path) {
    PalRupDecompressor decompressor(input_path);
    ByteWriter bytes(open_file(output_path, "wb"));
    const PalRupWriter writer(bytes);
    Record record;
    while (decompressor.read(record)) {
        writer.write(record);
    }
    bytes.close();
    if (bytes.has_error()) {
        throw OperationError(std::string("write error: ") + output_path);
    }
}
