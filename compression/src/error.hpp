#pragma once


#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <stdexcept>


[[noreturn]]
inline void invariant_violation(
    const char* expression,
    const std::source_location location =
        std::source_location::current()
) noexcept {
    std::fprintf(
        stderr,
        "Invariant violation: %s at %s:%u\n",
        expression,
        location.file_name(),
        location.line()
    );
    std::abort();
}

#define CHECK(expr) \
    ((expr) ? (void)0 : invariant_violation(#expr))


class OperationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};