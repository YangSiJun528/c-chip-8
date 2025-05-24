cmake_minimum_required(VERSION 3.30)
project(c_chip_8 C)

set(CMAKE_C_STANDARD 99)

add_executable(c_chip_8 src/main.c src/log.c)
