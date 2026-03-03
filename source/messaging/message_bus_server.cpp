#include "message_bus_server.hpp"
#include <utils/uuid.hpp>
#include <glaze/json.hpp>
#include <chrono>
#include <stdexcept>

namespace moltcat::messaging {

// ================================
// Constructor
// ================================

MessageBusServer::MessageBusServer(const Config& config)
    : config_(config)
    , context_(config_.io_threads)
    , router_socket_(context_, zmq::socket_type::router)
    , pub_socket_(context_, zmq::socket_type::pub)
    , registry_(std::make_unique<ServiceRegistry>())
    , router_(std::make_unique<MessageRouter>(
          *registry_,
          config_.lb_strategy
      ))
    , logger_(spdlog::get("moltcat"))
{
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }

    logger_->set_level(config_.log_level);
    logger_->info("MessageBus server created successfully, endpoint: {}", config_.endpoint);
}

// ================================
// Destructor
// ================================

MessageBusServer::~MessageBusServer() {
    stop();
}

// ================================
// Start server
// ================================

auto MessageBusServer::start() -> bool {
    if (running_.exchange(true)) {
        logger_->warn("Server is already running");
        return false;
    }

    try {
        // Bind ROUTER socket
        router_socket_.bind(config_.endpoint);
        logger_->info("ROUTER socket bound successfully: {}", config_.endpoint);

        // Configure ROUTER socket
        if (config_.recv_timeout_ms >= 0) {
            router_socket_.set(zmq::sockopt::rcvtimeo, config_.recv_timeout_ms);
        }
        if (config_.send_timeout_ms >= 0) {
            router_socket_.set(zmq::sockopt::sndtimeo, config_.send_timeout_ms);
        }

        // Bind PUB socket (if configured)
        if (!config_.pub_endpoint.empty()) {
            pub_socket_.bind(config_.pub_endpoint);
            logger_->info("PUB socket bound successfully: {}", config_.pub_endpoint);
        }

        // Start message loop thread
        message_loop_thread_ = std::thread([this] {
            this->message_loop();
        });

        logger_->info("MessageBus server started");
        return true;

    } catch (const zmq::error_t& e) {
        logger_->error("ZeroMQ error: {}", e.what());
        running_.store(false);
        return false;
    } catch (const std::exception& e) {
        logger_->error("Failed to start server: {}", e.what());
        running_.store(false);
        return false;
    }
}

// ================================
// Stop server
// ================================

auto MessageBusServer::stop() -> void {
    if (!running_.exchange(false)) {
        return;
    }

    logger_->info("MessageBus server shutting down...");

    // Wait for message loop thread to finish
    if (message_loop_thread_.joinable()) {
        message_loop_thread_.join();
    }

    // Close sockets
    try {
        router_socket_.close();
        pub_socket_.close();
    } catch (...) {
        // Ignore errors during close
    }

    logger_->info("MessageBus server shut down");
}

// ================================
// Get server status
// ================================

auto MessageBusServer::get_status() const -> ServerStatus {
    ServerStatus status;
    status.running = running_.load();
    status.connected_clients = connected_clients_.load();
    status.registered_services = registry_->get_stats().total_services;
    status.messages_routed = messages_routed_.load();
    status.routing_errors = routing_errors_.load();

    return status;
}

// ================================
// Set load balancing strategy
// ================================

auto MessageBusServer::set_load_balance_strategy(LoadBalanceStrategy strategy)
    -> void {
    router_->set_load_balance_strategy(strategy);
    logger_->info("Load balancing strategy changed");
}

// ================================
// Message loop
// ================================

