#pragma once

#include <cstddef>
#include <vector>

namespace euxis::core {

/**
 * @brief Lightweight 2D non-owning view (row-major).
 * 
 * Equivalent to a subset of std::mdspan<T, dextents<2>>.
 * @tparam T The element type stored in the grid.
 */
template <typename T>
class MdSpan2D {
public:
    /**
     * @brief Create a view over raw data.
     * @param data Pointer to the start of the contiguous 2D data.
     * @param rows Number of rows.
     * @param cols Number of columns.
     */
    MdSpan2D(T* data, size_t rows, size_t cols) // NOLINT
        : data_(data), rows_(rows), cols_(cols) {}

    /** @brief Access element at (row, column). */
    T& operator[](size_t r, size_t c) { return data_[(r * cols_) + c]; } // NOLINT
    
    /** @brief Access element at (row, column) (const). */
    const T& operator[](size_t r, size_t c) const { return data_[(r * cols_) + c]; } // NOLINT

    /** @brief Get the number of rows in the view. */
    [[nodiscard]] auto rows() const -> size_t { return rows_; }
    
    /** @brief Get the number of columns in the view. */
    [[nodiscard]] auto cols() const -> size_t { return cols_; }

private:
    T* data_;
    size_t rows_;
    size_t cols_;
};

/**
 * @brief Represents an NxN agent adjacency and cost matrix.
 * 
 * Models the topology of a swarm, where edges between agents represent communication
 * costs or physical distance.
 */
class TopologyGrid {
public:
    /**
     * @brief Construct a new square Topology Grid.
     * @param num_agents The number of agents (N) in the square matrix.
     */
    explicit TopologyGrid(size_t num_agents);

    /**
     * @brief Define the weighted edge between two agents.
     * @param from Source agent index.
     * @param to Destination agent index.
     * @param cost Weight/cost of the connection.
     */
    void set_cost(size_t from, size_t to, double cost);
    
    /** @brief Get the edge weight between two agents. */
    [[nodiscard]] auto cost(size_t from, size_t to) const -> double;

    /** @brief Returns a zero-copy 2D view of the grid. */
    [[nodiscard]] auto view() -> MdSpan2D<double>;
    
    /** @brief Returns a zero-copy 2D view of the grid (const). */
    [[nodiscard]] auto view() const -> MdSpan2D<const double>;

    /**
     * @brief Identify the agent with the lowest aggregate outgoing edge cost.
     * @return size_t Index of the best coordinator agent.
     */
    [[nodiscard]] auto best_coordinator() const -> size_t;

    /** @brief Returns true if an edge with cost > 0 exists between from and to. */
    [[nodiscard]] auto reachable(size_t from, size_t to) const -> bool;

    /** @brief Get the dimension (N) of the grid. */
    [[nodiscard]] auto num_agents() const -> size_t { return n_; }

private:
    size_t n_;
    std::vector<double> data_;
};

} // namespace euxis::core
