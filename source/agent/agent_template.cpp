#include "agent_template.hpp"
#include "agent.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace moltcat::agent {

AgentTemplate::AgentTemplate(
    std::string template_id,
    std::string name,
    std::string description,
    const model::AgentConfig& config,
    const model::AgentMetadata& metadata
)
    : template_id_(std::move(template_id))
    , name_(std::move(name))
    , description_(std::move(description))
    , config_(config)
    , metadata_(metadata)
{
    MOLT_LOGGER.info("创建Agent模板: {} ({})", template_id_, name_);
}

auto AgentTemplate::create_instance(std::string instance_id) const
    -> std::unique_ptr<Agent> {

    MOLT_LOGGER.info("从模板 {} 创建Agent实例: {}", template_id_, instance_id);

    // 创建Agent实例，共享模板的配置
    auto agent = std::make_unique<Agent>(
        instance_id,
        *this  // 传递模板引用
    );

    return agent;
}

auto AgentTemplate::create_instance(
    std::string instance_id,
    const model::AgentConfig& config_overrides
) const -> std::unique_ptr<Agent> {

    MOLT_LOGGER.info("从模板 {} 创建Agent实例（带配置覆盖）: {}", template_id_, instance_id);

    // 合并配置：以overrides覆盖默认配置
    model::AgentConfig merged_config = config_;

    // 覆盖非空字段
    if (!config_overrides.agent_id.empty()) {
        merged_config.agent_id = config_overrides.agent_id;
    }
    if (!config_overrides.name.empty()) {
        merged_config.name = config_overrides.name;
    }
    if (!config_overrides.description.empty()) {
        merged_config.description = config_overrides.description;
    }
    if (!config_overrides.model_name.empty()) {
        merged_config.model_name = config_overrides.model_name;
    }
    if (!config_overrides.provider_id.empty()) {
        merged_config.provider_id = config_overrides.provider_id;
    }

    // 覆盖数值字段（非零值）
    if (config_overrides.max_memory_bytes != 0) {
        merged_config.max_memory_bytes = config_overrides.max_memory_bytes;
    }
    if (config_overrides.task_timeout_ms != 0) {
        merged_config.task_timeout_ms = config_overrides.task_timeout_ms;
    }
    if (config_overrides.requests_per_minute != 0) {
        merged_config.requests_per_minute = config_overrides.requests_per_minute;
    }
    if (config_overrides.tokens_per_minute != 0) {
        merged_config.tokens_per_minute = config_overrides.tokens_per_minute;
    }

    // 覆盖能力列表
    if (!config_overrides.allowed_capabilities.empty()) {
        merged_config.allowed_capabilities = config_overrides.allowed_capabilities;
    }

    // 创建Agent实例（带配置覆盖）
    auto agent = std::make_unique<Agent>(
        instance_id,
        *this,              // 传递模板引用
        merged_config       // 传递覆盖后的配置
    );

    return agent;
}

auto AgentTemplate::validate() const -> bool {
    // 验证必填字段
    if (template_id_.empty()) {
        MOLT_LOGGER.error("模板验证失败: template_id为空");
        return false;
    }

    if (name_.empty()) {
        MOLT_LOGGER.error("模板验证失败: name为空");
        return false;
    }

    if (config_.provider_id.empty()) {
        MOLT_LOGGER.error("模板验证失败: provider_id为空");
        return false;
    }

    // 验证数值范围
    if (config_.requests_per_minute == 0) {
        MOLT_LOGGER.error("模板验证失败: requests_per_minute为0");
        return false;
    }

    // 验证能力列表
    if (config_.allowed_capabilities.empty()) {
        MOLT_LOGGER.warn("模板警告: allowed_capabilities为空");
    }

    MOLT_LOGGER.info("模板验证通过: {}", template_id_);
    return true;
}

} // namespace moltcat::agent
