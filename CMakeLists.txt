cmake_minimum_required(VERSION 3.28.3)
project(redis)

set(CMAKE_CXX_STANDARD 23)

include_directories(include)
add_executable(redis src/main.cpp
        include/connection.h
        src/connections.cpp
        include/parser.h
        src/parser.cpp
        src/marshal.cpp)
