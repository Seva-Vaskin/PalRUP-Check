#pragma once

#include <array>
#include <cstdint>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"
#include "DistanceTable.hpp"
#include "LZ.hpp"

struct DeleteRecordParams {
    DistanceTable::Config gaps_table = {.exact_until = 256, .groups_per_level = 32};
    DistanceTable::Config first_table = {.exact_until = 256, .groups_per_level = 32};

    void fields(this auto &self, auto &&f) {
        f(self.gaps_table);
        f(self.first_table);
    }
};

struct ImportRecordParams {
    DistanceTable::Config id_table = {.exact_until = 256, .groups_per_level = 32};

    void fields(this auto &self, auto &&f) {
        f(self.id_table);
    }
};

struct LZParams {
    LZWindow::Config window = {
        .window_log = 15, .hash_bits = 16, .max_match = 1u << 16,
        .nice_match = 256, .max_search_depth = 128
    };
    DistanceTable::Config length_table = {.exact_until = 64, .groups_per_level = 8};
    DistanceTable::Config distance_table = {.exact_until = 64, .groups_per_level = 16};

    void fields(this auto &self, auto &&f) {
        f(self.window);
        f(self.length_table);
        f(self.distance_table);
    }
};

struct LiteralParams {
    DistanceTable::Config first_table = {.exact_until = 3072, .groups_per_level = 64};
    DistanceTable::Config gap_table = {.exact_until = 3072, .groups_per_level = 64};

    LZParams lz;

    void fields(this auto &self, auto &&f) {
        f(self.first_table);
        f(self.gap_table);
        f(self.lz);
    }
};

struct HintParams {
    DistanceTable::Config first_table = {.exact_until = 256, .groups_per_level = 32};
    DistanceTable::Config rest_table = {.exact_until = 256, .groups_per_level = 32};

    LZParams lz;

    void fields(this auto &self, auto &&f) {
        f(self.first_table);
        f(self.rest_table);
        f(self.lz);
    }
};

struct AddRecordParams {
    DistanceTable::Config jump_table = {.exact_until = 256, .groups_per_level = 32};

    void fields(this auto &self, auto &&f) {
        f(self.jump_table);
    }
};

struct Params {
    static constexpr std::array<uint8_t, 4> kMagic = {'P', 'R', 'P', 'H'};
    static constexpr uint8_t kVersion = 1;

    static constexpr uint64_t kMaxIdDelta = uint64_t{1} << 62;
    static constexpr uint64_t kMaxListLength = UINT32_MAX;
    static constexpr uint64_t kMaxThreadCount = uint64_t{1} << 16;
    static constexpr uint32_t kRecordTagCount = 5;
    static constexpr uint64_t kMaxLiteralCode = UINT32_MAX;

    uint32_t variable_count = 0;
    uint64_t clause_count = 0;
    uint32_t thread_count = 1;

    DeleteRecordParams delete_record;
    LiteralParams literals;
    ImportRecordParams import_record;
    HintParams hints;
    AddRecordParams add_record;

    void validate() const;

    void write(ByteWriter &out) const noexcept;

    [[nodiscard]] static Params read(ByteReader &in);

    void fields(this auto &self, auto &&f) {
        f(self.variable_count);
        f(self.clause_count);
        f(self.thread_count);

        f(self.delete_record);
        f(self.literals);
        f(self.import_record);
        f(self.hints);
        f(self.add_record);
    }
};
