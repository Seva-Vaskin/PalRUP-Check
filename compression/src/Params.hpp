#pragma once

#include "LZWindow.hpp"
#include "DistanceTable.hpp"

struct LZParams {
    LZWindow::Config window;
    DistanceTable::Config length;
    DistanceTable::Config distance;

    [[nodiscard]] uint64_t window_size() const noexcept;

    void fields(this auto &self, auto &&f) {
        f(self.window);
        f(self.length);
        f(self.distance);
    }
};

struct DeleteRecordParams {
    DistanceTable::Config gaps_table = {.exact_until = 256, .groups_per_level = 32};
    DistanceTable::Config first_table = {.exact_until = 256, .groups_per_level = 32};

    void fields(this auto &self, auto &&f) {
        f(self.gaps_table);
        f(self.first_table);
    }
};

struct Params {
    static constexpr std::array<uint8_t, 4> kMagic = {'P', 'R', 'P', 'H'};
    static constexpr uint8_t kVersion = 1;

    static constexpr uint64_t kMaxIdDelta = uint64_t{1} << 62;
    static constexpr uint64_t kMaxListLength = UINT32_MAX;
    static constexpr uint64_t kMaxThreadCount = uint64_t{1} << 16;
    static constexpr uint32_t kRecordTagCount = 5;

    uint32_t variable_count;
    uint64_t clause_count;
    uint32_t thread_count;

    DeleteRecordParams delete_record;

    // TODO
};

//
//
// // Remark:
// // Params.hpp, Params.cpp are LLM-generated.
//
// #include "ByteReader.hpp"
// #include "ByteWriter.hpp"
// #include "DistanceTable.hpp"
// #include "LZWindow.hpp"
//
// #include <array>
// #include <cstdint>
//
// // Two kinds of parameters, split by one question: does the decoder need it?
// //
// //   StreamParams  written at the start of the stream; encoder and decoder must
// //                 agree on every field, so the decoder reads them from the file.
// //   SearchParams  how hard the encoder searches; invisible in the format, any
// //                 value produces a stream that decodes the same way.
//
// // Tunable shape of one DistanceTable. min_value / max_value are not part of it:
// // they follow from what the table codes (gap >= 1, |lit| <= variable_count, ...)
// // and live in the codec, so a file can never carry an inconsistent pair.
// struct TableShape {
//     uint64_t exact_until;
//     uint32_t groups_per_level;
//
//     // Bounds for untrusted input. exact_until caps the alphabet (and memory) of
//     // the Vitter model; with these limits a table has at most ~2.4M symbols.
//     static constexpr uint64_t kMaxExactUntil = uint64_t{1} << 21;
//     static constexpr uint32_t kMaxGroupsPerLevel = 4096;
//
//     // Throws OperationError naming `field` if out of range.
//     void validate(const char *field) const;
//
//     [[nodiscard]] DistanceTable::Config table(const uint64_t min_value, const uint64_t max_value) const noexcept {
//         return DistanceTable::Config{
//             .min_value = min_value,
//             .exact_until = exact_until,
//             .groups_per_level = groups_per_level,
//             .max_value = max_value,
//         };
//     }
//
//     bool operator==(const TableShape &) const = default;
// };
//
// // Format half of one LZ stream (hints or literals).
// // Table bounds follow from the other fields:
// //   length:   min LZWindow::MIN_MATCH (2), max max_match
// //   distance: min 1,                       max window_size()
// struct LzShape {
//     uint8_t window_log; // window of 2^window_log tokens; 0 = LZ off for this stream
//     uint32_t max_match; // longest copy a match may encode
//     TableShape length;
//     TableShape distance;
//
//     static constexpr uint32_t kMinMatch = 2;
//     static constexpr uint32_t kMaxMatch = 1u << 16;
//
//     [[nodiscard]] bool enabled() const noexcept { return window_log != 0; }
//     [[nodiscard]] uint64_t window_size() const noexcept { return uint64_t{1} << window_log; }
//
//     // Throws OperationError naming `field` if out of range.
//     void validate(const char *field) const;
//
//     bool operator==(const LzShape &) const = default;
// };
//
// // Everything the decoder needs, stored right after magic + version.
// // The bit stream starts after it, byte-aligned.
// //
// // Rule: changing a default here keeps old files decodable (the decoder reads
// // the values from the file); changing how the stream is written or read means
// // bumping kVersion.
// struct StreamParams {
//     static constexpr std::array<uint8_t, 4> kMagic = {'P', 'R', 'P', 'H'};
//     static constexpr uint8_t kVersion = 1;
//
//     // --- Facts about the proof: supplied by the caller. ---
//     uint32_t variable_count = 0; // |lit| <= variable_count
//     uint64_t clause_count = 0; // formula clauses are ids 1..clause_count
//     uint32_t thread_count = 1; // Add-id predictor stride
//
//     // --- Codec parameters. Defaults live here and only here. ---
//     //                         exact  groups  start_bits
//     TableShape add_id_jump{256, 32}; // zigzag(id - predicted) after UpdateId
//     TableShape import_id{256, 32}; // zigzag(id - predicted)
//     TableShape literal_count{64, 8}; // literals per clause
//     TableShape literal_first{3072, 64}; // smallest sorted zigzag code
//     TableShape literal_gap{3072, 64}; // gap between consecutive sorted codes
//     TableShape hint_count{64, 8}; // hints per Add
//     TableShape hint_delta{256, 32}; // zigzag(current_id - hint)
//     TableShape delete_count{64, 8}; // ids per Delete
//     TableShape delete_delta{256, 32}; // zigzag delta between consecutive deleted ids
//
//     // --- LZ streams (defaults from the Rust codec). ---
//     //                window_log  max_match   length          distance
//     LzShape hint_lz{15, 1u << 16, {64, 8}, {64, 16}}; // copies of hint-id runs
//     LzShape literal_lz{15, 1u << 16, {64, 8}, {64, 16}}; // copies of sorted literal-code runs
//
//     // Throws OperationError on any out-of-range field. read() calls it; the
//     // compressor calls it before writing anything.
//     void validate() const;
//
//     void write(ByteWriter &out) const noexcept;
//
//     // Consumes exactly the bytes write() produced. Throws OperationError on a
//     // truncated stream, a foreign file, an unknown version or invalid fields.
//     [[nodiscard]] static StreamParams read(ByteReader &in);
//
//     bool operator==(const StreamParams &) const = default;
// };
//
// // How an LZ stream picks between a match and coding tokens singly.
// enum class LzParser : uint8_t {
//     Greedy, // take the longest match found
//     CostBased, // take a match only if it costs fewer bits than the tokens it covers
// };
//
// // Search half of one LZ stream. Encoder-only.
// struct LzSearch {
//     uint8_t hash_bits;
//     uint32_t nice_match; // stop searching once a match this long is found
//     uint32_t max_search_depth; // hash-chain probes per position
//     LzParser parser;
//
//     // Matcher config = format half + search half.
//     [[nodiscard]] LZWindow::Config window(const LzShape &shape) const noexcept {
//         return LZWindow::Config{
//             .window_log = shape.window_log,
//             .hash_bits = hash_bits,
//             .max_match = shape.max_match,
//             .nice_match = nice_match,
//             .max_search_depth = max_search_depth,
//         };
//     }
// };
//
// // Encoder-only: how hard to search. Never written, never needed to decode.
// struct SearchParams {
//     //               hash_bits  nice_match  depth  parser
//     LzSearch hint_lz{16, 64, 128, LzParser::CostBased};
//     LzSearch literal_lz{16, 64, 128, LzParser::CostBased};
// };
