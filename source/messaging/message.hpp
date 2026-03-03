#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <glaze/json.hpp>
#include <utils/uuid.hpp>

namespace moltcat::messaging {

// ================================
// Message Type Enumeration
// ================================
enum class MessageType : uint32_t {
    // Task related
    TASK_SUBMIT = 1,          // Submit task
    TASK_EXECUTE = 2,         // Execute task
    TASK_RESULT = 3,          // Task result
    TASK_CANCEL = 4,          // Cancel task

    // Memory related
    MEMORY_STORE = 10,        // Store memory
    MEMORY_RETRIEVE = 11,     // Retrieve memory
    MEMORY_QUERY = 12,        // Query memory
    MEMORY_RESPONSE = 13,     // Memory response

    // Collaboration related
    COLLABORATE_PIPELINE = 20,     // Pipeline collaboration
    COLLABORATE_PARALLEL = 21,     // Parallel collaboration
    COLLABORATE_COMPETITIVE = 22,  // Competitive collaboration
    COLLABORATE_HIERARCHICAL = 23, // Hierarchical collaboration

    // Service discovery
    SERVICE_REGISTER = 30,    // Service registration
    SERVICE_UNREGISTER = 31,  // Service unregistration
    SERVICE_HEARTBEAT = 32,   // Service heartbeat
    SERVICE_DISCOVER = 33,    // Service discovery
    SERVICE_ANNOUNCE = 34,    // Service announcement

    // System control
    SYSTEM_START = 40,        // Start component
    SYSTEM_STOP = 41,         // Stop component
    SYSTEM_STATUS = 42,       // Status query
    SYSTEM_ERROR = 43,        // Error report

    // Agent discovery (layered communication architecture)
    AGENT_REGISTER = 50,           // Agent registers to system
    AGENT_UNREGISTER = 51,         // Agent unregisters
    TEAM_DISCOVER_MEMBERS = 52,    // Query team members
    TEAM_MEMBER_JOINED = 53,       // Member joined event
    TEAM_MEMBER_LEFT = 54,         // Member left event
    TEAM_MEMBERS_RESPONSE = 55,    // Member list response

    // Agent communication (layered communication architecture)
    AGENT_RPC = 60,                // Cross-team RPC call
    AGENT_MESSAGE = 61,            // Agent message
};

// ================================
// Message Header
// ================================
struct MessageHeader {
    std::string message_id;           // Message unique ID
    MessageType type;                 // Message type
    std::string source;               // Sender ID
    std::string destination;          // Receiver ID (empty means broadcast)
    std::string correlation_id;       // Correlation ID (for request-response matching)
    uint64_t timestamp;               // Timestamp (milliseconds)
    uint32_t priority;                // Priority (0-9, 9 is highest)
    std::unordered_map<std::string, std::string> metadata;  // Extended metadata
};

// Glaze serialization support
template <>
struct glz::meta<MessageHeader> {
    using T = MessageHeader;
    static constexpr auto value = glz::object(
        "message_id", &T::message_id,
        "type", &T::type,
        "source", &T::source,
        "destination", &T::destination,
        "correlation_id", &T::correlation_id,
        "timestamp", &T::timestamp,
        "priority", &T::priority,
        "metadata", &T::metadata
    );
};

// ================================
// Message Body
// ================================
struct MessageBody {
    std::string content;              // JSON-serialized payload content
    std::unordered_map<std::string, std::string> attachments;  // Attachment data
};

template <>
struct glz::meta<MessageBody> {
    using T = MessageBody;
    static constexpr auto value = glz::object(
        "content", &T::content,
        "attachments", &T::attachments
    );
};

// ================================
// Complete Message
// ================================
struct Message {
    MessageHeader header;
    MessageBody body;

    // Utility method: generate new message ID
    static auto generate_message_id() -> std::string {
        return moltcat::utils::UUID::generate_v4();
    }

    // Utility method: get current timestamp
    static auto current_timestamp() -> uint64_t {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
    }

    // Utility method: create base message
    static auto create(MessageType type, std::string_view source) -> Message {
        Message msg;
        msg.header.message_id = generate_message_id();
        msg.header.type = type;
        msg.header.source = source;
        msg.header.timestamp = current_timestamp();
        msg.header.priority = 5;  // Default medium priority
        return msg;
    }

