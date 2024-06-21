cmake_minimum_required(VERSION 3.26)
project(snowblind_ingress C)

set(CMAKE_C_STANDARD 11)

# Add the CLAP library
add_subdirectory(lib/clap)

# Add the shared library target
add_library(snowblind_ingress SHARED main.c)

# Link the CLAP library to the shared library
target_link_libraries(snowblind_ingress PRIVATE clap)

# Set the library output directory
set_target_properties(snowblind_ingress PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
