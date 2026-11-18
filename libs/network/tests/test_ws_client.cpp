#include <gtest/gtest.h>
#include "euxis/network/ws_client.hpp"

#include <ixwebsocket/IXDNSLookup.h>

#include <memory>
#include <string>

namespace euxis::network {
namespace {

TEST(WebSocketClientTest, BasicConstruction) {
    EXPECT_NO_THROW({
        WebSocketClient client("ws://localhost:9999");
    });
}

TEST(WebSocketClientTest, MethodsNoThrow) {
    WebSocketClient client("ws://localhost:9999");
    EXPECT_NO_THROW(client.connect());
    EXPECT_NO_THROW(client.send(std::string("test")));
}

TEST(WebSocketClientTest, SetOnMessage) {
    WebSocketClient client("ws://localhost:9999");
    EXPECT_NO_THROW({
        client.set_on_message([](const std::string& msg) { (void)msg; });
    });
}

// A failed lookup used to hand freeaddrinfo a null pointer, which musl
// dereferences: the static build segfaulted on any unresolvable host.
// ".invalid" never resolves (RFC 6761), with or without a network.
TEST(DnsLookupTest, UnresolvableHostFailsCleanly) {
    auto lookup = std::make_shared<ix::DNSLookup>("euxis-test.invalid", 443);
    std::string err;
    const auto res = lookup->resolve(err, [] { return false; }, /*cancellable=*/false);
    EXPECT_EQ(res, nullptr);
    EXPECT_FALSE(err.empty());
}

} // namespace
} // namespace euxis::network
