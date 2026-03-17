/// @file
/// @brief Multi-dimensional views for performance metric analysis.
#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace euxis::metrics {

/// @brief Minimal implementation of a 2D multi-dimensional span.
template <typename T>
class mdspan_2d {
public:
    /// @brief Construct view over raw pointer.
    mdspan_2d(T* data, size_t rows, size_t cols)
        : data_(data), rows_(rows), cols_(cols) {}

    /// @brief Access element at row/column.
    T& operator[](size_t r, size_t c) { return data_[r * cols_ + c]; }
    
    /// @brief Access element at row/column (const).
    const T& operator[](size_t r, size_t c) const { return data_[r * cols_ + c]; }

private:
    T* data_;
    size_t rows_;
    size_t cols_;
};

/// @brief Grid for storing and slicing time-series metrics across multiple agents.
class AgentMetricsGrid {
public:
    /// @brief Construct grid with fixed dimensions.
    AgentMetricsGrid(size_t num_agents, size_t num_timesteps);

    /// @brief Update value at agent/timestep.
    void set(size_t agent, size_t timestep, double value);
    
    /// @brief Get value at agent/timestep.
    auto get(size_t agent, size_t timestep) const -> double;

    /// @brief Get a 2D view of the grid.
    auto view() -> mdspan_2d<double>;
    
    /// @brief Get a 2D view of the grid (const).
    auto view() const -> mdspan_2d<const double>;

    /// @brief Get a 1D span of all timesteps for a single agent.
    auto agent_slice(size_t agent) const -> std::span<const double>;

    /// @brief compute mean values per agent.
    auto row_means() const -> std::vector<double>;
    
    /// @brief compute mean values per timestep.
    auto col_means() const -> std::vector<double>;

    [[nodiscard]] auto num_agents() const -> size_t { return agents_; }
    [[nodiscard]] auto num_timesteps() const -> size_t { return timesteps_; }

private:
    size_t agents_;
    size_t timesteps_;
    std::vector<double> data_;
};

} // namespace euxis::metrics
