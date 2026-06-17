#pragma once

/**
 * @file contracts.hpp
 * @brief Umbrella header for all EUXIS Core public interfaces.
 * 
 * Including this file provides access to the complete Agentic Development Environment
 * core SDK, including swarm orchestration, FinOps routing, and resilience patterns.
 */

#include "router.hpp"
#include "swarm.hpp"
#include "types.hpp"

// Resilience primitives live in libs/network; re-exported here so callers can
// use the umbrella header without a second include.
#include <euxis/network/resilience.hpp>
