# Euxis A2A C++

The `euxis::a2a` module provides the Agent-to-Agent (A2A) messaging substrate for the Euxis Agent OS. It implements the Auction-Based Agentic Mesh protocols, replacing static hierarchical delegation with decentralized resource bidding.

## Agent-to-Agent Messaging

The A2A protocol enforces standard schema boundaries across all fleet communications.

* **Precondition**: Connecting agents must hold verified ERC-8004 credentials.
* **Postcondition**: Grants duplex message passing over the `WebSocketA2ATransport`.

Use the `A2AMessage` format to encapsulate requests. The `WebSocketA2ATransport` maps asynchronous responses to correlation IDs using `std::mutex` and condition variables, ensuring thread-safe concurrency.

## Decentralized Auction Bidding

The `BidRequest` and `BidResponse` structures implement the decentralized task market.

* **SoA**: Structure of Arrays — Enable SIMD optimization.
* **RAII**: Resource-bound lifetime management.

```cpp
auto res = transport.send(bid_request)
    .and_then([](auto&& response) { return evaluate_bid(response); })
    .or_else([](auto&& err) { return fallback_delegate(err); });
```

Utilize C++23 monadic chains to coordinate fleet auctions. Orchestrator nodes evaluate incoming bids via the SIMD-optimized `FinOpsRouter` to ensure optimal SLM execution.

## Artifact Exchange

Agents pass complex state blocks via base64 encoded payloads in the `Artifact` struct.

* **std::span**: Bounds-checked memory view.

When transmitting large payloads, utilize `std::span` to slice and encode raw binary memory without incurring `std::string` reallocation overhead.