//
// Created by symek on 3/28/25.
//
# pragma once

#include <iostream>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfThreading.h>
#include <OpenEXR/ImfArray.h>
#include <Imath/ImathBox.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fmt/core.h>
#include "threadpool.h"

namespace exrprofile {

    void read_region(Imf::RgbaInputFile &, int y_start, int y_end, int width, std::atomic<int> &completed);
    long multithreaded_read(const std::string & filename, int num_threads, ThreadPool &);

}