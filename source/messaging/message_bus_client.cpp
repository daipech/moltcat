#include "message_bus_client.hpp"
#include <utils/uuid.hpp>
#include <glaze/json.hpp>
#include <chrono>
#include <stdexcept>

namespace moltcat::messaging {

// ================================
// Constructor
// ================================

MessageBusClient::MessageBusClient(const Config& config)
    : config_(config)
    , connected_(false)
    , context_(config_.io_threads)  // Use configured I/O thread count
    , dealer_socket_(context_, zmq::socket_type::dealer)
    , logger_(spdlog::get("moltcat"))
{
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }

    logger_->set_level(config_.log_level);

    // Auto-generate queue ID if not specified
    if (config_.queue_id.empty()) {
        config_.queue_id = utils::UUID::generate_v4();
        logger_->info("Auto-generated queue ID: {}", config_.queue_id);
    }

    // Set DEALER socket's identity
    dealer_socket_.set(zmq::sockopt::routing_id, zmq::buffer(config_.queue_id));

    // Configure timeouts
    if (config_.recv_timeout_ms >= 0) {
        dealer_socket_.set(zmq::sockopt::rcvtimeo, config_.recv_timeout_ms);
    }
    if (config_.send_timeout_ms >= 0) {
        dealer_socket_.set(zmq::sockopt::sndtimeo, config_.send_timeout_ms);
    }

    // Create SUB socket (if subscription is enabled)
    if (config_.enable_subscription && !config_.server_pub_endpoint.empty()) {
        sub_socket_.emplace(context_, zmq::socket_type::sub);
        logger_->info("SUB socket created");
    }

    logger_->info("MessageBus client created successfully, service: {}, queue: {}",
        config_.service_name, config_.queue_id);
}

// ================================
// Destructor
// ================================

MessageBusClient::~MessageBusClient() {
    disconnect();
}

// ================================
// Connect to server
// ================================

auto MessageBusClient::connect() -> bool {
    if (connected_) {
        logger_->warn("Client already connected");
        return true;
    }

    logger_->info("Connecting to MessageBus server: {}", config_.server_endpoint);

    if (!connect_to_server()) {
        return false;
    }

    // Register service
    if (!register_service()) {
        logger_->error("Service registration failed");
        dealer_socket_.close();
        if (sub_socket_.has_value()) {
            sub_socket_->close();
        }
        return false;
    }

    connected_ = true;
    logger_->info("Client connected and service registered: {}", config_.service_name);

    return true;
}

// ================================
// Disconnect
// ================================

auto MessageBusClient::disconnect() -> void {
    if (!connected_) {
        return;
    }

    logger_->info("Disconnecting...");

    // Stop message loop
    stop_message_loop();

    // Unregister service
    if (connected_) {
        unregister_service();
    }

    // Close sockets
    try {
        dealer_socket_.close();
        if (sub_socket_.has_value()) {
            sub_socket_->close();
        }
    } catch (...) {
        // Ignore errors during close
    }

    connected_ = false;
    logger_->info("Client disconnected");
}

// ================================
// Connect to server (internal implementation)
// ================================

