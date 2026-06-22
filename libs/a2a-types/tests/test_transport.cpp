/// @file
/// @brief Tests for the A2A transport interface and error taxonomy.

#include <gtest/gtest.h>

#include <expected>
#include <memory>
#include <string>

#include "euxis/a2a/transport.hpp"

// NOLINTBEGIN(bugprone-unused-return-value) — gtest-style discards on
// EXPECT_*/ASSERT_* helpers are intentional; tests can blanket-disable
// per docs/development/clang-tidy-policy.md.

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// TransportError enum
// ---------------------------------------------------------------------------

TEST(TransportErrorTest, EnumeratesFourDistinctValues) {
    EXPECT_NE(TransportError::ConnectionFailed, TransportError::Timeout);
    EXPECT_NE(TransportError::Timeout,          TransportError::ProtocolError);
    EXPECT_NE(TransportError::ProtocolError,    TransportError::AuthFailed);
    EXPECT_NE(TransportError::AuthFailed,       TransportError::ConnectionFailed);
}

TEST(TransportErrorTest, CanBeCarriedByStdExpected) {
    // The whole point of the enum is to ride in std::expected — verify the
    // shape compiles and round-trips through expected's value/error API.
    std::expected<int, TransportError> ok{42};
    std::expected<int, TransportError> err{std::unexpected{TransportError::Timeout}};

    EXPECT_TRUE(ok.has_value());
    EXPECT_EQ(*ok, 42);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), TransportError::Timeout);
}

// ---------------------------------------------------------------------------
// ITransport — interface contract verified with an in-process mock.
// ---------------------------------------------------------------------------

namespace {

/// Minimal mock that echoes the request as the response.
class EchoTransport final : public ITransport {
public:
    int send_count{0};

    auto send(const A2AMessage& msg)
        -> std::expected<A2AMessage, TransportError> override {
        ++send_count;
        return msg;
    }
};

/// Mock that returns a configured failure for every send.
class FailingTransport final : public ITransport {
public:
    TransportError reason{TransportError::ConnectionFailed};

    auto send(const A2AMessage& /*msg*/)
        -> std::expected<A2AMessage, TransportError> override {
        return std::unexpected{reason};
    }
};

auto make_text_message(std::string role, std::string content) -> A2AMessage {
    A2AMessage m;
    m.role = std::move(role);
    m.created_at = "2026-05-30T00:00:00Z";
    ContentPart p;
    p.type = "text";
    p.content = std::move(content);
    m.parts.push_back(std::move(p));
    return m;
}

} // namespace

TEST(ITransportTest, MockEchoReturnsMessage) {
    EchoTransport t;
    auto reply = t.send(make_text_message("user", "ping"));
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->role, "user");
    ASSERT_EQ(reply->parts.size(), 1u);
    EXPECT_EQ(reply->parts[0].content, "ping");
    EXPECT_EQ(t.send_count, 1);
}

TEST(ITransportTest, MockEchoCountsSends) {
    EchoTransport t;
    for (int i = 0; i < 5; ++i) {
        (void)t.send(make_text_message("user", "msg" + std::to_string(i)));
    }
    EXPECT_EQ(t.send_count, 5);
}

TEST(ITransportTest, FailingTransportSurfacesReason) {
    FailingTransport t;
    t.reason = TransportError::AuthFailed;
    auto reply = t.send(make_text_message("user", "auth me"));
    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error(), TransportError::AuthFailed);
}

TEST(ITransportTest, FailingTransportCanSwitchReasonBetweenSends) {
    FailingTransport t;

    t.reason = TransportError::Timeout;
    EXPECT_EQ(t.send(make_text_message("u", "a")).error(),
              TransportError::Timeout);

    t.reason = TransportError::ProtocolError;
    EXPECT_EQ(t.send(make_text_message("u", "b")).error(),
              TransportError::ProtocolError);
}

TEST(ITransportTest, PolymorphicUseViaUniquePtr) {
    // Verify ITransport polymorphism via std::unique_ptr — the typical way
    // callers will hold an injected transport.
    std::unique_ptr<ITransport> t = std::make_unique<EchoTransport>();
    auto reply = t->send(make_text_message("agent", "hello"));
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->role, "agent");
}

TEST(ITransportTest, DestructorIsVirtual) {
    // Without a virtual destructor, deleting an EchoTransport through an
    // ITransport pointer would slice the derived destructor. This test
    // proves the contract — if it ever regresses, ASan in debug + UBSan
    // will trip on the next test run.
    ITransport* t = new EchoTransport{};
    delete t;  // must invoke EchoTransport::~EchoTransport
    SUCCEED();
}

} // namespace
} // namespace euxis::a2a

// NOLINTEND(bugprone-unused-return-value)
