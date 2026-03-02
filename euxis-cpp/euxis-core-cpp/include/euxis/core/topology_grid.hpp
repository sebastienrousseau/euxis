#pragma once

#include <cstddef>
#include <vector>

namespace euxis::core {

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

/// NxN agent adjacency/cost matrix with zero-copy mdspan view.
/// Models swarm topology with weighted edges.
class TopologyGrid {
public:
    explicit TopologyGrid(size_t num_agents);

    void set_cost(size_t from, size_t to, double cost);
    [[nodiscard]] auto cost(size_t from, size_t to) const -> double;

    [[nodiscard]] auto view() -> mdspan_2d<double>;
    [[nodiscard]] auto view() const -> mdspan_2d<const double>;

    /// Find the agent with the lowest total outgoing cost (best coordinator).
    [[nodiscard]] auto best_coordinator() const -> size_t;

    /// Check if agent `to` is reachable from agent `from` (cost > 0).
    [[nodiscard]] auto reachable(size_t from, size_t to) const -> bool;

    [[nodiscard]] auto num_agents() const -> size_t { return n_; }

private:
    size_t n_;
    std::vector<double> data_;
};

} // namespace euxis::core
