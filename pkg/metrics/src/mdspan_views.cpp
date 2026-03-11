#include "euxis/metrics/mdspan_views.hpp"

#include <stdexcept>

namespace euxis::metrics {

AgentMetricsGrid::AgentMetricsGrid(size_t num_agents, size_t num_timesteps)
    : agents_(num_agents)
    , timesteps_(num_timesteps)
    , data_(num_agents * num_timesteps, 0.0)
{
}

void AgentMetricsGrid::set(size_t agent, size_t timestep, double value) {
    if (agent >= agents_ || timestep >= timesteps_)
        throw std::out_of_range("AgentMetricsGrid::set out of bounds");
    data_[agent * timesteps_ + timestep] = value;
}

auto AgentMetricsGrid::get(size_t agent, size_t timestep) const -> double {
    if (agent >= agents_ || timestep >= timesteps_)
        throw std::out_of_range("AgentMetricsGrid::get out of bounds");
    return data_[agent * timesteps_ + timestep];
}

auto AgentMetricsGrid::view() -> mdspan_2d<double> {
    return mdspan_2d<double>(data_.data(), agents_, timesteps_);
}

auto AgentMetricsGrid::view() const -> mdspan_2d<const double> {
    return mdspan_2d<const double>(data_.data(), agents_, timesteps_);
}

auto AgentMetricsGrid::agent_slice(size_t agent) const -> std::span<const double> {
    if (agent >= agents_)
        throw std::out_of_range("AgentMetricsGrid::agent_slice out of bounds");
    return std::span<const double>(data_.data() + agent * timesteps_, timesteps_);
}

auto AgentMetricsGrid::row_means() const -> std::vector<double> {
    std::vector<double> means(agents_);
    auto v = view();
    for (size_t a = 0; a < agents_; ++a) {
        double sum = 0.0;
        for (size_t t = 0; t < timesteps_; ++t)
            sum += v[a, t];
        means[a] = timesteps_ > 0 ? sum / static_cast<double>(timesteps_) : 0.0;
    }
    return means;
}

auto AgentMetricsGrid::col_means() const -> std::vector<double> {
    std::vector<double> means(timesteps_);
    auto v = view();
    for (size_t t = 0; t < timesteps_; ++t) {
        double sum = 0.0;
        for (size_t a = 0; a < agents_; ++a)
            sum += v[a, t];
        means[t] = agents_ > 0 ? sum / static_cast<double>(agents_) : 0.0;
    }
    return means;
}

} // namespace euxis::metrics
