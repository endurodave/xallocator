# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A Win32 -B ../xallocator_build -S .
#
# *** Linux ***
# cmake -G "Unix Makefiles" -B ../xallocator_build -S .

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(xallocator VERSION 1.0 LANGUAGES CXX)

# Collect all .cpp and *.h source files in the current directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Add an executable target
add_executable(xallocator_app ${SOURCES})



