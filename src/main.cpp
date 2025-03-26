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
int main() {
    constexpr int mult = 8;
    constexpr int width = 1024*mult;
    constexpr int height = 1024*mult;
    constexpr int threads = 1;
    // Generate random channel data
    std::cout << "=== Generating random data: " << width << "x" << height << " === "<< std::endl;

    auto compression_list = std::vector<int>(Imf::Compression::NUM_COMPRESSION_METHODS);
    std::iota(compression_list.begin(), compression_list.end(), 0);
    auto compression_name = std::string{};

    using stats =  std::array<long, 3>;
    auto results = std::map<std::string, stats>{};

    std::cout << "=== Profiling compressions" << " ===" << std::endl;
    for (const auto compression: compression_list) {
        getCompressionNameFromId(static_cast<Imf::Compression>(compression), compression_name);
        const auto filename = std::string{"test_"} + compression_name + std::string{".exr"};

        // Measure compression time
        const auto start_compress = std::chrono::high_resolution_clock::now();
        exrprofile::generate_and_save_exr(filename, width, height, (Imf::Compression) compression, threads);
        const auto end_compress = std::chrono::high_resolution_clock::now();
        const auto compression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_compress - start_compress).count();
        // std::cout << std::format("{:>25} {:<30}: {:.6f} seconds\n", comp, action, time); no c++20 format in gcc 11.5 :(
        auto compression_description = std::string{};
        getCompressionDescriptionFromId(static_cast<Imf::Compression>(compression), compression_description);
        std::uintmax_t filesize = std::filesystem::file_size(filename);
        std::cout << "=== " << compression_description << " === " << std::endl;
        std::cout << "  compression: " << compression_time << " ms" << std::endl;


        // Measure decompression time
        const auto start_decompress = std::chrono::high_resolution_clock::now();
        exrprofile::load_exr(filename);
        const auto end_decompress = std::chrono::high_resolution_clock::now();
        const auto decompression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_decompress - start_decompress).count();
        std::cout << "decompression: " << decompression_time << " ms" << std::endl;


        results[compression_name] = {compression_time, decompression_time, (long) filesize};

    }

    // Convert map to vector of pairs for sorting
    std::vector<std::pair<std::string, stats>> sorted_results(results.begin(), results.end());


    std::cout << "Sorted by Compression Time:\n";
    std::sort(sorted_results.begin(), sorted_results.end(),
              [](const auto &a, const auto &b) { return a.second[0] < b.second[0]; });


    for (const auto &[name, stat]: sorted_results) {
        std::cout << std::setw(25) << std::right << name
                  << ": " << std::setw(4) << stat[0] << " ms"
                  << std::fixed << std::setprecision(2)
                  << " -> size: " << stat[2] / (1024 * 1024) << " MB"
                  << std::endl;
    }

    std::cout << "\nSorted by Decompression Time:\n";
    std::sort(sorted_results.begin(), sorted_results.end(),
              [](const auto &a, const auto &b) { return a.second[1] < b.second[1]; });


    for (const auto &[name, stat]: sorted_results) {
        std::cout << std::setw(25) << std::right << name
                  << ": " << std::setw(4) << stat[1]
                  << " ms" << std::fixed << std::setprecision(2)
                  << " -> " << stat[2] / (1024 * 1024) << " MB"
                  << std::endl;
    }

    std::cout << "\nSorted by File Size:\n";
    std::sort(sorted_results.begin(), sorted_results.end(),
              [](const auto &a, const auto &b) { return a.second[2] < b.second[2]; });


    for (const auto &[name, stat]: sorted_results) {
        std::cout << std::setw(25) << std::right << name
                  << std::fixed << std::setprecision(2)
                  << ": " << stat[2] / (1024 * 1024) << " MB"
                  << " -> " << std::setw(4) << stat[1] << " ms"
                  << std::endl;
    }


    return 0;
}
