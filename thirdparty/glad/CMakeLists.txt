cmake_minimum_required(VERSION 3.10)
project(glad)

add_library(glad)
target_sources(glad PRIVATE src/glad.c "src/errorReporting.cpp")
target_include_directories(glad PUBLIC include)