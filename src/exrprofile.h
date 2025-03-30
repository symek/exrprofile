//
// Created by symek on 3/29/25.
//
#pragma once
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/OpenEXRConfig.h>
#include <Imath/half.h>
#include <array>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <execution>
#include <filesystem>
#include <thread>

#include <fmt/core.h>


namespace exrprofile {

    using Stats = std::array<long, 3>;
    enum Records {
        compression = 0,
        decompression = 1,
        filesize = 2
    };

    using Results = std::map<std::string, exrprofile::Stats>;
} // end of exrprofile namespace