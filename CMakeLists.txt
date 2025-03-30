cmake_minimum_required(VERSION 3.22)
project(exrprofile)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Fetch CLI11 from GitHub
FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.3.2  # Latest stable release
)

# Make CLI11 available
FetchContent_MakeAvailable(CLI11)

# Lets add fmt to the mix (we are on gcc-11.5 without <format>)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.1.1  # Latest stable release
)

FetchContent_MakeAvailable(fmt)

# Find OpenEXR and Imath
find_package(OpenEXR REQUIRED)
find_package(Imath REQUIRED)

# Enable threading support (FIXME: std::transform(par) can't use it atm, probably due to sharing state between threads)
find_package(Threads REQUIRED)

# code
add_executable(exrprofile
        src/exrprofile.cpp
        src/exrprofile.h
        src/mtread.cpp
        src/mtread.h
        src/threadpool.h)

# Links
target_link_libraries(exrprofile PRIVATE OpenEXR::OpenEXR Imath::Imath)
target_link_libraries(exrprofile PRIVATE CLI11::CLI11 fmt::fmt)
target_link_libraries(exrprofile PRIVATE Threads::Threads)

# compiler flags
target_compile_options(exrprofile PRIVATE
    $<$<CONFIG:Release>:-Wall -Wextra -O3 -march=native>
    $<$<CONFIG:Debug>:-Wall -Wextra -O0 -g>
)
