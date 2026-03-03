#pragma once

// Model module unified header file
// Contains definitions of all core data structures

// Type definitions
#include "types.hpp"

// Core data structures
#include "task.hpp"
#include "context.hpp"
#include "result.hpp"
#include "state.hpp"

// Agent configuration
#include "agent_config.hpp"

// State machine management
#include "task_state_machine.hpp"

/**
 * @namespace moltcat::model
 * @brief MoltCat Model Module - Core Data Structure Layer
 *
 * This module defines all core data structures of the system:
 * - Task: Task definition and lifecycle management
 * - Context: Execution context and shared state
 * - Result: Execution result and performance metrics
 * - StateSnapshot: System state snapshot
 * - TaskStateMachine: Task state machine (unified management of state transitions and retry)
 * - AgentConfig: Agent static configuration (model, API, resource limits, etc.)
 * - AgentMetadata: Agent metadata (version, tags, statistics, etc.)
 *
 * Design principles:
 * - Lightweight: Reduce memory copying
 * - Type-safe: Use strong types to avoid errors
 * - Serializable: Support JSON serialization
 * - Extensible: Reserved extension fields
 * - Thread-safe: All state transitions go through state machine, ensuring consistency
 *
 * Configuration management:
 * - Static configuration (AgentConfig) and metadata (AgentMetadata) defined in Model module
 * - Dynamic state (AgentRuntimeState) managed in Agent module
 * - Follow the responsibility separation principle of "Model manages data, Agent manages behavior"
 */
