cmake_minimum_required(VERSION 3.10)
project(waterfall-picker)
include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)
# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
set(RAYLIB_VERSION 5.5)
set(GTEST_VERSION 1.17.0)
FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(raylib)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME}
    raylib 
    sqlite3
)

option(ENABLE_TESTING "Enable unit tests" ON)

if (ENABLE_TESTING)
    # Fetch Google Test
    FetchContent_Declare(
        googletest
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/google/googletest/releases/download/v${GTEST_VERSION}/googletest-${GTEST_VERSION}.tar.gz
        FIND_PACKAGE_ARGS
    )
    FetchContent_MakeAvailable(googletest)

    # Enable testing
    enable_testing()

    # Add test executable
    add_executable(test_advance test/test_advance.cpp)
    target_include_directories(test_advance PRIVATE src)

    # Link test executable with Google Test and your project libraries
    target_link_libraries(test_advance
        gtest
        gtest_main
        raylib
        sqlite3
    )

    # Add tests to CTest
    add_test(NAME AdvanceTests COMMAND test_advance)
endif()
