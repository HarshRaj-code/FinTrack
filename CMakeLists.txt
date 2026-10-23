cmake_minimum_required(VERSION 3.14)
project(FinTrack VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FINTRACK_CORE_SOURCES
    src/DateUtils.cpp
    src/InputUtils.cpp
    src/Transaction.cpp
    src/Category.cpp
    src/Budget.cpp
    src/CsvStorage.cpp
    src/FinanceManager.cpp
)

add_executable(FinTrack
    main.cpp
    ${FINTRACK_CORE_SOURCES}
)
target_include_directories(FinTrack PRIVATE include)

add_executable(FinTrackTests
    tests/test_fintrack.cpp
    ${FINTRACK_CORE_SOURCES}
)
target_include_directories(FinTrackTests PRIVATE include)

if(MSVC)
    target_compile_options(FinTrack PRIVATE /W4 /utf-8)
    target_compile_options(FinTrackTests PRIVATE /W4 /utf-8)
else()
    target_compile_options(FinTrack PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(FinTrackTests PRIVATE -Wall -Wextra -Wpedantic)
endif()