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
#include <thread>



// Generate random float data in the range [0, 1] with noise
std::vector<Imath::half> generate_channel_data(const int width, const int height) {
    std::vector<Imath::half> data(width * height);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 0.5f);
    std::normal_distribution<float> noise(0.0f, 0.15f);

//    for (auto& pixel : data) {
//        float value = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
//        pixel = Imath::half(value);
//    }

    std::generate(std::execution::par, data.begin(), data.end(), [&]() {
    const float value = std::clamp(dist(gen) + noise(gen), 0.0f, 1.0f);
    return Imath::half(value);
});

    return data;
}

void save_exr(const std::string& filename, const std::vector<Imath::half>& r, const std::vector<Imath::half>& g,
              const std::vector<Imath::half>& b, const int width, const int height, Imf::Compression compression,
              const int threads) {
    try {


        Imf::RgbaOutputFile file(filename.c_str(),
                                 width, height,
                                 Imf::WRITE_RGBA,
                                 1,
                                 Imath::V2f {0,0}, 1,
                                 Imf::LineOrder::INCREASING_Y,
                                 compression,
                                 threads);

        std::vector<Imf::Rgba> pixels(width * height);
        std::vector<int> indices(width * height);

        // Fill the indices with sequential numbers (0, 1, 2, ...)
        std::iota(indices.begin(), indices.end(), 0);

        // Use transform to fill pixel data based on the generated indices
        std::transform(std::execution::par_unseq, indices.begin(), indices.end(), pixels.begin(),
            [&](int index) {
                Imf::Rgba pixel;
                const float ramp = static_cast<float>(index) / (width*height);
                pixel.r = ramp + r[index];
                pixel.g = (1.0f - ramp) + g[index];
                pixel.b = b[index];
                pixel.a = Imath::half(1.0f);  // Alpha channel (optional)
                return pixel;
            });


        file.setFrameBuffer(pixels.data(), 1, width);
        file.writePixels(height);
    } catch (const std::exception& e) {
        std::cerr << "Error saving EXR file: " << e.what() << std::endl;
    }
}

void load_exr(const std::string& filename) {
    try {
        Imf::RgbaInputFile file(filename.c_str());
        Imath::Box2i dw = file.dataWindow();
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        std::vector<Imf::Rgba> pixels(width * height);
        file.setFrameBuffer(pixels.data(), 1, width);
        file.readPixels(dw.min.y, dw.max.y);
    } catch (const std::exception& e) {
        std::cerr << "Error loading EXR file: " << e.what() << std::endl;
    }
}

int main() {
    constexpr int mult = 8;
    constexpr int width = 1024*mult;
    constexpr int height = 1024*mult;
    constexpr int threads = 10;
    // Generate random channel data
    std::cout << "=== Generating random data: " << width << "x" << height << " === "<< std::endl;
    std::vector<Imath::half> r = generate_channel_data(width, height);
    std::vector<Imath::half> g = generate_channel_data(width, height);
    std::vector<Imath::half> b = generate_channel_data(width, height);

#if OPENEXR_VERSION_MINOR == 3
    const auto compression_list = std::array<int, 10>{0,1,2,3,4,5,6,7,8,9};
#elif OPENEXR_VERSION_MINOR == 4
    const auto compression_list = std::array<int, 11>{0,1,2,3,4,5,6,7,8,9,10};
#endif

    auto compression_name       = std::string{};
    std::cout << "=== Profiling compressions" << " ===" << std::endl;
    for (const auto compression: compression_list) {
        getCompressionNameFromId(static_cast<Imf::Compression>(compression), compression_name);
        const auto filename = std::string{"test_"} + compression_name + std::string{".exr"};
        // Measure compression time
        auto start_compress = std::chrono::high_resolution_clock::now();
        save_exr(filename, r, g, b, width, height, (Imf::Compression) compression, threads);
        auto end_compress = std::chrono::high_resolution_clock::now();
        auto compression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_compress - start_compress).count();
//        std::cout << std::format("{:>25} {:<30}: {:.6f} seconds\n", comp, action, time); no c++20 format in gcc 11.5 :(
        std::cout  << compression_name << " compression time : " << compression_time << " ms" << std::endl;

        // Measure decompression time
        auto start_decompress = std::chrono::high_resolution_clock::now();
        load_exr(filename);
        auto end_decompress = std::chrono::high_resolution_clock::now();
        auto decompression_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_decompress - start_decompress).count();
        std::cout << compression_name << " decompression time: " << decompression_time << " ms" << std::endl;
    }

    return 0;
}
