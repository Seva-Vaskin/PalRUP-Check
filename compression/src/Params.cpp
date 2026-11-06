#include "Params.hpp"

#include "error.hpp"

#include <string>

namespace {
    [[noreturn]] void invalid(const char *field) {
        throw OperationError(std::string("stream params: ") + field + " out of range");
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
                throw OperationError("stream params: unexpected end of stream");
            }
            value |= static_cast<uint64_t>(c) << (8 * i);
        }
        return value;
    }

    void put_shape(ByteWriter &out, const TableShape &s) noexcept {
        put_le(out, s.exact_until, 8);
        put_le(out, s.groups_per_level, 4);
        put_le(out, s.start_extra_bits, 4);
    }

    [[nodiscard]] TableShape get_shape(ByteReader &in) {
        TableShape s{};
        s.exact_until = get_le(in, 8);
        s.groups_per_level = static_cast<uint32_t>(get_le(in, 4));
        s.start_extra_bits = static_cast<uint32_t>(get_le(in, 4));
        return s;
    }

    void put_lz(ByteWriter &out, const LzShape &lz) noexcept {
        put_le(out, lz.window_log, 1);
        put_le(out, lz.max_match, 4);
        put_shape(out, lz.length);
        put_shape(out, lz.distance);
    }

    [[nodiscard]] LzShape get_lz(ByteReader &in) {
        LzShape lz{};
        lz.window_log = static_cast<uint8_t>(get_le(in, 1));
        lz.max_match = static_cast<uint32_t>(get_le(in, 4));
        lz.length = get_shape(in);
        lz.distance = get_shape(in);
        return lz;
    }
}

void TableShape::validate(const char *field) const {
    if (exact_until > kMaxExactUntil
        || groups_per_level == 0 || groups_per_level > kMaxGroupsPerLevel
        || start_extra_bits > kMaxStartExtraBits) {
        invalid(field);
    }
}

void LzShape::validate(const char *field) const {
    if (window_log != 0 && !LZWindow::valid_window_log(window_log)) invalid(field);
    if (max_match < kMinMatch || max_match > kMaxMatch) invalid(field);
    length.validate(field);
    distance.validate(field);
}

void StreamParams::validate() const {
    // Literals are int32 and never 0, so 1 <= variable_count <= INT32_MAX.
    // variable_count >= 1 also keeps every literal table non-empty.
    if (variable_count == 0 || variable_count > INT32_MAX) invalid("variable_count");
    if (thread_count == 0) invalid("thread_count");

    add_id_jump.validate("add_id_jump");
    import_id.validate("import_id");
    literal_count.validate("literal_count");
    literal_first.validate("literal_first");
    literal_gap.validate("literal_gap");
    hint_count.validate("hint_count");
    hint_delta.validate("hint_delta");
    delete_count.validate("delete_count");
    delete_delta.validate("delete_delta");

    hint_lz.validate("hint_lz");
    literal_lz.validate("literal_lz");
}

void StreamParams::write(ByteWriter &out) const noexcept {
    for (const uint8_t b: kMagic) {
        out.put(b);
    }
    out.put(kVersion);

    put_le(out, variable_count, 4);
    put_le(out, clause_count, 8);
    put_le(out, thread_count, 4);

    put_shape(out, add_id_jump);
    put_shape(out, import_id);
    put_shape(out, literal_count);
    put_shape(out, literal_first);
    put_shape(out, literal_gap);
    put_shape(out, hint_count);
    put_shape(out, hint_delta);
    put_shape(out, delete_count);
    put_shape(out, delete_delta);

    put_lz(out, hint_lz);
    put_lz(out, literal_lz);
}

StreamParams StreamParams::read(ByteReader &in) {
    for (const uint8_t b: kMagic) {
        if (get_le(in, 1) != b) {
            throw OperationError("stream params: not a compressed PalRUP file (bad magic)");
        }
    }
    if (get_le(in, 1) != kVersion) {
        throw OperationError("stream params: unsupported format version");
    }

    StreamParams p;
    p.variable_count = static_cast<uint32_t>(get_le(in, 4));
    p.clause_count = get_le(in, 8);
    p.thread_count = static_cast<uint32_t>(get_le(in, 4));

    p.add_id_jump = get_shape(in);
    p.import_id = get_shape(in);
    p.literal_count = get_shape(in);
    p.literal_first = get_shape(in);
    p.literal_gap = get_shape(in);
    p.hint_count = get_shape(in);
    p.hint_delta = get_shape(in);
    p.delete_count = get_shape(in);
    p.delete_delta = get_shape(in);

    p.hint_lz = get_lz(in);
    p.literal_lz = get_lz(in);

    p.validate();
    return p;
}