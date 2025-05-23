
#include <CLI/CLI.hpp>
#include "exrprofile.h"
#include "mtread.h"
#include "threadpool.h"
#include "stats.h"

namespace exrprofile {

    void print_sorted_stats(const exrprofile::Results & results) {
        // Convert map to vector of pairs for sorting FIXME: make stats container suitable for sorting
        std::vector<std::pair<std::string, exrprofile::Stats>> sorted_results(results.begin(), results.end());

        using namespace exrprofile;
        std::cout << "\nSorted by Compression Time:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) {
                      return a.second[Records::compression] < b.second[Records::compression];
                  });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {} ms -> size: {:.2f}MB \n", name, stat[Records::compression],
                       (double) stat[Records::filesize] / (1024 * 1024));
        }

        std::cout << "\nSorted by Decompression Time:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) {
                      return a.second[Records::decompression] < b.second[Records::decompression];
                  });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {} ms -> size: {:.2f}MB \n", name, stat[Records::decompression],
                       (double) stat[Records::filesize] / (1024 * 1024));
        }

        std::cout << "\nSorted by File Size:\n";
        std::sort(sorted_results.begin(), sorted_results.end(),
                  [](const auto &a, const auto &b) { return a.second[Records::filesize] < b.second[Records::filesize]; });
        for (const auto &[name, stat]: sorted_results) {
            fmt::print("{:>25}: {:.2f}MB -> {} ms \n", name, (double)
                                                                     stat[Records::filesize] / (1024 * 1024),
                       stat[Records::decompression]);
        }
    }

    std::vector<std::string> parse_file_list(const std::string &list_path) {
        std::ifstream file(list_path);
        std::vector<std::string> filenames;

        if (!file.is_open()) {
            throw std::runtime_error("Could not open file list: " + list_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            line.erase(std::find_if(line.rbegin(), line.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), line.end());

            if (!line.empty()) {
                filenames.push_back(std::move(line));
            }
        }

        return filenames;
    }


    std::vector<Imf::Rgba> generate_synthetic_pixels(const int width, const int height) {

        // ever wonder if RVO is a real thing ;)
        std::vector<Imf::Rgba> pixels(width * height);
        std::vector<int> indices(width * height);


        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 0.5f);
        std::normal_distribution<float> noise(0.0f, 0.15f);

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

        return pixels;
    }

    void
    save_exr_file(const std::vector<Imf::Rgba> &pixels, const std::string &filename, const int width, const int height,
                  const Imf::Compression compression, const int threads) {
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

    void load_exr_file(const std::string &filename) {
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

} // end of exrprofile namespace

int main(int argc, char **argv) {
    CLI::App app{"EXR Profiler"};

    int scale = 1;
    int threads = 1;
    int workers = 1;
    bool cleanup = false;
    bool verbose = false;
    auto prefix = std::string{"./test_"};
    bool mt_read = false;
    std::vector<std::string> files;
    auto list = std::string{""};

    // Basic timing tools
    using clock = std::chrono::high_resolution_clock;
    auto timeit = [](auto &&start) {
        const auto end = clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    };

    app.add_option("-p,--prefix", prefix, "Prefix to the EXR files (default ./test_ )");
    app.add_option("-t,--threads", threads, "Number of threads per frame (default 1)");
    app.add_option("-w,--workers", workers, "Number of thread workers (x threads) (default 1)");
    app.add_option("-s,--scale", scale, "Multiply of 1Kx1K test size (default 1)");
    app.add_flag("-c,--clean", cleanup, "Cleanup the files");
    app.add_flag("-v,--verbose", verbose, "Be more verbose");
    app.add_flag("-r,--read", mt_read, "Profile multi-thread reading");
    app.add_option("-f,--files", files, "Files to use for multi-thread reading")->expected(-1);
    app.add_option("-l,--list", list, "Text file with test EXRs to proceed with (alternatively to -f)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (list != "") {
        files = exrprofile::parse_file_list(list);
        if (files.size() == 0)
            return 1;
    }


    if (mt_read) {
        fmt::print("=== Profiling read from a file with {} threads per frame, and {} worker frames \n", threads,
                   workers);
        auto results = exrprofile::Results{};
        std::vector<std::thread> frame_threads;

        for (const auto &filename: files) {
            const std::uintmax_t filesize = std::filesystem::file_size(filename);
            results[filename] = {0, 0, (long) filesize};
        }

        std::atomic<size_t> frame_index{0};
        auto frame_worker = [&, threads]() {
            exrprofile::ThreadPool pool(threads);
            while (true) {
                using namespace exrprofile;
                const size_t frame = frame_index.fetch_add(1);
                if (frame >= files.size()) break;
                const auto &filename = files[frame];
                const auto result = multithreaded_read(filename, threads, pool);
                results[filename][Records::decompression] = result;
            }
        };


        const auto start_reading = clock::now();
        for (int i = 0; i < workers; ++i) {
            frame_threads.emplace_back(frame_worker);
        }

        for (auto &t: frame_threads) {
            t.join();
        }

        const auto read_time = timeit(start_reading);


        if (verbose) {
            std::vector<std::pair<std::string, exrprofile::Stats>> sorted_results(results.begin(), results.end());
            using namespace exrprofile;
            std::cout << "\nSorted by Reading Time:\n";
            std::sort(sorted_results.begin(), sorted_results.end(),
                      [](const auto &a, const auto &b) {
                          return a.second[Records::decompression] < b.second[Records::decompression];
                      });
            for (const auto &[name, stat]: sorted_results) {
                fmt::print("{:>25}: {} ms -> size: {:.2f}MB \n", name, stat[Records::decompression],
                           (double) stat[Records::filesize] / (1024 * 1024));
            }
        }

        auto readings = std::vector<long>();
        for (const auto &[read, stat]: results) {
            readings.push_back(stat[exrprofile::Records::decompression]);
        }

        std::cout << exrprofile::StatsSummary<long>::compute(readings, true);
        fmt::print("Total time: {:.6f} seconds (avg. {} ms per frame)\n",
                   (double) read_time / 1024, read_time / files.size());
        return 0; // NOTE: We quit here
    }

    const int width = std::clamp(scale, 1, 32) * 1024;
    const int height = width;

    // Generate random channel data
    fmt::print("=== Generating random data: {}x{} with {} ===\n", width, height, threads);

    auto compression_list = std::vector<int>(Imf::Compression::NUM_COMPRESSION_METHODS);
    std::iota(compression_list.begin(), compression_list.end(), 0);
    auto compression_name = std::string{};


    auto results = exrprofile::Results{};
    const auto start_gen = clock::now();
    const std::vector<Imf::Rgba> pixels = exrprofile::generate_synthetic_pixels(width, height);
    const auto end_gen = timeit(start_gen);
    fmt::print("{:>15}: {:.6f} seconds\n", "making pixels", (double) end_gen / 1024);

    std::cout << "=== Profiling compressions" << " ===" << std::endl;
    for (const auto compression: compression_list) {

        getCompressionNameFromId(static_cast<Imf::Compression>(compression), compression_name);
        const auto filename = prefix + compression_name + std::string{".exr"};
        auto compression_description = std::string{};
        getCompressionDescriptionFromId(static_cast<Imf::Compression>(compression), compression_description);

        // Measure compression time
        const auto start_compress = clock::now();
        exrprofile::save_exr_file(pixels, filename, width, height, static_cast<Imf::Compression>(compression), threads);
        const auto compression_time = timeit(start_compress);

        const std::uintmax_t filesize = std::filesystem::file_size(filename);
        fmt::print("{} -> {}\n", filename, compression_description);
        fmt::print("{:>15}: {:.6f} seconds\n", "compression", (double) compression_time / 1024);

        // Measure decompression time
        const auto start_decompress = clock::now();
        exrprofile::load_exr_file(filename);
        const auto decompression_time = timeit(start_decompress);
        fmt::print("{:>15}: {:.6f} seconds\n", "decompression", (double) decompression_time / 1024);

        // Store stats
        results[compression_name] = {(long) compression_time, (long) decompression_time, (long) filesize};

        // Optionally cleanup our mess
        if (cleanup)
            exrprofile::delete_test_file(filename);

    }

    exrprofile::print_sorted_stats(results);

    return 0;
}




