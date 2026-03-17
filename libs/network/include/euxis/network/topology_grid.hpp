#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace euxis::network {

struct NodeInfo {
    std::string id;
    std::string role;
    std::string status;
    std::vector<std::string> capabilities;
};

/**
 * @brief High-performance topology grid with C++23 multidimensional indexing.
 */
class TopologyGrid {
public:
    explicit TopologyGrid(size_t size = 0) : size_(size), data_(size * size, 0.0) {}

    // THE MODERN WAY (C++23): Multidimensional Subscript Operator
    [[nodiscard]] double& operator[](size_t i, size_t j) {
        if (i >= size_ || j >= size_) [[unlikely]] {
            throw std::out_of_range("Grid index out of bounds");
        }
        return data_[i * size_ + j];
    }

    [[nodiscard]] double operator[](size_t i, size_t j) const {
        if (i >= size_ || j >= size_) [[unlikely]] {
            throw std::out_of_range("Grid index out of bounds");
        }
        return data_[i * size_ + j];
    }

    void register_node(const NodeInfo& node);
    void unregister_node(const std::string& node_id);
    std::vector<NodeInfo> get_active_nodes() const;
    std::vector<NodeInfo> find_nodes_by_capability(const std::string& capability) const;

    [[nodiscard]] size_t size() const noexcept { return size_; }

private:
    size_t size_;
    std::vector<double> data_; // Flat array optimized for cache locality
    std::map<std::string, NodeInfo> nodes_;
    std::map<std::string, std::vector<std::string>> capability_to_node_ids_;
};

} // namespace euxis::network
