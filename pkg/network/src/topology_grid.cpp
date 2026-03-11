#include <euxis/network/topology_grid.hpp>
#include <algorithm>

namespace euxis::network {

void TopologyGrid::register_node(const NodeInfo& node) {
    if (nodes_.contains(node.id)) [[unlikely]] {
        unregister_node(node.id);
    }

    nodes_[node.id] = node;
    for (const auto& cap : node.capabilities) {
        capability_to_node_ids_[cap].push_back(node.id);
    }
    
    // Auto-resize matrix if needed (simplification for audit)
    if (nodes_.size() > size_) {
        size_t new_size = nodes_.size();
        std::vector<double> new_data(new_size * new_size, 0.0);
        for (size_t i = 0; i < size_; ++i) {
            for (size_t j = 0; j < size_; ++j) {
                new_data[i * new_size + j] = data_[i * size_ + j];
            }
        }
        data_ = std::move(new_data);
        size_ = new_size;
    }
}

void TopologyGrid::unregister_node(const std::string& node_id) {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) [[likely]] {
        for (const auto& cap : it->second.capabilities) {
            auto& ids = capability_to_node_ids_[cap];
            ids.erase(std::remove(ids.begin(), ids.end(), node_id), ids.end());
            if (ids.empty()) capability_to_node_ids_.erase(cap);
        }
        nodes_.erase(it);
    }
}

std::vector<NodeInfo> TopologyGrid::get_active_nodes() const {
    std::vector<NodeInfo> active;
    for (const auto& [id, node] : nodes_) {
        if (node.status == "online") [[likely]] active.push_back(node);
    }
    return active;
}

std::vector<NodeInfo> TopologyGrid::find_nodes_by_capability(const std::string& capability) const {
    std::vector<NodeInfo> result;
    auto it = capability_to_node_ids_.find(capability);
    if (it != capability_to_node_ids_.end()) [[likely]] {
        for (const auto& id : it->second) {
            result.push_back(nodes_.at(id));
        }
    }
    return result;
}

} // namespace euxis::network
