cmake_minimum_required(VERSION 3.26)
project(snowblind_ingress C)

set(CMAKE_C_STANDARD 11)

# Add the CLAP library
add_subdirectory(lib/clap)

# Find ZeroMQ package
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

# Add the shared library target
add_library(snowblind_ingress SHARED main.c)

# Link the CLAP and ZeroMQ libraries to the shared library
target_link_libraries(snowblind_ingress PRIVATE clap ${ZeroMQ_LIBRARIES})

# Include ZeroMQ headers
target_include_directories(snowblind_ingress PRIVATE ${ZeroMQ_INCLUDE_DIRS})

# Set the library output directory
set_target_properties(snowblind_ingress PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
