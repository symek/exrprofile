//
// Created by symek on 3/31/25.
//
#pragma once
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <optional>

namespace exrprofile {

    template<typename It>
    constexpr decltype(auto) deref(It it) noexcept(noexcept(*it)) {
        return *it;
    }

    template<typename T>
    struct StatsSummary {
        size_t count = 0;
        T min = {};
        T max = {};
        double mean = 0.0;
        double stdev = 0.0;
        std::optional<T> median = std::nullopt;

        static StatsSummary compute(const std::vector<T> &data, bool compute_median = false) {
            StatsSummary result;
            if (data.empty()) return result;

            result.count = data.size();
            result.min = deref(std::min_element(data.begin(), data.end()));
            result.max = deref(std::max_element(data.begin(), data.end()));

            double sum = std::accumulate(data.begin(), data.end(), 0.0);
            result.mean = sum / result.count;

            // compute standard deviation
            double variance = std::accumulate(
                    data.begin(), data.end(), 0.0,
                    [&](double acc, T x) {
                        double diff = x - result.mean;
                        return acc + diff * diff;
                    }) / result.count;

            result.stdev = std::sqrt(variance);

            if (compute_median) {
                std::vector<T> sorted = data;
                std::sort(sorted.begin(), sorted.end());
                if (result.count % 2 == 0)
                    result.median = (sorted[result.count / 2 - 1] + sorted[result.count / 2]) / static_cast<T>(2);
                else
                    result.median = sorted[result.count / 2];
            }

            return result;
        }
        //  Stream output operator using fmt
       static std::string header()  {
            return fmt::format("Count (files) -- Min -- Max -- Mean -- Stdev -- Median\n");
        }
    friend std::ostream& operator<<(std::ostream& os, const StatsSummary& s) {
        os << fmt::format("Files: {} | Min: {}ms | Max: {}ms | Mean: {:.4f}ms | Stdev: {:.4f}ms | Median: {}ms\n", s.count,  s.min, s.max, s.mean, s.stdev, *s.median);
        return os;
    }
    };

} // end of namespace exrprofile

