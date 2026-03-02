#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace euxis::metrics {

/// Lightweight 2D non-owning view (row-major), equivalent to std::mdspan<T, dextents<2>>.
/// Used because GCC 15 on this system does not ship <mdspan>.
template <typename T>
class mdspan_2d {
public:
    mdspan_2d(T* data, size_t rows, size_t cols)
        : data_(data), rows_(rows), cols_(cols) {}

    T& operator[](size_t r, size_t c) { return data_[r * cols_ + c]; }
    const T& operator[](size_t r, size_t c) const { return data_[r * cols_ + c]; }

    [[nodiscard]] auto rows() const -> size_t { return rows_; }
    [[nodiscard]] auto cols() const -> size_t { return cols_; }
    [[nodiscard]] auto data() const -> T* { return data_; }

private:
    T* data_;
    size_t rows_;
    size_t cols_;
};

/// 2D agent-by-timestep metrics grid with zero-copy mdspan view.
/// Row-major layout: agents x timesteps.
class AgentMetricsGrid {
public:
    AgentMetricsGrid(size_t num_agents, size_t num_timesteps);

    void set(size_t agent, size_t timestep, double value);
    [[nodiscard]] auto get(size_t agent, size_t timestep) const -> double;

    [[nodiscard]] auto view() -> mdspan_2d<double>;
    [[nodiscard]] auto view() const -> mdspan_2d<const double>;

    [[nodiscard]] auto agent_slice(size_t agent) const -> std::span<const double>;

    [[nodiscard]] auto row_means() const -> std::vector<double>;
    [[nodiscard]] auto col_means() const -> std::vector<double>;

    [[nodiscard]] auto num_agents() const -> size_t { return agents_; }
    [[nodiscard]] auto num_timesteps() const -> size_t { return timesteps_; }

private:
    size_t agents_;
    size_t timesteps_;
    std::vector<double> data_;
};

} // namespace euxis::metrics
