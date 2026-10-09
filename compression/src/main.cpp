#include <iostream>
#include "error.hpp"

int main(int argc, char** argv) {
    try {
        std::cerr << "CLI for palrup-compression is not implemented." << std::endl;
        return 1;
    }
    catch (const OperationError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        std::abort();
    }
    catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        std::abort();
    }
}