//
// Created by symek on 3/27/25.
//
#include "exrprofile.h"
#include "mtread.h"

namespace exrprofile {

    void read_region(const std::string &filename, int y_start, int y_end, int width, std::atomic<int> &completed) {
        try {
            Imf::RgbaInputFile file(filename.c_str());
            Imath::Box2i dw = file.dataWindow();
            int height = dw.max.y - dw.min.y + 1;

            // Allocate storage for the region
            std::vector<Imf::Rgba> pixels(width * (y_end - y_start + 1));

            file.setFrameBuffer(pixels.data() - dw.min.x - y_start * width, 1, width);
            file.readPixels(y_start, y_end);

            // Track the number of completed regions
            completed.fetch_add(1, std::memory_order_relaxed);
            std::cout << "Read region from Y: " << y_start << " to Y: " << y_end << " by thread "
                      << std::this_thread::get_id() << std::endl;

        } catch (const std::exception &e) {
            std::cerr << "Error reading EXR file region: " << e.what() << std::endl;
        }
    }

    long multithreaded_read(const std::string & filename, const int num_threads) {


        // Set global thread count for OpenEXR
        Imf::setGlobalThreadCount(num_threads);
        std::cout << "Using " << num_threads << " OpenEXR threads." << std::endl;

        long result = 0;

        try {
            Imf::RgbaInputFile file(filename.c_str());
            Imath::Box2i dw = file.dataWindow();
            int width = dw.max.x - dw.min.x + 1;
            int height = dw.max.y - dw.min.y + 1;

            int chunk_size = height / num_threads;
            std::vector<std::thread> threads;
            std::atomic<int> completed(0);

            // Launch threads to read different regions of the image
            for (int i = 0; i < num_threads; ++i) {
                int y_start = dw.min.y + i * chunk_size;
                int y_end = (i == num_threads - 1) ? dw.max.y : y_start + chunk_size - 1;
                threads.emplace_back(read_region, std::ref(filename), y_start, y_end, width, std::ref(completed));
            }

            const auto start_decompress = std::chrono::high_resolution_clock::now();

            // Join threads
            for (auto &t: threads) {
                t.join();
            }
            const auto end_decompress = std::chrono::high_resolution_clock::now();
            const auto decompression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_decompress - start_decompress).count();
            fmt::print("{:>15}: {:.6f} seconds\n", "decompression", (double) decompression_time / 1024);
            std::cout << "All regions read. Total completed: " << completed.load() << std::endl;
            result = decompression_time;

        } catch (const std::exception &e) {
            std::cerr << "Error reading EXR file: " << e.what() << std::endl;
        }
        return result;
    }

}