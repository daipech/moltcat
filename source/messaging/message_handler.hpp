#pragma once

#include "message.hpp"
#include <optional>

namespace moltcat::messaging {

/**
 * @brief Message handler interface
 *
 * All custom message handlers must implement this interface
 * Used to handle messages routed through MessageBus
 */
class IMessageHandler {
public:
    virtual ~IMessageHandler() = default;

    /**
     * @brief Handle message
     *
     * @param msg Received message
     * @return Optional response message (return nullopt indicates no response)
     *
     * @note This method will be called by MessageBus worker threads
     * @note Should return quickly to avoid blocking message processing
     */
    virtual auto handle_message(const Message& msg) -> std::optional<Message> = 0;

    /**
     * @brief Check if specific message type can be handled
     *
     * @param type Message type
     * @return true indicates this message type can be handled
     *
     * @note Can be used for message filtering to improve efficiency
     */
    virtual auto can_handle(MessageType type) const -> bool = 0;
};

/**
 * @brief Message handler base class (provides default implementation)
 *
 * Provides default implementations for some convenience methods
 */
class MessageHandlerBase : public IMessageHandler {
public:
    ~MessageHandlerBase() override = default;

    /**
     * @brief Default implementation: handle all message types
     */
    auto can_handle(MessageType type) const -> bool override {
        (void)type;  // Unused parameter
        return true;
    }
};

} // namespace moltcat::messaging