    // Serialize entire message to JSON
    auto serialize() const -> std::string {
        std::string json;
        glz::write_json(msg, json);
        return json;
    }

    // Deserialize from JSON
    static auto deserialize(const std::string& json) -> std::optional<Message> {
        Message msg;
        auto error = glz::read_json(msg, json);
        if (error) {
            return std::nullopt;
        }
        return msg;
    }
};

template <>
struct glz::meta<Message> {
    using T = Message;
    static constexpr auto value = glz::object(
        "header", &T::header,
        "body", &T::body
    );
};

// ================================
// User extensible section: Message priority strategy
// ================================

/**
 * TODO: Implement custom message priority calculation strategy
 *
 * This function defines how to calculate priority based on message content and context.
 * Different application scenarios may have different priority strategies.
 *
 * Implementation hints:
 * 1. Can assign base priority based on message type
 * 2. Can adjust priority based on message size (small messages first)
 * 3. Can adjust priority based on source identity (VIP users first)
 * 4. Can dynamically adjust priority based on system load
 *
 * Examples:
 * - TASK_SUBMIT: priority 7
 * - SYSTEM_ERROR: priority 9
 * - MEMORY_QUERY: priority 5
 * - SERVICE_HEARTBEAT: priority 3
 *
 * @param msg Message to calculate
 * @return Priority value (0-9, 9 is highest)
 */
inline auto calculate_message_priority(const Message& msg) -> uint32_t {
    // TODO: Implement your priority strategy here (5-10 lines of code)
    // This is the key point where you define message routing strategy

    // Default implementation: simple strategy based on message type
    uint32_t base_priority = 5;

    switch (msg.header.type) {
        case MessageType::SYSTEM_ERROR:
            base_priority = 9;  // System errors have highest priority
            break;
        case MessageType::TASK_CANCEL:
            base_priority = 8;  // Cancel operations have high priority
            break;
        case MessageType::TASK_SUBMIT:
            base_priority = 7;  // Task submission has high priority
            break;
        case MessageType::AGENT_REGISTER:
            base_priority = 6;  // Agent registration is important but not urgent
            break;
        case MessageType::AGENT_UNREGISTER:
            base_priority = 5;  // Agent unregistration has medium priority
            break;
        case MessageType::TEAM_DISCOVER_MEMBERS:
            base_priority = 6;  // Member discovery has high priority
            break;
        case MessageType::TEAM_MEMBER_JOINED:
        case MessageType::TEAM_MEMBER_LEFT:
            base_priority = 5;  // Member changes have medium priority
            break;
        case MessageType::AGENT_RPC:
            base_priority = 7;  // RPC calls have high priority
            break;
        case MessageType::SERVICE_HEARTBEAT:
            base_priority = 2;  // Heartbeats have low priority
            break;
        default:
            base_priority = 5;
            break;
    }

    return base_priority;
}

// ================================
// User extensible section: Message validation
// ================================

/**
 * TODO: Implement message validation logic
 *
 * Validate message validity before sending to prevent invalid messages from entering the system.
 *
 * Implementation hints:
 * 1. Check if required fields are complete
 * 2. Validate message ID format
 * 3. Check if message type is supported
 * 4. Verify destination address exists
 *
 * @param msg Message to validate
 * @return true indicates message is valid, false indicates invalid
 */
inline auto validate_message(const Message& msg) -> bool {
    // TODO: Implement your message validation logic here (5-10 lines of code)
    // This is the key point to ensure system robustness

    // Default implementation: basic validation
    if (msg.header.message_id.empty()) return false;
    if (msg.header.source.empty()) return false;
    if (msg.header.timestamp == 0) return false;
    if (msg.header.priority > 9) return false;

    return true;
}

// ================================
// Agent discovery related data structures
// ================================

/**
 * Agent status enumeration
 */
enum class AgentStatus : uint32_t {
    ONLINE = 0,         // Online
    OFFLINE = 1,        // Offline
    BUSY = 2,           // Busy
    SUSPENDED = 3,      // Suspended
    ERROR = 4           // Error state
};

/**
 * Agent information
 */
struct AgentInfo {
    std::string agent_id;
    std::string team_id;
    std::string a2a_address;              // A2A listen address (tcp://192.168.1.10:5571)
    std::vector<std::string> capabilities;  // Agent capability list
    AgentStatus status = AgentStatus::ONLINE;
    uint64_t last_heartbeat = 0;          // Last heartbeat timestamp (milliseconds)
    glz::json_t metadata;                 // Extended metadata
};

template <>
struct glz::meta<AgentInfo> {
    using T = AgentInfo;
    static constexpr auto value = glz::object(
        "agent_id", &T::agent_id,
        "team_id", &T::team_id,
        "a2a_address", &T::a2a_address,
        "capabilities", &T::capabilities,
        "status", &T::status,
        "last_heartbeat", &T::last_heartbeat,
        "metadata", &T::metadata
    );
};

/**
 * Agent registration request
 */
struct AgentRegisterRequest {
    std::string agent_id;
    std::string team_id;                  // Single team (simplified design)
    std::string agent_type;
    std::string a2a_address;              // A2A listen address
    std::vector<std::string> capabilities;
    glz::json_t metadata;