auto MessageBusServer::message_loop() -> void {
    logger_->info("Message loop thread started");

    while (running_.load()) {
        try {
            // Receive message (multi-part: sender_id, empty, message_json)
            zmq::message_t sender_msg;
            zmq::message_t empty_msg;
            zmq::message_t content_msg;

            auto result = router_socket_.recv(sender_msg);

            if (!result.has_value()) {
                // Timeout, continue loop
                continue;
            }

            // Receive empty frame
            result = router_socket_.recv(empty_msg);
            if (!result.has_value()) {
                logger_->warn("Failed to receive empty frame");
                routing_errors_++;
                continue;
            }

            // Receive message content
            result = router_socket_.recv(content_msg);
            if (!result.has_value()) {
                logger_->warn("Failed to receive message content");
                routing_errors_++;
                continue;
            }

            // Extract sender ID
            std::string sender_id(
                static_cast<char*>(sender_msg.data()),
                sender_msg.size()
            );

            // Extract message content
            std::string content(
                static_cast<char*>(content_msg.data()),
                content_msg.size()
            );

            // Deserialize message
            auto msg_opt = Message::deserialize(content);
            if (!msg_opt) {
                logger_->error("Message deserialization failed, from: {}", sender_id);
                routing_errors_++;
                continue;
            }

            Message& msg = *msg_opt;

            logger_->debug("Received message: {} from: {} type: {}",
                msg.header.message_id,
                sender_id,
                static_cast<int>(msg.header.type));

            // Handle message
            handle_message(sender_id, msg);

        } catch (const zmq::error_t& e) {
            if (running_.load()) {
                logger_->error("ZeroMQ error: {}", e.what());
                routing_errors_++;
            }
        } catch (const std::exception& e) {
            logger_->error("Message loop exception: {}", e.what());
            routing_errors_++;
        }
    }

    logger_->info("Message loop thread exited");
}

// ================================
// Handle received message
// ================================

auto MessageBusServer::handle_message(
    const std::string& sender_id,
    const Message& msg
) -> void {
    // Handle based on message type
    switch (msg.header.type) {
        case MessageType::SERVICE_REGISTER:
            handle_service_register(sender_id, msg);
            break;

        case MessageType::SERVICE_UNREGISTER:
            handle_service_unregister(sender_id, msg);
            break;

        case MessageType::SERVICE_HEARTBEAT:
            // Heartbeat message, log only for now
            logger_->debug("Received heartbeat from: {}", sender_id);
            break;

        default:
            // Route message to target service
            auto result = router_->route(msg, sender_id,
                [this, &sender_id](const std::string& recipient_id, const Message& msg) -> bool {
                    return this->send_to_queue(recipient_id, msg);
                }
            );

            if (result.success) {
                messages_routed_++;
            } else {
                routing_errors_++;
                logger_->warn("Routing failed: {}, error: {}",
                    msg.header.message_id,
                    result.error_message);
            }
            break;
    }
}

// ================================
// Handle service registration
// ================================

auto MessageBusServer::handle_service_register(
    const std::string& queue_id,
    const Message& msg
) -> void {
    // Parse service name from message body
    try {
        auto json = glz::read_json<glz::json_t>(msg.body.content);

        if (!json.contains("service_name")) {
            logger_->error("Registration message missing service_name field");
            send_response(queue_id, msg,
                R"({"error": "missing service_name field"})");
            return;
        }

        std::string service_name = json["service_name"].get<std::string>();

        // Register service
        bool success = registry_->register_service(service_name, queue_id);

        if (success) {
            logger_->info("Service registered successfully: {}, queue: {}", service_name, queue_id);
            send_response(queue_id, msg,
                R"({"status": "registered", "service_name": ")" + service_name + "\"}");
            } else {
            logger_->error("Service registration failed: {}, queue: {}", service_name, queue_id);
            send_response(queue_id, msg,
                R"({"error": "registration failed", "service_name": ")" + service_name + "\"}");
        }

    } catch (const std::exception& e) {
        logger_->error("Failed to handle service registration: {}", e.what());
        send_response(queue_id, msg,
            R"({"error": ")" + std::string(e.what()) + "\"}");
    }
}

// ================================
// Handle service unregistration
// ================================

auto MessageBusServer::handle_service_unregister(
    const std::string& queue_id,
    const Message& msg
) -> void {
    // Parse service name from message body
    try {
        auto json = glz::read_json<glz::json_t>(msg.body.content);

        if (!json.contains("service_name")) {
            logger_->error("Unregistration message missing service_name field");
            send_response(queue_id, msg,
                R"({"error": "missing service_name field"})");
            return;
        }

        std::string service_name = json["service_name"].get<std::string>();

        // Unregister service
        bool success = registry_->unregister_service(service_name, queue_id);

        if (success) {
            logger_->info("Service unregistered successfully: {}, queue: {}", service_name, queue_id);
            send_response(queue_id, msg,
                R"({"status": "unregistered", "service_name": ")" + service_name + "\"}");
            } else {
            logger_->warn("Service unregistration failed: {}, queue: {}", service_name, queue_id);
            send_response(queue_id, msg,
                R"({"error": "unregistration failed", "service_name": ")" + service_name + "\"}");
        }

    } catch (const std::exception& e) {
        logger_->error("Failed to handle service unregistration: {}", e.what());
        send_response(queue_id, msg,
            R"({"error": ")" + std::string(e.what()) + "\"}");
    }
}

