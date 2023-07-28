cmake_minimum_required(VERSION 3.20)

project (
    w8ress
)

set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
add_executable(w8ress ${SRC_DIR}/main.cpp)