auto MessageBusClient::connect_to_server() -> bool {
    try {
        // Connect DEALER socket to server's ROUTER
        dealer_socket_.connect(config_.server_endpoint);
        logger_->info("DEALER socket connected: {}", config_.server_endpoint);

        // If there's a SUB socket, connect to PUB endpoint
        if (sub_socket_.has_value()) {
            sub_socket_->connect(config_.server_pub_endpoint);
            logger_->info("SUB socket connected: {}", config_.server_pub_endpoint);
        }

        return true;

    } catch (const zmq::error_t& e) {
        logger_->error("ZeroMQ connection error: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        logger_->error("Connection failed: {}", e.what());
        return false;
    }
}

// ================================
// Register service
// ================================

auto MessageBusClient::register_service() -> bool {
    try {
        // Build registration message
        Message msg = Message::create(MessageType::SERVICE_REGISTER, config_.service_name);
        msg.header.destination = "MessageBusServer";

        // Message body contains service name
        glz::json_t json{
            {"service_name", config_.service_name}
        };
        std::string json_content;
        glz::write_json(json, json_content);
        msg.body.content = json_content;

        // Send registration message
        if (!send_internal(msg, config_.connect_timeout_ms)) {
            logger_->error("Failed to send registration message");
            return false;
        }

        // Wait for response
        auto response_opt = receive(config_.connect_timeout_ms);
        if (!response_opt) {
            logger_->error("Registration response timeout");
            return false;
        }

        const Message& response = *response_opt;

        // Check response
        auto response_json = glz::read_json<glz::json_t>(response.body.content);
        if (response_json.contains("status")) {
            std::string status = response_json["status"].get<std::string>();
            if (status == "registered") {
                logger_->info("Service registered successfully: {}", config_.service_name);
                return true;
            }
        }

        if (response_json.contains("error")) {
            std::string error = response_json["error"].get<std::string>();
            logger_->error("Service registration failed: {}", error);
        }

        return false;

    } catch (const std::exception& e) {
        logger_->error("Service registration exception: {}", e.what());
        return false;
    }
}

// ================================
// Unregister service
// ================================

auto MessageBusClient::unregister_service() -> bool {
    try {
        // Build unregistration message
        Message msg = Message::create(MessageType::SERVICE_UNREGISTER, config_.service_name);
        msg.header.destination = "MessageBusServer";

        // Message body contains service name
        glz::json_t json{
            {"service_name", config_.service_name}
        };
        std::string json_content;
        glz::write_json(json, json_content);
        msg.body.content = json_content;

        // Send unregistration message (no need to wait for response)
        send_internal(msg, 1000);

        logger_->info("Service unregistration request sent: {}", config_.service_name);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Service unregistration exception: {}", e.what());
        return false;
    }
}

// ================================
// Send message (auto routing)
// ================================

auto MessageBusClient::send(const Message& msg, int timeout_ms) -> bool {
    if (!connected_) {
        logger_->error("Client not connected");
        return false;
    }

    if (msg.header.destination.empty()) {
        logger_->error("Message has no destination");
        return false;
    }

    return send_internal(msg, timeout_ms);
}

// ================================
// Send to specific service
// ================================

auto MessageBusClient::send_to(
    const std::string& service_name,
    const Message& msg,
    int timeout_ms
) -> bool {
    if (!connected_) {
        logger_->error("Client not connected");
        return false;
    }

    // Copy message and set destination
    Message msg_copy = msg;
    msg_copy.header.destination = service_name;

    return send_internal(msg_copy, timeout_ms);
}

// ================================
// Broadcast message
// ================================

auto MessageBusClient::broadcast(
    const std::string& service_name,
    const Message& msg,
    int timeout_ms
) -> size_t {
    if (!connected_) {
        logger_->error("Client not connected");
        return 0;
    }

    // Copy message and set destination
    Message msg_copy = msg;
    msg_copy.header.destination = service_name;

    // Send (server will broadcast to all instances)
    if (send_internal(msg_copy, timeout_ms)) {
        return 1;  // Successfully sent
    }

    return 0;
}

// ================================
// Subscribe to topic
// ================================

auto MessageBusClient::subscribe(const std::string& topic) -> void {
    if (!sub_socket_.has_value()) {
        logger_->warn("SUB socket not enabled, cannot subscribe to topic: {}", topic);
        return;
    }

    try {
        // Set subscription filter
        sub_socket_->set(zmq::sockopt::subscribe, topic);
        logger_->info("Subscribed to topic: {}", topic);

        // Send subscription message to server (for registration)
        Message msg = Message::create(MessageType::SERVICE_HEARTBEAT, config_.service_name);
        msg.header.destination = "MessageBusServer";

        glz::json_t json{
            {"topic", topic}
        };
        std::string json_content;
        glz::write_json(json, json_content);
        msg.body.content = json_content;

        send_internal(msg, 1000);

    } catch (const std::exception& e) {
        logger_->error("Failed to subscribe to topic: {}", e.what());
    }
}

// ================================
// Unsubscribe
// ================================

auto MessageBusClient::unsubscribe(const std::string& topic) -> void {
    if (!sub_socket_.has_value()) {
        logger_->warn("SUB socket not enabled, cannot unsubscribe from topic: {}", topic);
        return;
    }

    try {
        // Unset subscription filter
        sub_socket_->set(zmq::sockopt::unsubscribe, topic);
        logger_->info("Unsubscribed from topic: {}", topic);

        // Send unsubscribe message to server
        Message msg = Message::create(MessageType::SERVICE_HEARTBEAT, config_.service_name);
        msg.header.destination = "MessageBusServer";

        glz::json_t json{
            {"topic", topic}
        };
        std::string json_content;
        glz::write_json(json, json_content);
        msg.body.content = json_content;

        send_internal(msg, 1000);

    } catch (const std::exception& e) {
        logger_->error("Failed to unsubscribe: {}", e.what());
    }
}

// ================================
// Receive message (synchronous)
// ================================

auto MessageBusClient::receive(int timeout_ms) -> std::optional<Message> {
    if (!connected_) {
        logger_->error("Client not connected");
        return std::nullopt;
    }

    try {
        // Receive message (multi-part: empty, content)
        zmq::message_t empty_msg;
        zmq::message_t content_msg;

        auto result = dealer_socket_.recv(empty_msg);

        if (!result.has_value()) {
            // Timeout
            return std::nullopt;
        }

        result = dealer_socket_.recv(content_msg);
        if (!result.has_value()) {
            logger_->error("Failed to receive message content");
            return std::nullopt;
        }

        // Extract message content
        std::string content(
            static_cast<char*>(content_msg.data()),
            content_msg.size()
        );

        // Deserialize message
        auto msg_opt = Message::deserialize(content);
        if (!msg_opt) {
            logger_->error("Message deserialization failed");
            return std::nullopt;
        }

        logger_->debug("Received message: {}", msg_opt->header.message_id);
        return msg_opt;

    } catch (const std::exception& e) {
        logger_->error("Message receive exception: {}", e.what());
        return std::nullopt;
    }
}

// ================================
// Start message loop (asynchronous receive)
// ================================

auto MessageBusClient::start_message_loop(
    std::function<void(const Message&)> callback
) -> void {
    if (receiving_.exchange(true)) {
        logger_->warn("Message loop already running");
        return;
    }

    message_loop_thread_ = std::thread([this, callback]() {
        this->message_loop(callback);
    });

    logger_->info("Message loop started");
}

// ================================
// Stop message loop
// ================================

auto MessageBusClient::stop_message_loop() -> void {
    if (!receiving_.exchange(false)) {
        return;
    }

    logger_->info("Stopping message loop...");

    if (message_loop_thread_.joinable()) {
        message_loop_thread_.join();
    }

    logger_->info("Message loop stopped");
}

// ================================
// Message loop (internal implementation)
// ================================

auto MessageBusClient::message_loop(
    std::function<void(const Message&)> callback
) -> void {
    logger_->info("Message loop thread started");

    while (receiving_.load()) {
        auto msg_opt = receive(100);  // 100ms timeout, allows checking exit condition

        if (msg_opt) {
            try {
                callback(*msg_opt);
            } catch (const std::exception& e) {
                logger_->error("Message processing callback exception: {}", e.what());
            }
        }
    }

    logger_->info("Message loop thread exited");
}

// ================================
// Send internal message
// ================================

auto MessageBusClient::send_internal(
    const Message& msg,
    int timeout_ms
) -> bool {
    try {
        // Serialize message
        std::string json_content = msg.serialize();

        // Build multi-part message: empty, content
        zmq::message_t empty_msg(0);
        zmq::message_t content_msg(json_content.begin(), json_content.end());

        // Send
        auto result = dealer_socket_.send(std::move(empty_msg), zmq::send_flags::sndmore);
        if (!result.has_value()) {
            return false;
        }

        result = dealer_socket_.send(std::move(content_msg), zmq::send_flags::none);
        if (!result.has_value()) {
            return false;
        }

        logger_->debug("Sent message: {} to: {}",
            msg.header.message_id, msg.header.destination);

        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to send message: {}", e.what());
        return false;
    }
}

} // namespace moltcat::messaging
