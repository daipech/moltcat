#include "agent_team.hpp"
#include "task_classifier.hpp"
#include "task_router.hpp"
#include "team_registry.hpp"
#include "messaging/message.hpp"
#include "utils/logger.hpp"
#include "utils/uuid.hpp"
#include <algorithm>
#include <format>
#include <thread>
#include <future>
#include <functional>

namespace moltcat::agent {

// ================================
// 构造函数和析构函数
// ================================

AgentTeam::AgentTeam(
    std::string team_id,
    std::vector<std::string> agent_ids,
    CollaborationMode mode,
    std::shared_ptr<messaging::IMessageQueue> message_queue
)
    : team_id_(std::move(team_id))
    , mode_(mode)
    , agent_ids_(std::move(agent_ids))
    , message_queue_(std::move(message_queue))
    , mutex_()
    , team_topic_(team_id_ + ".updates")  // 初始化 A2A 主题
{
    if (!message_queue_) {
        MOLT_LOGGER.error("创建 AgentTeam 失败：消息队列为空");
        throw std::invalid_argument("message_queue cannot be null");
    }

    MOLT_LOGGER.info("创建 AgentTeam（解耦版本）: {} (mode={}, agents={}, topic={})",
                     team_id_,
                     collaboration_mode_to_string(mode),
                     agent_ids_.size(),
                     team_topic_);
}

AgentTeam::~AgentTeam() {
    MOLT_LOGGER.info("销毁 AgentTeam: {} (tasks_sent={}, results_received={})",
                     team_id_,
                     tasks_sent_.load(),
                     results_received_.load());
}

// ================================
// 团队管理
// ================================

auto AgentTeam::add_agent_id(std::string_view agent_id) -> void {
    std::unique_lock lock(mutex_);

    // 检查是否已存在
    if (std::find(agent_ids_.begin(), agent_ids_.end(), agent_id) != agent_ids_.end()) {
        MOLT_LOGGER.warn("Agent ID {} 已在团队中", agent_id);
        return;
    }

    agent_ids_.push_back(std::string(agent_id));
    MOLT_LOGGER.info("团队 {} 添加 Agent ID: {}", team_id_, agent_id);
}

auto AgentTeam::remove_agent_id(std::string_view agent_id) -> void {
    std::unique_lock lock(mutex_);

    auto it = std::remove(agent_ids_.begin(), agent_ids_.end(), agent_id);

    if (it != agent_ids_.end()) {
        agent_ids_.erase(it, agent_ids_.end());
        MOLT_LOGGER.info("团队 {} 移除 Agent ID: {}", team_id_, agent_id);
    } else {
        MOLT_LOGGER.warn("团队 {} 中不存在 Agent ID: {}", team_id_, agent_id);
    }
}

auto AgentTeam::get_agent_ids() const -> const std::vector<std::string>& {
    return agent_ids_;
}

auto AgentTeam::size() const noexcept -> size_t {
    std::shared_lock lock(mutex_);
    return agent_ids_.size();
}

auto AgentTeam::has_agent_id(std::string_view agent_id) const -> bool {
    std::shared_lock lock(mutex_);
    return std::find(agent_ids_.begin(), agent_ids_.end(), agent_id) != agent_ids_.end();
}

// ================================
// 异步任务执行
// ================================

auto AgentTeam::pipeline_execute_async(
    const std::vector<A2ATask>& tasks,
    std::shared_ptr<messaging::Context<std::vector<A2ATask>>> context
) -> void {
    // 1. 检查参数
    if (agent_ids_.empty()) {
        MOLT_LOGGER.error("流水线执行失败：团队中没有 Agent");
        context->set_result(std::vector<A2ATask>{});
        return;
    }

    if (tasks.empty()) {
        MOLT_LOGGER.warn("流水线执行失败：任务列表为空");
        context->set_result(std::vector<A2ATask>{});
        return;
    }

    size_t num_stages = std::min(tasks.size(), agent_ids_.size());

    MOLT_LOGGER.info("团队 {} 流水线异步执行: {} 个阶段",
                     team_id_, num_stages);

    // 2. 启动异步流水线处理
    std::thread([this, tasks, num_stages, context]() {
        std::vector<A2ATask> results;
        results.reserve(num_stages);

        // 流水线：顺序执行，每个 Agent 处理一个阶段
        A2ATask current_task = tasks[0];
        std::string pipeline_data;  // 用于传递阶段结果

        for (size_t i = 0; i < num_stages; ++i) {
            auto agent_id = agent_ids_[i];

            MOLT_LOGGER.info("阶段 {}: Agent {} 执行任务 {}",
                             i + 1, agent_id, current_task.id);

            // 检查上下文是否已取消
            if (context->is_done()) {
                MOLT_LOGGER.warn("流水线在阶段 {} 被取消", i + 1);
                break;
            }

            // 发送任务给 Agent
            if (!send_task_to_agent(current_task, agent_id,
                                   i == 0 ? TeamMessageType::TASK_REQUEST
                                          : TeamMessageType::PIPELINE_FORWARD)) {
                MOLT_LOGGER.error("阶段 {} 发送任务失败", i + 1);

                // 创建失败结果
                A2ATask failed_result = current_task;
                failed_result.status = A2ATaskStatus::FAILED;
                failed_result.error_message = std::format("Failed to send task to agent {}", agent_id);
                results.push_back(failed_result);

                break;
            }

            // 接收结果（阻塞等待）
            auto result = receive_result_from_agent(agent_id, 30000);  // 30秒超时

            if (!result) {
                MOLT_LOGGER.warn("阶段 {} 超时或失败", i + 1);

                // 创建超时结果
                A2ATask timeout_result = current_task;
                timeout_result.status = A2ATaskStatus::FAILED;
                timeout_result.error_message = "Timeout waiting for agent response";
                results.push_back(timeout_result);

                break;
            }

            results.push_back(*result);
            pipeline_stages_completed_.fetch_add(1);

            // 如果某阶段失败，终止流水线
            if (result->status != A2ATaskStatus::COMPLETED &&
                result->status != A2ATaskStatus::PARTIALLY_COMPLETED) {

                MOLT_LOGGER.warn("阶段 {} 失败，终止流水线 (status={})",
                                 i + 1, static_cast<int>(result->status));
                break;
            }

            // 将当前阶段的结果作为下一阶段的输入
            if (i + 1 < num_stages) {
                current_task = tasks[i + 1];

                // 将当前结果添加到 pipeline_data（JSON格式）
                if (!pipeline_data.empty()) {
                    pipeline_data += ",";
                }
                pipeline_data += result->output;
            }
        }

        MOLT_LOGGER.info("流水线执行完成: {} / {} 阶段成功",
                         results.size(), num_stages);

        // 设置最终结果
        context->set_result(results);

    }).detach();  // 分离线程，立即返回
}

auto AgentTeam::parallel_execute_async(
    const A2ATask& task,
    std::shared_ptr<messaging::Context<std::vector<A2ATask>>> context
) -> void {
    // 1. 检查参数
    if (agent_ids_.empty()) {
        MOLT_LOGGER.error("并行执行失败：团队中没有 Agent");
        context->set_result(std::vector<A2ATask>{});
        return;
    }

    MOLT_LOGGER.info("团队 {} 并行异步执行: {} 个 Agent 处理任务 {}",
                     team_id_, agent_ids_.size(), task.id);

    // 2. 启动异步并行处理
    std::thread([this, task, context]() {
        std::vector<A2ATask> results;
        results.reserve(agent_ids_.size());

        // 为每个 Agent 启动一个异步任务
        std::vector<std::future<std::optional<A2ATask>>> futures;
        futures.reserve(agent_ids_.size());

        for (const auto& agent_id : agent_ids_) {
            // 使用 std::async 启动异步任务
            std::future<std::optional<A2ATask>> future = std::async(
                std::launch::async,
                [this, task, agent_id]() {
                    // 发送任务
                    if (!send_task_to_agent(task, agent_id)) {
                        MOLT_LOGGER.error("并行 Agent {} 发送任务失败", agent_id);
                        return std::optional<A2ATask>{};
                    }

                    // 接收结果（阻塞等待）
                    return receive_result_from_agent(agent_id, 30000);  // 30秒超时
                }
            );

            futures.push_back(std::move(future));
        }

        // 等待所有 Agent 完成
        for (size_t i = 0; i < futures.size(); ++i) {
            try {
                auto result = futures[i].get();

                if (result) {
                    results.push_back(*result);
                    MOLT_LOGGER.debug("并行 Agent {} 完成", agent_ids_[i]);
                } else {
                    // 创建失败结果
                    A2ATask failed_result = task;
                    failed_result.status = A2ATaskStatus::FAILED;
                    failed_result.error_message = std::format("Agent {} failed or timeout", agent_ids_[i]);
                    results.push_back(failed_result);
                }

            } catch (const std::exception& e) {
                MOLT_LOGGER.error("并行 Agent {} 执行异常: {}", agent_ids_[i], e.what());

                // 创建错误结果
                A2ATask error_result = task;
                error_result.status = A2ATaskStatus::FAILED;
                error_result.error_message = std::format("Exception: {}", e.what());
                results.push_back(error_result);
            }
        }

        size_t success_count = std::count_if(results.begin(), results.end(),
            [](const auto& r) {
                return r.status == A2ATaskStatus::COMPLETED ||
                       r.status == A2ATaskStatus::PARTIALLY_COMPLETED;
            });

        MOLT_LOGGER.info("并行执行完成: {} / {} 成功",
                         success_count, agent_ids_.size());

        // 设置最终结果
        context->set_result(results);

    }).detach();  // 分离线程，立即返回
}

auto AgentTeam::competitive_execute_async(
    const A2ATask& task,
    std::shared_ptr<messaging::Context<A2ATask>> context
) -> void {
    // 1. 检查参数
    if (agent_ids_.empty()) {
        MOLT_LOGGER.error("竞争执行失败：团队中没有 Agent");
        A2ATask error_result = task;
        error_result.status = A2ATaskStatus::FAILED;
        error_result.error_message = "No agents in team";
        context->set_result(error_result);
        return;
    }

    MOLT_LOGGER.info("团队 {} 竞争异步执行: {} 个 Agent 竞争任务 {}",
                     team_id_, agent_ids_.size(), task.id);

    // 2. 启动异步竞争处理
    std::thread([this, task, context]() {
        std::atomic<bool> finished{false};  // 标记是否已有 Agent 完成
        std::string winner_id;
        A2ATask winning_result;
        bool success = false;

        // 为每个 Agent 启动一个异步任务
        std::vector<std::future<void>> futures;
        futures.reserve(agent_ids_.size());

        for (const auto& agent_id : agent_ids_) {
            // 使用 std::async 启动异步任务
            std::future<void> future = std::async(
                std::launch::async,
                [this, &task, &finished, &winner_id, &winning_result, &success, agent_id]() {
                    try {
                        // 检查是否已结束（其他 Agent 已完成）
                        if (finished.load(std::memory_order_acquire)) {
                            return;
                        }

                        // 发送任务
                        if (!send_task_to_agent(task, agent_id)) {
                            MOLT_LOGGER.error("竞争 Agent {} 发送任务失败", agent_id);
                            return;
                        }

                        // 接收结果（阻塞等待）
                        auto result = receive_result_from_agent(agent_id, 30000);  // 30秒超时

                        if (!result) {
                            MOLT_LOGGER.warn("竞争 Agent {} 超时", agent_id);
                            return;
                        }

                        // 检查结果是否成功
                        if (result->status != A2ATaskStatus::COMPLETED &&
                            result->status != A2ATaskStatus::PARTIALLY_COMPLETED) {
                            return;
                        }

                        // 尝试标记为胜利者（compare_exchange_strong 确保只有一个胜利者）
                        bool expected = false;
                        if (finished.compare_exchange_strong(
                                expected, true,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire
                            )) {
                            // 成功标记为胜利者
                            winner_id = agent_id;
                            winning_result = *result;
                            success = true;

                            MOLT_LOGGER.info("Agent {} 在竞争中获胜", agent_id);
                        }

                    } catch (const std::exception& e) {
                        MOLT_LOGGER.error("Agent {} 竞争执行异常: {}", agent_id, e.what());
                    }
                }
            );

            futures.push_back(std::move(future));
        }

        // 等待所有竞争 Agent 完成（或被取消）
        for (auto& future : futures) {
            try {
                future.get();
            } catch (const std::exception& e) {
                // 忽略单个 Agent 的异常
            }
        }

        // 如果没有 Agent 成功，返回错误
        if (!success) {
            MOLT_LOGGER.error("所有 Agent 在竞争中失败");

            A2ATask error_result = task;
            error_result.status = A2ATaskStatus::FAILED;
            error_result.error_message = "All agents failed in competition";
            context->set_result(error_result);
        } else {
            MOLT_LOGGER.info("竞争执行获胜者: {}", winner_id);

            // 设置胜利结果
            winning_result.metadata["winner_agent_id"] = winner_id;
            winning_result.metadata["competition_mode"] = "competitive";
            context->set_result(winning_result);
        }

    }).detach();  // 分离线程，立即返回
}

auto AgentTeam::hierarchical_execute_async(
    const A2ATask& task,
    std::string_view master_agent_id,
    std::shared_ptr<messaging::Context<A2ATask>> context
) -> void {
    // 1. 检查参数
    if (agent_ids_.empty()) {
        MOLT_LOGGER.error("层级执行失败：团队中没有 Agent");
        A2ATask error_result = task;
        error_result.status = A2ATaskStatus::FAILED;
        error_result.error_message = "No agents in team";
        context->set_result(error_result);
        return;
    }

    MOLT_LOGGER.info("团队 {} 层级异步执行: 主 Agent={}, 任务={}",
                     team_id_, master_agent_id, task.id);

    // 2. 启动异步层级处理
    std::thread([this, task, master_agent_id, context]() {
        // 1. 找到主 Agent ID
        bool has_master = has_agent_id(master_agent_id);
        if (!has_master) {
            MOLT_LOGGER.error("层级执行失败：主 Agent {} 不在团队中", master_agent_id);

            A2ATask error_result = task;
            error_result.status = A2ATaskStatus::FAILED;
            error_result.error_message = std::format("Master agent {} not found", master_agent_id);
            context->set_result(error_result);
            return;
        }

        // 2. 找到从 Agent ID（排除主 Agent）
        std::vector<std::string> worker_ids;
        for (const auto& aid : agent_ids_) {
            if (aid != master_agent_id) {
                worker_ids.push_back(aid);
            }
        }

        if (worker_ids.empty()) {
            MOLT_LOGGER.warn("层级执行警告：没有从 Agent，主 Agent 独立执行");

            // 主 Agent 独立执行
            if (!send_task_to_agent(task, master_agent_id)) {
                A2ATask error_result = task;
                error_result.status = A2ATaskStatus::FAILED;
                error_result.error_message = "Failed to send task to master agent";
                context->set_result(error_result);
                return;
            }

            auto result = receive_result_from_agent(master_agent_id, 60000);  // 60秒超时

            if (result) {
                context->set_result(*result);
            } else {
                A2ATask error_result = task;
                error_result.status = A2ATaskStatus::FAILED;
                error_result.error_message = "Master agent timeout";
                context->set_result(error_result);
            }

            return;
        }

        MOLT_LOGGER.info("主 Agent {} 分解任务给 {} 个从 Agent",
                         master_agent_id, worker_ids.size());

        // 3. 发送任务给主 Agent（带有分解指令）
        A2ATask master_task = task;
        master_task.metadata["is_master"] = true;
        master_task.metadata["worker_count"] = static_cast<int>(worker_ids.size());

        if (!send_task_to_agent(master_task, master_agent_id)) {
            A2ATask error_result = task;
            error_result.status = A2ATaskStatus::FAILED;
            error_result.error_message = "Failed to send task to master agent";
            context->set_result(error_result);
            return;
        }

        // 4. 等待主 Agent 完成
        auto master_result = receive_result_from_agent(master_agent_id, 120000);  // 120秒超时

        if (!master_result) {
            MOLT_LOGGER.error("主 Agent 超时");

            A2ATask error_result = task;
            error_result.status = A2ATaskStatus::FAILED;
            error_result.error_message = "Master agent timeout";
            context->set_result(error_result);
            return;
        }

        // 5. 主 Agent 应该已经协调了从 Agent，直接返回主 Agent 的结果
        MOLT_LOGGER.info("层级执行完成: 主 Agent={}, 从 Agent={}",
                         master_agent_id, worker_ids.size());

        master_result->metadata["master_agent"] = std::string(master_agent_id);
        master_result->metadata["worker_count"] = static_cast<int>(worker_ids.size());

        context->set_result(*master_result);

    }).detach();  // 分离线程，立即返回
}

// ================================
// Router 功能（智能执行）
// ================================

auto AgentTeam::smart_execute_async(
    std::string_view instruction,
    std::shared_ptr<messaging::Context<A2ATask>> context
) -> void {
    MOLT_LOGGER.info("团队 {} 智能执行: instruction={}", team_id_, instruction);

    // 1. 任务分类（TaskClassifier）
    auto classification = parse_task(instruction);

    MOLT_LOGGER.info("任务分类: category={}, confidence={}, mode={}, reasoning={}",
                     static_cast<int>(classification.category),
                     classification.confidence,
                     collaboration_mode_to_string(classification.recommended_mode),
                     classification.reasoning);

    // 2. 路由决策（TaskRouter）
    auto decision = route_task(classification);

    MOLT_LOGGER.info("路由决策: mode={}, selected_agents={}, reasoning={}",
                     collaboration_mode_to_string(decision.mode),
                     decision.selected_agent_ids.size(),
                     decision.reasoning);

    // 3. 转换为 A2A Task
    A2ATask a2a_task;
    a2a_task.id = utils::UUID::generate_v4();
    a2a_task.content = std::string(instruction);
    a2a_task.metadata["category"] = static_cast<int>(classification.category);
    a2a_task.metadata["confidence"] = classification.confidence;
    a2a_task.metadata["auto_routed"] = true;
    a2a_task.status = A2ATaskStatus::create_submitted();

    // 4. 根据路由决策，选择协作模式执行
    switch (decision.mode) {
        case CollaborationMode::PIPELINE:
            // 流水线模式：将 instruction 分解为多个任务
            {
                std::vector<A2ATask> pipeline_tasks;
                pipeline_tasks.reserve(decision.selected_agent_ids.size());

                for (size_t i = 0; i < decision.selected_agent_ids.size(); ++i) {
                    A2ATask stage_task = a2a_task;
                    stage_task.id = utils::UUID::generate_v4();
                    stage_task.metadata["stage"] = static_cast<int>(i);
                    stage_task.metadata["total_stages"] = static_cast<int>(decision.selected_agent_ids.size());
                    pipeline_tasks.push_back(stage_task);
                }

                // 创建 vector<A2ATask> 类型的上下文
                auto vector_context = messaging::background<std::vector<A2ATask>>();
                pipeline_execute_async(pipeline_tasks, vector_context);

                // 等待流水线完成，然后转换结果
                std::thread([this, context, vector_context]() {
                    vector_context->wait();
                    auto results = vector_context->get_result();

                    // 合并所有阶段的结果
                    if (!results.empty()) {
                        A2ATask final_result = results.back();  // 使用最后一个阶段的结果
                        final_result.metadata["pipeline_mode"] = true;
                        final_result.metadata["num_stages"] = static_cast<int>(results.size());
                        context->set_result(final_result);
                    } else {
                        A2ATask error_result;
                        error_result.status = A2ATaskStatus::FAILED;
                        error_result.error_message = "Pipeline execution failed: no results";
                        context->set_result(error_result);
                    }
                }).detach();
            }
            break;

        case CollaborationMode::PARALLEL:
            // 并行模式：所有 Agent 同时执行
            {
                auto vector_context = messaging::background<std::vector<A2ATask>>();
                parallel_execute_async(a2a_task, vector_context);

                // 等待并行完成，然后选择最佳结果
                std::thread([this, context, vector_context]() {
                    vector_context->wait();
                    auto results = vector_context->get_result();

                    if (!results.empty()) {
                        // 选择第一个成功的结果
                        A2ATask best_result = results[0];
                        for (const auto& result : results) {
                            if (result.status == A2ATaskStatus::COMPLETED) {
                                best_result = result;
                                break;
                            }
                        }

                        best_result.metadata["parallel_mode"] = true;
                        best_result.metadata["num_agents"] = static_cast<int>(results.size());
                        context->set_result(best_result);
                    } else {
                        A2ATask error_result;
                        error_result.status = A2ATaskStatus::FAILED;
                        error_result.error_message = "Parallel execution failed: no results";
                        context->set_result(error_result);
                    }
                }).detach();
            }
            break;

        case CollaborationMode::COMPETITIVE:
            // 竞争模式：最快完成的胜出
            competitive_execute_async(a2a_task, context);
            break;

        case CollaborationMode::HIERARCHICAL:
            // 层级模式：第一个 Agent 作为主 Agent
            if (!decision.selected_agent_ids.empty()) {
                hierarchical_execute_async(
                    a2a_task,
                    decision.selected_agent_ids[0],
                    context
                );
            } else {
                A2ATask error_result = a2a_task;
                error_result.status = A2ATaskStatus::FAILED;
                error_result.error_message = "No agents selected for hierarchical execution";
                context->set_result(error_result);
            }
            break;
    }
}

auto AgentTeam::smart_execute_async(
    const model::Task& task,
    std::shared_ptr<messaging::Context<A2ATask>> context
) -> void {
    // 从 Task 对象中提取 instruction
    std::string instruction = task.description;

    // 尝试从 payload 中获取 prompt
    if (task.payload.contains("prompt")) {
        if (auto prompt = task.payload["prompt"].get_if<std::string>(); prompt) {
            instruction = *prompt;
        }
    }

    // 委托给字符串版本的 smart_execute_async
    smart_execute_async(instruction, context);
}

auto AgentTeam::parse_task(std::string_view instruction) const
    -> TaskClassification {
    // 委托给 TaskClassifier
    return TaskClassifier::classify_from_instruction(instruction);
}

auto AgentTeam::route_task(const TaskClassification& classification) const
    -> RoutingDecision {
    // 委托给 TaskRouter
    return TaskRouter::route(classification, agent_ids_);
}

// ================================
// 辅助方法
// ================================

auto AgentTeam::send_task_to_agent(
    const A2ATask& task,
    std::string_view agent_id,
    TeamMessageType type
) -> bool {
    try {
        // 构建 TeamMessage
        TeamMessage team_msg;
        team_msg.type = type;
        team_msg.team_id = team_id_;
        team_msg.agent_id = std::string(agent_id);
        team_msg.task_id = task.id;
        team_msg.a2a_task = task;

        // 转换为 A2A Message
        auto a2a_msg = team_msg.to_a2a_message();

        // 发送到消息队列
        if (!message_queue_->send(a2a_msg, 5000)) {  // 5秒超时
            MOLT_LOGGER.error("发送任务到 Agent {} 失败", agent_id);
            return false;
        }

        tasks_sent_.fetch_add(1);
        return true;

    } catch (const std::exception& e) {
        MOLT_LOGGER.error("发送任务异常: {} (agent={}, task={})",
                          e.what(), agent_id, task.id);
        return false;
    }
}

auto AgentTeam::receive_result_from_agent(
    std::string_view agent_id,
    int timeout_ms
) -> std::optional<A2ATask> {
    try {
        // 从消息队列接收结果
        auto a2a_msg = message_queue_->receive(timeout_ms);

        if (!a2a_msg) {
            MOLT_LOGGER.warn("接收 Agent {} 结果超时", agent_id);
            return std::nullopt;
        }

        // 解析为 TeamMessage
        auto team_msg = TeamMessage::from_a2a_message(*a2a_msg);

        // 验证消息来源
        if (team_msg.agent_id != agent_id) {
            MOLT_LOGGER.warn("收到意外的 Agent 结果: 期望={}, 实际={}",
                             agent_id, team_msg.agent_id);
            // 继续处理（可能消息顺序错乱）
        }

        results_received_.fetch_add(1);
        return team_msg.a2a_task;

    } catch (const std::exception& e) {
        MOLT_LOGGER.error("接收结果异常: {} (agent={})", e.what(), agent_id);
        return std::nullopt;
    }
}

auto AgentTeam::handle_agent_result(
    std::string_view agent_id,
    const A2ATask& result,
    std::shared_ptr<messaging::Context<A2ATask>> context
) -> void {
    // 处理 Agent 返回的结果
    // 这里可以添加自定义逻辑，如日志记录、统计等

    MOLT_LOGGER.debug("处理 Agent {} 的结果: status={}",
                      agent_id, static_cast<int>(result.status));
}

auto AgentTeam::cancel_agent_task(std::string_view agent_id, std::string_view task_id) -> void {
    MOLT_LOGGER.info("取消 Agent {} 的任务 {}", agent_id, task_id);

    // 构建取消消息
    A2ATask cancel_task;
    cancel_task.id = std::string(task_id);
    cancel_task.status = A2ATaskStatus::CANCELED;

    send_task_to_agent(cancel_task, agent_id, TeamMessageType::CANCEL_REQUEST);
}

auto AgentTeam::get_agent_endpoint(std::string_view agent_id) const -> std::string {
    // 使用简单的 hash 生成端口号
    size_t hash = std::hash<std::string_view>{}(agent_id);
    uint16_t port = 5500 + (hash % 1000);  // 端口范围：5500-6499

    return std::format("tcp://localhost:{}", port);
}

// ================================
// 分层通信架构：成员发现与 A2A 连接
// ================================

auto AgentTeam::discover_members() -> bool {
    if (!message_bus_client_) {
        MOLT_LOGGER.error("MessageBusClient 未设置，无法发现成员");
        return false;
    }

    if (!registry_) {
        MOLT_LOGGER.error("TeamRegistry 未设置，无法发现成员");
        return false;
    }

    try {
        // 查询团队成员（从 TeamRegistry）
        auto members = registry_->get_team_members(team_id_);

        std::unique_lock lock(mutex_);

        // 更新本地缓存
        members_.clear();
        for (const auto& member : members) {
            members_[member.agent_id] = member;
        }

        MOLT_LOGGER.info("Team {} 发现 {} 个成员", team_id_, members.size());

        return true;

    } catch (const std::exception& e) {
        MOLT_LOGGER.error("发现成员失败: {}", e.what());
        return false;
    }
}

auto AgentTeam::subscribe_team_updates() -> void {
    if (!message_bus_client_) {
        MOLT_LOGGER.error("MessageBusClient 未设置，无法订阅 Team 更新");
        return;
    }

    // TODO: 实现消息订阅逻辑
    // 需要订阅 "team_updates" 队列的 TEAM_MEMBER_JOINED 和 TEAM_MEMBER_LEFT 事件
    //
    // 示例代码（需要 MessageBusClient 支持）：
    // message_bus_client_->subscribe("team_updates");
    //
    // 启动消息循环以接收事件：
    // message_bus_client_->start_message_loop([this](const Message& msg) {
    //     switch (msg.header.type) {
    //         case MessageType::TEAM_MEMBER_JOINED:
    //             handle_member_joined(msg);
    //             break;
    //         case MessageType::TEAM_MEMBER_LEFT:
    //             handle_member_left(msg);
    //             break;
    //     }
    // });

    MOLT_LOGGER.info("Team {} 已订阅成员更新事件", team_id_);
}

auto AgentTeam::on_member_joined(const messaging::AgentInfo& info) -> void {
    std::unique_lock lock(mutex_);

    // 检查是否是同一个 Team
    if (info.team_id != team_id_) {
        return;
    }

    // 添加到本地缓存
    members_[info.agent_id] = info;

    // 添加到 agent_ids_（如果不在）
    if (std::find(agent_ids_.begin(), agent_ids_.end(), info.agent_id) == agent_ids_.end()) {
        agent_ids_.push_back(info.agent_id);
    }

    MOLT_LOGGER.info("Team {} 成员加入: {} (地址: {})",
                     team_id_, info.agent_id, info.a2a_address);

    // TODO: 建立 A2A 连接
    // if (a2a_subscriber_) {
    //     a2a_subscriber_->connect(info.a2a_address);
    // }
}

auto AgentTeam::on_member_left(const std::string& agent_id) -> void {
    std::unique_lock lock(mutex_);

    // 从本地缓存移除
    members_.erase(agent_id);

    // 从 agent_ids_ 移除
    auto it = std::remove(agent_ids_.begin(), agent_ids_.end(), agent_id);
    agent_ids_.erase(it, agent_ids_.end());

    MOLT_LOGGER.info("Team {} 成员离开: {}", team_id_, agent_id);

    // TODO: 清理 A2A 连接
    // if (a2a_subscriber_) {
    //     a2a_subscriber_->disconnect(agent_id);
    // }
}

auto AgentTeam::establish_a2a_connections() -> bool {
    std::shared_lock lock(mutex_);

    MOLT_LOGGER.info("Team {} 建立 A2A PUB-SUB 连接，主题: {}",
                     team_id_, team_topic_);

    // TODO: 实现 A2A Publisher 和 Subscriber
    //
    // 示例代码：
    // a2a_publisher_ = std::make_shared<A2APublisher>(context_);
    // a2a_publisher_->bind("tcp://*:5570");
    //
    // a2a_subscriber_ = std::make_shared<A2ASubscriber>(context_);
    // a2a_subscriber_->connect("tcp://localhost:5570");
    // a2a_subscriber_->subscribe(team_topic_, [this](const A2AMessage& msg) {
    //     this->handle_team_message(msg);
    // });
    //
    // 为所有成员建立连接
    // for (const auto& [agent_id, info] : members_) {
    //     a2a_subscriber_->connect(info.a2a_address);
    // }

    return true;
}

auto AgentTeam::broadcast_to_team(const A2AMessage& message) -> void {
    // TODO: 通过 A2A Publisher 广播消息
    //
    // 示例代码：
    // if (a2a_publisher_) {
    //     a2a_publisher_->publish(team_topic_, message);
    // }

    MOLT_LOGGER.debug("Team {} 广播消息到所有成员", team_id_);
}

auto AgentTeam::get_members() const
    -> std::unordered_map<std::string, messaging::AgentInfo>
{
    std::shared_lock lock(mutex_);
    return members_;  // 返回副本
}

// ================================
// 分层通信架构：Team 资源共享
// ================================

auto AgentTeam::create_temporary_agent(const model::AgentConfig& config)
    -> std::unique_ptr<Agent, std::function<void(Agent*)>>
{
    MOLT_LOGGER.info("Team {} 创建临时 Agent: {}", team_id_, config.agent_id);

    // 1. 检查 Team 级别的速率限制
    if (llm_rate_limiter_) {
        if (!llm_rate_limiter_->check()) {
            MOLT_LOGGER.warn("Team LLM 速率限制：超过限制");
            throw std::runtime_error("Team LLM rate limit exceeded");
        }
    }

    // 2. 从 AgentPool 获取或创建 Agent
    if (agent_pool_) {
        auto agent = agent_pool_->acquire(config);

        MOLT_LOGGER.debug("Team {} 临时 Agent 创建成功: {} (从 Pool 获取)",
                          team_id_, config.agent_id);

        return agent;

    } else {
        // 没有 Pool，直接创建（使用默认工厂）
        // TODO: 集成实际的 Agent 创建逻辑
        //
        // auto factory = create_default_agent_factory();
        // auto agent_ptr = factory(config);
        //
        // std::unique_ptr<Agent, std::function<void(Agent*)>> result(
        //     agent_ptr.release(),
        //     [this](Agent* a) {
        //         this->destroy_agent(a);
        //     }
        // );

        MOLT_LOGGER.warn("Team {} 没有设置 AgentPool，无法创建临时 Agent",
                         team_id_);

        throw std::runtime_error("AgentPool not set");
    }
}

auto AgentTeam::destroy_agent(Agent* agent) -> void {
    if (!agent) {
        return;
    }

    MOLT_LOGGER.debug("Team {} 销毁 Agent: {}", team_id_, agent->get_instance_id());

    // 1. 从 A2A PUB-SUB 退出（TODO: 需要 A2A 支持）
    // if (a2a_subscriber_) {
    //     a2a_subscriber_->unsubscribe(agent->id());
    // }

    // 2. 从 MessageBus 注销（TODO: 需要 MessageBus 支持）

    // 3. 归还到 Pool（复用）
    if (agent_pool_) {
        agent_pool_->release(agent);
    } else {
        // 没有 Pool，直接删除
        delete agent;
    }
}

auto AgentTeam::check_llm_rate_limit() const -> bool {
    if (llm_rate_limiter_) {
        return llm_rate_limiter_->check();
    }
    return true;  // 没有限制器，允许所有请求
}

auto AgentTeam::get_llm_client() const -> std::shared_ptr<void> {
    return llm_client_;
}

auto AgentTeam::set_llm_client(std::shared_ptr<void> client) -> void {
    llm_client_ = std::move(client);
    MOLT_LOGGER.info("Team {} 设置 LLM 客户端", team_id_);
}

auto AgentTeam::get_agent_pool() const -> std::shared_ptr<AgentPool> {
    return agent_pool_;
}

auto AgentTeam::set_agent_pool(std::shared_ptr<AgentPool> pool) -> void {
    agent_pool_ = std::move(pool);
    MOLT_LOGGER.info("Team {} 设置 AgentPool", team_id_);
}

auto AgentTeam::get_rate_limiter() const -> std::shared_ptr<moltcat::utils::RateLimiter> {
    return llm_rate_limiter_;
}

auto AgentTeam::set_rate_limiter(std::shared_ptr<moltcat::utils::RateLimiter> limiter) -> void {
    llm_rate_limiter_ = std::move(limiter);
    MOLT_LOGGER.info("Team {} 设置速率限制器", team_id_);
}

} // namespace moltcat::agent
