#include "euxis/core/topology_grid.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace euxis::core {

TopologyGrid::TopologyGrid(size_t num_agents)
    : n_(num_agents)
    , data_(num_agents * num_agents, 0.0)
{
}

void TopologyGrid::set_cost(size_t from, size_t to, double cost) {
    if (from >= n_ || to >= n_) {
        throw std::out_of_range("TopologyGrid::set_cost out of bounds");
    }
    data_[(from * n_) + to] = cost;
}

auto TopologyGrid::cost(size_t from, size_t to) const -> double {
    if (from >= n_ || to >= n_) {
        throw std::out_of_range("TopologyGrid::cost out of bounds");
    }
    return data_[(from * n_) + to];
}

auto TopologyGrid::view() -> MdSpan2D<double> {
    return {data_.data(), n_, n_};
}

auto TopologyGrid::view() const -> MdSpan2D<const double> {
    return {data_.data(), n_, n_};
}

auto TopologyGrid::best_coordinator() const -> size_t {
    if (n_ == 0) {
        throw std::logic_error("TopologyGrid::best_coordinator on empty grid");
    }

    const auto v = view();
    size_t best = 0;
    double best_total = std::numeric_limits<double>::max();

    for (size_t from = 0; from < n_; ++from) {
        double total = 0.0;
        for (size_t to = 0; to < n_; ++to) {
            total += v[from, to];
        }
        if (total < best_total) {
            best_total = total;
            best = from;
        }
    }
    return best;
}

auto TopologyGrid::reachable(size_t from, size_t to) const -> bool {
    if (from >= n_ || to >= n_) {
        throw std::out_of_range("TopologyGrid::reachable out of bounds");
    }
    return data_[(from * n_) + to] > 0.0;
}

} // namespace euxis::core
