#include <gtest/gtest.h>
#include "euxis/network/topology_grid.hpp"

namespace euxis::network {
namespace {

TEST(TopologyGridTest, RegisterAndFind) {
    TopologyGrid grid;
    NodeInfo n1{.id = "node-1", .role = "worker", .status = "online", .capabilities = {"compute", "storage"}};
    NodeInfo n2{.id = "node-2", .role = "worker", .status = "online", .capabilities = {"compute"}};
    
    grid.register_node(n1);
    grid.register_node(n2);
    
    auto compute_nodes = grid.find_nodes_by_capability("compute");
    EXPECT_EQ(compute_nodes.size(), 2u);
    
    auto storage_nodes = grid.find_nodes_by_capability("storage");
    EXPECT_EQ(storage_nodes.size(), 1u);
    EXPECT_EQ(storage_nodes[0].id, "node-1");
}

TEST(TopologyGridTest, Unregister) {
    TopologyGrid grid;
    NodeInfo n1{.id = "node-1", .capabilities = {"compute"}};
    grid.register_node(n1);
    
    EXPECT_EQ(grid.find_nodes_by_capability("compute").size(), 1u);
    
    grid.unregister_node("node-1");
    EXPECT_EQ(grid.find_nodes_by_capability("compute").size(), 0u);
}

TEST(TopologyGridTest, ReRegisterUpdatesIndex) {
    TopologyGrid grid;
    NodeInfo n1{.id = "node-1", .capabilities = {"compute"}};
    grid.register_node(n1);
    
    NodeInfo n1_updated{.id = "node-1", .capabilities = {"storage"}};
    grid.register_node(n1_updated);
    
    EXPECT_EQ(grid.find_nodes_by_capability("compute").size(), 0u);
    EXPECT_EQ(grid.find_nodes_by_capability("storage").size(), 1u);
}

TEST(TopologyGridTest, GetActiveNodes) {
    TopologyGrid grid;
    grid.register_node({.id = "n1", .status = "online"});
    grid.register_node({.id = "n2", .status = "offline"});
    
    auto active = grid.get_active_nodes();
    EXPECT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].id, "n1");
}

} // namespace
} // namespace euxis::network
