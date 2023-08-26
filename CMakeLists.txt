cmake_minimum_required(VERSION 3.20)

project (
    w8ress
)

# Compilation settings
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)  # I'm fine

# Some sanitizers
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${SRC_DIR}/**/*.cpp)
# file(GLOB SOURCES ${SRC_DIR}/*/*/*.cpp)

add_executable(w8ress ${SRC_DIR}/main.cpp ${SOURCES})

target_include_directories(w8ress PUBLIC ${SRC_DIR})