// ================================
// Handle topic subscription
// ================================

auto MessageBusServer::handle_subscribe(
    const std::string& queue_id,
    const Message& msg
) -> void {
    try {
        auto json = glz::read_json<glz::json_t>(msg.body.content);

        if (!json.contains("topic")) {
            logger_->error("Subscription message missing topic field");
            send_response(queue_id, msg,
                R"({"error": "missing topic field"})");
            return;
        }

        std::string topic = json["topic"].get<std::string>();

        registry_->subscribe(queue_id, topic);
        send_response(queue_id, msg,
            R"({"status": "subscribed", "topic": ")" + topic + "\"}");

    } catch (const std::exception& e) {
        logger_->error("Failed to handle subscription: {}", e.what());
        send_response(queue_id, msg,
            R"({"error": ")" + std::string(e.what()) + "\"}");
    }
}

// ================================
// Handle topic unsubscription
// ================================

auto MessageBusServer::handle_unsubscribe(
    const std::string& queue_id,
    const Message& msg
) -> void {
    try {
        auto json = glz::read_json<glz::json_t>(msg.body.content);

        if (!json.contains("topic")) {
            logger_->error("Unsubscription message missing topic field");
            send_response(queue_id, msg,
                R"({"error": "missing topic field"})");
            return;
        }

        std::string topic = json["topic"].get<std::string>();

        registry_->unsubscribe(queue_id, topic);
        send_response(queue_id, msg,
            R"({"status": "unsubscribed", "topic": ")" + topic + "\"}");

    } catch (const std::exception& e) {
        logger_->error("Failed to handle unsubscription: {}", e.what());
        send_response(queue_id, msg,
            R"({"error": ")" + std::string(e.what()) + "\"}");
    }
}

// ================================
// Send message to specified queue
// ================================

auto MessageBusServer::send_to_queue(
    const std::string& queue_id,
    const Message& msg
) -> bool {
    try {
        // Serialize message
        std::string json_content = msg.serialize();

        // Build multi-part message: queue_id, empty, content
        zmq::message_t queue_msg(queue_id.begin(), queue_id.end());
        zmq::message_t empty_msg(0);
        zmq::message_t content_msg(json_content.begin(), json_content.end());

        // Send
        auto result = router_socket_.send(queue_id, zmq::send_flags::sndmore);
        if (!result.has_value()) {
            return false;
        }

        result = router_socket_.send(std::move(empty_msg), zmq::send_flags::sndmore);
        if (!result.has_value()) {
            return false;
        }

        result = router_socket_.send(std::move(content_msg), zmq::send_flags::none);
        if (!result.has_value()) {
            return false;
        }

        logger_->debug("Sent message to queue {}: {}", queue_id, msg.header.message_id);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Failed to send message to queue {}: {}", queue_id, e.what());
        return false;
    }
}

// ================================
// Broadcast message (via PUB socket)
// ================================

auto MessageBusServer::broadcast_message(const Message& msg) -> void {
    try {
        // Serialize message
        std::string json_content = msg.serialize();

        // Send to PUB socket
        zmq::message_t msg_json(json_content.begin(), json_content.end());
        pub_socket_.send(std::move(msg_json), zmq::send_flags::none);

        logger_->debug("Broadcast message: {}", msg.header.message_id);

    } catch (const std::exception& e) {
        logger_->error("Failed to broadcast message: {}", e.what());
    }
}

// ================================
// Send response message
// ================================

auto MessageBusServer::send_response(
    const std::string& queue_id,
    const Message& request,
    const std::string& response_content
) -> void {
    // Build response message
    Message response = Message::create(request.header.type, request.header.destination);
    response.header.source = "MessageBusServer";
    response.header.destination = request.header.source;
    response.header.correlation_id = request.header.message_id;
    response.body.content = response_content;

    // Send response
    send_to_queue(queue_id, response);
}

} // namespace moltcat::messaging