    auto serialize() const -> glz::json_t {
        return glz::json_t{
            {"agent_id", agent_id},
            {"team_id", team_id},
            {"agent_type", agent_type},
            {"a2a_address", a2a_address},
            {"capabilities", capabilities},
            {"metadata", metadata}
        };
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<AgentRegisterRequest> {
        try {
            AgentRegisterRequest req;
            if (json.contains("agent_id")) req.agent_id = json["agent_id"].get<std::string>();
            if (json.contains("team_id")) req.team_id = json["team_id"].get<std::string>();
            if (json.contains("agent_type")) req.agent_type = json["agent_type"].get<std::string>();
            if (json.contains("a2a_address")) req.a2a_address = json["a2a_address"].get<std::string>();
            if (json.contains("capabilities")) req.capabilities = json["capabilities"].get<std::vector<std::string>>();
            if (json.contains("metadata")) req.metadata = json["metadata"];
            return req;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * Team member query response
 */
struct TeamMembersResponse {
    std::string team_id;
    std::vector<AgentInfo> members;

    auto serialize() const -> glz::json_t {
        glz::json_t json;
        json["team_id"] = team_id;
        json["members"] = glz::json_t::array();
        for (const auto& member : members) {
            glz::json_t member_json;
            glz::write_json(member, member_json);  // Use Glaze serialization
            json["members"].push_back(member_json);
        }
        return json;
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<TeamMembersResponse> {
        try {
            TeamMembersResponse resp;
            if (json.contains("team_id")) {
                resp.team_id = json["team_id"].get<std::string>();
            }
            if (json.contains("members")) {
                for (const auto& member_json : json["members"].array()) {
                    AgentInfo info;
                    auto err = glz::read_json(info, member_json);
                    if (!err) {
                        resp.members.push_back(info);
                    }
                }
            }
            return resp;
        } catch (...) {
            return std::nullopt;
        }
    }
};

/**
 * Agent RPC request (cross-team communication)
 */
struct AgentRpcRequest {
    std::string target_agent_id;
    std::string method;
    glz::json_t params;
    std::optional<uint64_t> timeout_ms;

    auto serialize() const -> glz::json_t {
        glz::json_t json;
        json["target_agent_id"] = target_agent_id;
        json["method"] = method;
        json["params"] = params;
        if (timeout_ms) {
            json["timeout_ms"] = *timeout_ms;
        }
        return json;
    }

    static auto deserialize(const glz::json_t& json) -> std::optional<AgentRpcRequest> {
        try {
            AgentRpcRequest req;
            if (json.contains("target_agent_id")) {
                req.target_agent_id = json["target_agent_id"].get<std::string>();
            }
            if (json.contains("method")) {
                req.method = json["method"].get<std::string>();
            }
            if (json.contains("params")) {
                req.params = json["params"];
            }
            if (json.contains("timeout_ms")) {
                req.timeout_ms = json["timeout_ms"].get<uint64_t>();
            }
            return req;
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace moltcat::messaging
