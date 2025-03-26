cmake_minimum_required(VERSION 3.22)
project(exrprofile)

set(CMAKE_CXX_STANDARD 20)


# Find OpenEXR and Imath
find_package(OpenEXR REQUIRED)
find_package(Imath REQUIRED)

# Enable threading support
find_package(Threads REQUIRED)

add_executable(exrprofile src/main.cpp)

# Link OpenEXR and Imath libraries
target_link_libraries(exrprofile PRIVATE OpenEXR::OpenEXR Imath::Imath)

# Extra compiler flags for optimization and warnings

target_compile_options(exrprofile PRIVATE
    $<$<CONFIG:Release>:-Wall -Wextra -O3 -march=native>
    $<$<CONFIG:Debug>:-Wall -Wextra -O0 -g>
)
