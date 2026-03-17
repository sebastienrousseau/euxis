# Euxis Core C++

The `euxis::core` module orchestrates the execution, routing, and supervision of the Agent OS. This is the hardware-native heartbeat of the Euxis ecosystem.

## SIMD Branchless Routing

The `FinOpsRouter` determines the optimal AI provider for an agent's task. 

* **Precondition**: A task requires routing based on complexity, speed, or cost priority.
* **Postcondition**: Returns the optimal provider string in $O(1)$ time.

For high-throughput evaluation, use the Structure-of-Arrays (SoA) layout. The router executes branchless SIMD auto-vectorized loops to score 16+ providers simultaneously.

## Autonomous Supervision

The `SupervisorAgent` daemon actively monitors the system's `CircuitBreaker`.

* **RAII**: Resource-bound lifetime management. 

If the supervisor detects latency drift or threshold breaches, it dynamically tunes the LRU `session_limit_` to protect VRAM and prevent cascading failures.

## Memory & Safety

The Core module strictly enforces memory safety. It avoids fragmented heap allocations, utilizing contiguous `std::vector` and `std::deque` containers where appropriate. 

**All architectural updates must align with C++23 standards.**