#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "PalRupCompression.hpp"
#include "error.hpp"

namespace {
    constexpr const char *kUsage =
            "usage:\n"
            "  palrup-compression encode <proof> <cnf> <output> --threads N\n"
            "  palrup-compression decode <input> <output>\n";

    [[nodiscard]] uint32_t parse_threads(const std::string_view flag, const char *value) {
        if (flag != "--threads" && flag != "-t") {
            throw OperationError("expected --threads N");
        }
        char *end = nullptr;
        errno = 0;
        const unsigned long long n = std::strtoull(value, &end, 10);
        if (errno != 0 || end == value || *end != '\0' || n == 0 || n > UINT32_MAX) {
            throw OperationError("--threads: expected a positive integer");
        }
        return static_cast<uint32_t>(n);
    }

    int run(const int argc, char **argv) {
        const std::string_view command = argc >= 2 ? argv[1] : "";
        if (command == "encode" && argc == 7) {
            PalRupCompressor::compress_file(argv[2], argv[3], argv[4], parse_threads(argv[5], argv[6]));
            return EXIT_SUCCESS;
        }
        if (command == "decode" && argc == 4) {
            PalRupDecompressor::decompress_file(argv[2], argv[3]);
            return EXIT_SUCCESS;
        }
        std::cerr << kUsage;
        return 2;
    }
}

int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const OperationError &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        std::abort();
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        std::abort();
    }
}
