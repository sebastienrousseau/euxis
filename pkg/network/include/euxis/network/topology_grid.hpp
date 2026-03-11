#pragma once

#include <string>
#include <vector>
#include <map>

namespace euxis::network {

struct NodeInfo {
    std::string id;
    std::string role;
    std::string status;
    std::vector<std::string> capabilities;
};

class TopologyGrid {
public:
    void register_node(const NodeInfo& node);
    void unregister_node(const std::string& node_id);
    std::vector<NodeInfo> get_active_nodes() const;
    std::vector<NodeInfo> find_nodes_by_capability(const std::string& capability) const;

private:
    std::map<std::string, NodeInfo> nodes_;
    std::map<std::string, std::vector<std::string>> capability_to_node_ids_;
};

} // namespace euxis::network
