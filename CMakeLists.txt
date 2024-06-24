cmake_minimum_required(VERSION 3.26)
project(snowblind_ingress)

set(CMAKE_C_STANDARD 11)

# Add the CLAP library
add_subdirectory(lib/clap)

# Add clap-wrapper
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
set(CLAP_WRAPPER_OUTPUT_NAME "snowblind_ingress_vst3")
add_subdirectory(lib/clap-wrapper)

# Find ZeroMQ package
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED libzmq)

# Add the shared library target
add_library(snowblind_ingress SHARED src/main.c)

# Link the CLAP and ZeroMQ libraries to the shared library
target_link_libraries(snowblind_ingress PRIVATE clap ${ZeroMQ_LIBRARIES})

# Include ZeroMQ headers
target_include_directories(snowblind_ingress PRIVATE ${ZeroMQ_INCLUDE_DIRS})

# Set the library output directory and file name
set_target_properties(snowblind_ingress PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_NAME "snowblind_ingress"
        SUFFIX ".clap"
        PREFIX ""
)

# VST3 target using clap-wrapper
set(VST3_TARGET ${PROJECT_NAME}_vst3)
add_library(${VST3_TARGET} MODULE)
target_add_vst3_wrapper(TARGET ${VST3_TARGET}
        OUTPUT_NAME "Snowblind Ingress"
        SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
)