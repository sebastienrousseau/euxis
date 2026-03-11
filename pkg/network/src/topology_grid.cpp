#include <euxis/network/topology_grid.hpp>
#include <algorithm>

namespace euxis::network {

void TopologyGrid::register_node(const NodeInfo& node) {
    // If node already exists, unregister first to clean up the index
    if (nodes_.contains(node.id)) {
        unregister_node(node.id);
    }

    nodes_[node.id] = node;
    for (const auto& cap : node.capabilities) {
        capability_to_node_ids_[cap].push_back(node.id);
    }
}

void TopologyGrid::unregister_node(const std::string& node_id) {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        // Remove from inverted index
        for (const auto& cap : it->second.capabilities) {
            auto& ids = capability_to_node_ids_[cap];
            ids.erase(std::remove(ids.begin(), ids.end(), node_id), ids.end());
            if (ids.empty()) {
                capability_to_node_ids_.erase(cap);
            }
        }
        nodes_.erase(it);
    }
}

std::vector<NodeInfo> TopologyGrid::get_active_nodes() const {
    std::vector<NodeInfo> active;
    for (const auto& [id, node] : nodes_) {
        if (node.status == "online") active.push_back(node);
    }
    return active;
}

std::vector<NodeInfo> TopologyGrid::find_nodes_by_capability(const std::string& capability) const {
    std::vector<NodeInfo> result;
    auto it = capability_to_node_ids_.find(capability);
    if (it != capability_to_node_ids_.end()) {
        for (const auto& id : it->second) {
            result.push_back(nodes_.at(id));
        }
    }
    return result;
}

} // namespace euxis::network
