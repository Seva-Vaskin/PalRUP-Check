#include "CnfHeader.hpp"

#include "error.hpp"

#include <cstdint>
#include <string>

namespace {
    [[noreturn]] void malformed(const char *what) {
        throw OperationError(std::string("cnf header: ") + what);
    }

    void skip_line(ByteReader &in) {
        for (int c = in.get(); c != '\n'; c = in.get()) {
            if (c == EOF) malformed("unexpected end of file");
        }
    }

    [[nodiscard]] int skip_blanks(ByteReader &in) {
        int c = in.get();
        while (c == ' ' || c == '\t') c = in.get();
        return c;
    }

    void expect_word(ByteReader &in, const char *word) {
        int c = skip_blanks(in);
        for (const char *p = word; *p != '\0'; ++p, c = in.get()) {
            if (c != *p) malformed("expected \"p cnf\"");
        }
        if (c != ' ' && c != '\t') malformed("expected \"p cnf\"");
    }

    [[nodiscard]] uint64_t read_number(ByteReader &in, int &c) {
        c = skip_blanks(in);
        if (c < '0' || c > '9') malformed("expected a number");
        uint64_t value = 0;
        for (; c >= '0' && c <= '9'; c = in.get()) {
            const auto digit = static_cast<uint64_t>(c - '0');
            if (value > (UINT64_MAX - digit) / 10) malformed("number too large");
            value = value * 10 + digit;
        }
        return value;
    }
}

CnfHeader CnfHeader::read(ByteReader &in) {
    for (;;) {
        const int c = in.get();
        if (c == EOF) malformed("no \"p cnf\" line");
        if (c == '\n' || c == '\r') continue;
        if (c == 'c') {
            skip_line(in);
            continue;
        }
        if (c != 'p') malformed("expected \"p cnf\"");
        break;
    }

    expect_word(in, "cnf");

    int c = 0;
    const uint64_t variables = read_number(in, c);
    const uint64_t clauses = read_number(in, c);

    while (c == ' ' || c == '\t' || c == '\r') c = in.get();
    if (c != '\n' && c != EOF) malformed("trailing characters after header");

    if (variables == 0 || variables > INT32_MAX) malformed("variable count out of range");

    return CnfHeader{
        .variable_count = static_cast<uint32_t>(variables),
        .clause_count = clauses,
    };
}