#include "euxis/bench/result_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace euxis::bench {

BenchmarkResultMatrix::BenchmarkResultMatrix(size_t iterations,
                                             std::vector<std::string> metric_names)
    : iterations_(iterations)
    , metric_names_(std::move(metric_names))
    , data_(iterations * metric_names_.size(), 0.0)
{
}

void BenchmarkResultMatrix::record(size_t iteration, size_t metric_index, double value) {
    if (iteration >= iterations_ || metric_index >= metric_names_.size())
        throw std::out_of_range("BenchmarkResultMatrix::record out of bounds");
    data_[iteration * metric_names_.size() + metric_index] = value;
}

auto BenchmarkResultMatrix::view() -> mdspan_2d<double> {
    return mdspan_2d<double>(data_.data(), iterations_, metric_names_.size());
}

auto BenchmarkResultMatrix::view() const -> mdspan_2d<const double> {
    return mdspan_2d<const double>(data_.data(), iterations_, metric_names_.size());
}

auto BenchmarkResultMatrix::column_mean(size_t metric_index) const -> double {
    if (metric_index >= metric_names_.size())
        throw std::out_of_range("BenchmarkResultMatrix::column_mean out of bounds");
    if (iterations_ == 0) return 0.0;

    auto v = view();
    double sum = 0.0;
    for (size_t i = 0; i < iterations_; ++i)
        sum += v[i, metric_index];
    return sum / static_cast<double>(iterations_);
}

auto BenchmarkResultMatrix::column_stddev(size_t metric_index) const -> double {
    if (metric_index >= metric_names_.size())
        throw std::out_of_range("BenchmarkResultMatrix::column_stddev out of bounds");
    if (iterations_ <= 1) return 0.0;

    double mean = column_mean(metric_index);
    auto v = view();
    double sum_sq = 0.0;
    for (size_t i = 0; i < iterations_; ++i) {
        double diff = v[i, metric_index] - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(iterations_ - 1));
}

auto BenchmarkResultMatrix::column_percentile(size_t metric_index, int pct) const -> double {
    if (metric_index >= metric_names_.size())
        throw std::out_of_range("BenchmarkResultMatrix::column_percentile out of bounds");
    if (iterations_ == 0) return 0.0;

    std::vector<double> col(iterations_);
    auto v = view();
    for (size_t i = 0; i < iterations_; ++i)
        col[i] = v[i, metric_index];

    std::ranges::sort(col);
    double rank = static_cast<double>(pct) / 100.0 * static_cast<double>(iterations_ - 1);
    auto lower = static_cast<size_t>(rank);
    double frac = rank - static_cast<double>(lower);
    if (lower + 1 >= iterations_) return col.back();
    return col[lower] + frac * (col[lower + 1] - col[lower]);
}

auto BenchmarkResultMatrix::metric_names() const -> const std::vector<std::string>& {
    return metric_names_;
}

} // namespace euxis::bench
