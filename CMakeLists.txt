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

# Find OpenEXR and Imath
find_package(OpenEXR REQUIRED)
find_package(Imath REQUIRED)

# Enable threading support (FIXME: albeit neither std::transform(par) nor OpenEXR seem to use it atm)
find_package(Threads REQUIRED)

# code
add_executable(exrprofile src/main.cpp)

# Links
target_link_libraries(exrprofile PRIVATE OpenEXR::OpenEXR Imath::Imath)
target_link_libraries(exrprofile PRIVATE CLI11::CLI11)

# compiler flags
target_compile_options(exrprofile PRIVATE
    $<$<CONFIG:Release>:-Wall -Wextra -O3 -march=native>
    $<$<CONFIG:Debug>:-Wall -Wextra -O0 -g>
)
