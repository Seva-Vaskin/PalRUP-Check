#include "Params.hpp"

#include <string>
#include <type_traits>

#include "error.hpp"

namespace {
    [[noreturn]] void invalid(const char *field) {
        throw OperationError(std::string("params: ") + field + " out of range");
    }

    void put_le(ByteWriter &out, uint64_t value, const unsigned bytes) noexcept {
        for (unsigned i = 0; i < bytes; ++i) {
            out.put(static_cast<uint8_t>(value));
            value >>= 8;
        }
    }

    [[nodiscard]] uint64_t get_le(ByteReader &in, const unsigned bytes) {
        uint64_t value = 0;
        for (unsigned i = 0; i < bytes; ++i) {
            const int c = in.get();
            if (c == EOF) {
                throw OperationError("params: unexpected end of stream");
            }
            value |= static_cast<uint64_t>(c) << (8 * i);
        }
        return value;
    }

    template<class T>
    void write_field(ByteWriter &out, const T &value) noexcept {
        if constexpr (std::is_integral_v<T>) {
            put_le(out, value, sizeof(T));
        } else {
            value.fields([&](const auto &field) { write_field(out, field); });
        }
    }

    template<class T>
    void read_field(ByteReader &in, T &value) {
        if constexpr (std::is_integral_v<T>) {
            value = static_cast<T>(get_le(in, sizeof(T)));
        } else {
            value.fields([&](auto &field) { read_field(in, field); });
        }
    }

    void check(const DistanceTable::Config &config, const char *field) {
        if (!config.valid()) {
            invalid(field);
        }
    }
}

void Params::validate() const {
    if (variable_count == 0 || variable_count > INT32_MAX) {
        invalid("variable_count");
    }
    if (thread_count == 0 || thread_count > kMaxThreadCount) {
        invalid("thread_count");
    }

#define CHECK_TABLE(field) check(field, #field)

    CHECK_TABLE(delete_record.gaps_table);
    CHECK_TABLE(delete_record.first_table);
    CHECK_TABLE(literals.first_table);
    CHECK_TABLE(literals.gap_table);
    CHECK_TABLE(import_record.id_table);
    CHECK_TABLE(hints.first_table);
    CHECK_TABLE(hints.rest_table);
    CHECK_TABLE(add_record.jump_table);

#undef CHECK_TABLE
}

void Params::write(ByteWriter &out) const noexcept {
    for (const uint8_t b: kMagic) {
        out.put(b);
    }
    out.put(kVersion);
    write_field(out, *this);
}

Params Params::read(ByteReader &in) {
    for (const uint8_t b: kMagic) {
        if (get_le(in, 1) != b) {
            throw OperationError("params: not a compressed PalRUP file (bad magic)");
        }
    }
    if (get_le(in, 1) != kVersion) {
        throw OperationError("params: unsupported format version");
    }

    Params p;
    read_field(in, p);
    p.validate();
    return p;
}
