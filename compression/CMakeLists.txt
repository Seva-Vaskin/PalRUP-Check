cmake_minimum_required(VERSION 3.30)

project(
        palrup-compression
        VERSION 1.0
        LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# ---------------- Library ----------------

add_library(palrup-compression STATIC
        include/compression.h
        src/types.hpp
        src/ByteReader.hpp
        src/ByteWriter.hpp
        src/BitReader.hpp
        src/BitWriter.hpp
        src/VitterCoder.cpp
        src/VitterCoder.hpp
        src/LZ.cpp
        src/LZ.hpp
        src/error.hpp
        src/utils.hpp
        src/PalRupReader.hpp
        src/PalRupReader.cpp
        src/PalRupWriter.cpp
        src/PalRupWriter.hpp
        src/vbl.hpp
        src/DistanceTable.cpp
        src/DistanceTable.hpp
        src/Params.cpp
        src/Params.hpp
        src/RecordCoder.cpp
        src/RecordCoder.hpp
        src/BitReader.cpp
        src/BitWriter.cpp
        src/ByteReader.cpp
        src/ByteWriter.cpp
        src/DeleteRecordCoder.cpp
        src/DeleteRecordCoder.hpp
        src/LiteralCoder.cpp
        src/LiteralCoder.hpp
        src/ImportRecordCoder.cpp
        src/ImportRecordCoder.hpp
        src/CnfHeader.cpp
        src/CnfHeader.hpp
        src/HintCoder.cpp
        src/HintCoder.hpp
        src/AddRecordCoder.cpp
        src/AddRecordCoder.hpp
        src/PalRupCompression.cpp
        src/PalRupCompression.hpp
        src/MatchCoder.cpp
        src/MatchCoder.hpp
        src/RecencyCache.cpp
        src/RecencyCache.hpp
)

add_library(palrup::compression ALIAS palrup-compression)

target_include_directories(palrup-compression
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(palrup-compression
        PUBLIC cxx_std_23
)

target_compile_options(palrup-compression
        PRIVATE -Wall -Wextra
)

set_target_properties(palrup-compression PROPERTIES
        CXX_EXTENSIONS OFF
)

# ---------------- CLI ----------------
option(
        PALRUP_COMPRESSION_BUILD_CLI
        "Build palrup-compression command-line tool"
        ${PROJECT_IS_TOP_LEVEL}
)

if(PALRUP_COMPRESSION_BUILD_CLI)
    add_executable(palrup-compression-cli
            src/main.cpp
    )

    target_link_libraries(palrup-compression-cli
            PRIVATE
            palrup::compression
    )

    set_target_properties(palrup-compression-cli PROPERTIES
            OUTPUT_NAME palrup-compression
    )
endif()