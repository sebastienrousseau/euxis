#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace euxis::bench {

/// Lightweight 2D non-owning view (row-major), equivalent to std::mdspan<T, dextents<2>>.
template <typename T>
class mdspan_2d {
public:
    mdspan_2d(T* data, size_t rows, size_t cols)
        : data_(data), rows_(rows), cols_(cols) {}

    T& operator[](size_t r, size_t c) { return data_[r * cols_ + c]; }
    const T& operator[](size_t r, size_t c) const { return data_[r * cols_ + c]; }

    [[nodiscard]] auto rows() const -> size_t { return rows_; }
    [[nodiscard]] auto cols() const -> size_t { return cols_; }

private:
    T* data_;
    size_t rows_;
    size_t cols_;
};

/// 2D benchmark result matrix: iterations x metrics, row-major.
/// Uses mdspan_2d for zero-copy, cache-friendly access.
class BenchmarkResultMatrix {
public:
    BenchmarkResultMatrix(size_t iterations, std::vector<std::string> metric_names);

    void record(size_t iteration, size_t metric_index, double value);

    [[nodiscard]] auto view() -> mdspan_2d<double>;
    [[nodiscard]] auto view() const -> mdspan_2d<const double>;

    [[nodiscard]] auto column_mean(size_t metric_index) const -> double;
    [[nodiscard]] auto column_stddev(size_t metric_index) const -> double;
    [[nodiscard]] auto column_percentile(size_t metric_index, int pct) const -> double;

    [[nodiscard]] auto metric_names() const -> const std::vector<std::string>&;
    [[nodiscard]] auto iterations() const -> size_t { return iterations_; }
    [[nodiscard]] auto num_metrics() const -> size_t { return metric_names_.size(); }

private:
    size_t iterations_;
    std::vector<std::string> metric_names_;
    std::vector<double> data_;
};

} // namespace euxis::bench
