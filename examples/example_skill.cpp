/**
 * @file example_skill.cpp
 * @brief Example skill plugin
 * @note Uses plugin interface types for ABI compatibility
 */

#include <moltcat/molt_export.hpp>
#include <thread>
#include <chrono>
#include <cstring>

/**
 * @brief Example skill: echo task
 *
 * This skill receives a task and returns its content unchanged,
 * demonstrating the plugin development workflow.
 * Uses pure C interface and plugin types for cross-DLL compatibility.
 */
class EchoSkill : public moltcat::IMoltSkill {
public:
    auto name() const -> const char* override {
        return "EchoSkill";
    }

    auto version() const -> const char* override {
        return "1.0.0";
    }

    auto description() const -> const char* override {
        return "Echo skill: returns task content unchanged";
    }

    auto author() const -> const char* override {
        return "MoltCat Team";
    }

    auto supported_task_types() -> moltcat::IList<moltcat::IString*>* override {
        auto list = moltcat::IList<moltcat::IString*>::create();
        list->add(moltcat::IString::create("echo"));
        list->add(moltcat::IString::create("ping"));
        return list;
    }

    auto can_handle(const char* task_type) const -> bool override {
        return (std::strcmp(task_type, "echo") == 0) ||
               (std::strcmp(task_type, "ping") == 0);
    }

    auto execute(const moltcat::MoltTask& task, moltcat::MoltContext& ctx) -> moltcat::MoltResult override {
        (void)ctx;  // Context not used for now

        moltcat::MoltResult result;

        // Simulate processing delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Get task type and payload
        const char* task_type = task.get_type();
        const char* payload = task.get_payload();

        // Return result based on task type
        if (std::strcmp(task_type, "ping") == 0) {
            result.success = true;
            // Build JSON response
            result.data = moltcat::IString::format(R"({"message": "pong", "original": "%s"})", payload);
        } else {
            result.success = true;
            result.data = moltcat::IString::format(R"({"echo": "%s"})", payload);
        }

        result.execution_time_ms = 100;
        result.status = moltcat::TaskStatus::COMPLETED;

        return result;
    }

private:
    // Skill status statistics
    uint64_t total_executed_ = 0;
};

/**
 * @brief Skill factory
 */
class EchoSkillFactory : public moltcat::IMoltSkillFactory {
public:
    auto create() -> moltcat::IMoltSkill* override {
        return new EchoSkill();
    }

    void destroy(moltcat::IMoltSkill* skill) override {
        delete skill;
    }

    auto type_name() const -> const char* override {
        return "EchoSkill";
    }

    auto get_metadata() const -> moltcat::SkillMetadata override {
        EchoSkill skill;
        return skill.metadata();
    }

    auto get_api_version() const -> const char* override {
        return "1.0.0";
    }
};

// Export plugin (using new macro)
MOLT_EXPORT_SKILL(EchoSkillFactory, "EchoSkill", "1.0.0")
