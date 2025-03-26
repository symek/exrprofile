#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/OpenEXRConfig.h>
#include <Imath/half.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <execution>
#include <filesystem>
#include <thread>
#include <CLI/CLI.hpp>
#include <fmt/core.h>

namespace exrprofile {


    void generate_and_save_exr(const std::string &filename, const int width, const int height, Imf::Compression compression,
                               const int threads) {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 0.5f);
        std::normal_distribution<float> noise(0.0f, 0.15f);

        std::vector<Imf::Rgba> pixels(width * height);
        std::vector<int> indices(width * height);

        // Fill the indices with sequential numbers (0, 1, 2, ...)
        std::iota(indices.begin(), indices.end(), 0);


        // Use transform to fill pixel data based on the generated indices
        std::transform(std::execution::par_unseq, indices.begin(), indices.end(), pixels.begin(),
                       [&](int index) {
                           Imf::Rgba pixel;
                           const float ramp = static_cast<float>(index) / (width * height);
                           const float r = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
                           const float g = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
                           const float b = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
                           pixel.r = Imath::half(ramp + r);
                           pixel.g = Imath::half((1.0f - ramp) + g);
                           pixel.b = Imath::half(b);
                           pixel.a = Imath::half(1.0f);  // Alpha channel (optional)
                           return pixel;
                       });

        try {

            Imf::RgbaOutputFile file(filename.c_str(),
                                     width, height,
                                     Imf::WRITE_RGBA,
                                     1,
                                     Imath::V2f{0, 0}, 1,
                                     Imf::LineOrder::INCREASING_Y,
                                     compression,
                                     threads);


            file.setFrameBuffer(pixels.data(), 1, width);
            file.writePixels(height);
        } catch (const std::exception &e) {
            std::cerr << "Error saving EXR file: " << e.what() << std::endl;
        }
    }

    void load_exr(const std::string &filename) {
        try {
            Imf::RgbaInputFile file(filename.c_str());
            Imath::Box2i dw = file.dataWindow();
            int width = dw.max.x - dw.min.x + 1;
            int height = dw.max.y - dw.min.y + 1;

            std::vector<Imf::Rgba> pixels(width * height);
            file.setFrameBuffer(pixels.data(), 1, width);
            file.readPixels(dw.min.y, dw.max.y);
        } catch (const std::exception &e) {
            std::cerr << "Error loading EXR file: " << e.what() << std::endl;
        }
    }
}

void delete_test_file(const std::string &filename) {
    try {
        if (std::filesystem::remove(filename)) {
            fmt::print("=== file: {} deleted successfully.\n", filename);
        } else {
            fmt::print("=== file: {} not found or already deleted.\n", filename);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error deleting file: " << e.what() << std::endl;
    }
}



int main(int argc, char** argv) {
    CLI::App app{"EXR Profiler"};

    int scale = 1;
    int threads = 1;
    bool cleanup = false;
    auto prefix = std::string{"./test_"};

    app.add_option("-f,--file", prefix, "Prefix to the EXR files (default ./test_ )");
    app.add_option("-t,--threads", threads, "Number of threads (default 1)");
    app.add_option("-s,--scale", scale, "Multiply of 1Kx1K test size (default 1)");
    app.add_flag("-c,--clean", cleanup, "Cleanup the files");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    const int width  = std::clamp(scale, 1, 32) * 1024;
    const int height = width;

    // Generate random channel data
    std::cout << "=== Generating random data: " << width << "x" << height <<  ", threads " << threads << " === " << std::endl;

    auto compression_list = std::vector<int>(Imf::Compression::NUM_COMPRESSION_METHODS);
    std::iota(compression_list.begin(), compression_list.end(), 0);
    auto compression_name = std::string{};

    using stats =  std::array<long, 3>;
    auto results = std::map<std::string, stats>{};

    std::cout << "=== Profiling compressions" << " ===" << std::endl;
    for (const auto compression: compression_list) {
        getCompressionNameFromId(static_cast<Imf::Compression>(compression), compression_name);
        const auto filename = prefix + compression_name + std::string{".exr"};
        auto compression_description = std::string{};
        getCompressionDescriptionFromId(static_cast<Imf::Compression>(compression), compression_description);

        // Measure compression time
        const auto start_compress = std::chrono::high_resolution_clock::now();
        exrprofile::generate_and_save_exr(filename, width, height, (Imf::Compression) compression, threads);
        const auto end_compress = std::chrono::high_resolution_clock::now();
        const auto compression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_compress - start_compress).count();

        const std::uintmax_t filesize = std::filesystem::file_size(filename);
        fmt::print("=== {} ==\n", compression_description);
        fmt::print("{:>15}: {:.6f} seconds\n", "compression", (double)compression_time / 1024);


        // Measure decompression time
        const auto start_decompress = std::chrono::high_resolution_clock::now();
        exrprofile::load_exr(filename);
        const auto end_decompress = std::chrono::high_resolution_clock::now();
        const auto decompression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_decompress - start_decompress).count();
        fmt::print("{:>15}: {:.6f} seconds\n", "decompression", (double)decompression_time / 1024);


        // Store stats
        results[compression_name] = {compression_time, decompression_time, (long) filesize};

        // Optionally cleanup our mess
        if (cleanup)
            delete_test_file(filename);

    }

    // Convert map to vector of pairs for sorting
    std::vector<std::pair<std::string, stats>> sorted_results(results.begin(), results.end());


    {
        std::cout << "\nSorted by Compression Time:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) { return a.second[0] < b.second[0]; });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {} ms -> size: {:.2f}MB \n", name, stat[0], (double) stat[2] / (1024 * 1024));
        }
    }


    {
        std::cout << "\nSorted by Decompression Time:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) { return a.second[1] < b.second[1]; });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {} ms -> size: {:.2f}MB \n", name, stat[1], (double) stat[2] / (1024 * 1024));
        }
    }


    {
        std::cout << "\nSorted by File Size:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) { return a.second[2] < b.second[2]; });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {:.2f}MB -> {} ms \n", name, (double) stat[2] / (1024 * 1024), stat[1]);
        }
    }


    return 0;
}

