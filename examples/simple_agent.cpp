/**
 * @file simple_agent.cpp
 * @brief Simple agent example
 * @note Uses plugin interface types
 */

#include <moltcat/molt_skill.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

/**
 * @brief Simple calculator skill
 */
class CalculatorSkill : public moltcat::IMoltSkill {
public:
    auto name() const -> const char* override { return "Calculator"; }
    auto version() const -> const char* override { return "1.0.0"; }
    auto description() const -> const char* override { return "Simple calculator skill"; }
    auto author() const -> const char* override { return "MoltCat Team"; }

    auto supported_task_types() -> moltcat::IList<moltcat::IString*>* override {
        auto list = moltcat::IList<moltcat::IString*>::create();
        list->add(moltcat::IString::create("add"));
        list->add(moltcat::IString::create("subtract"));
        list->add(moltcat::IString::create("multiply"));
        list->add(moltcat::IString::create("divide"));
        return list;
    }

    auto can_handle(const char* task_type) const -> bool override {
        return (std::strcmp(task_type, "add") == 0) ||
               (std::strcmp(task_type, "subtract") == 0) ||
               (std::strcmp(task_type, "multiply") == 0) ||
               (std::strcmp(task_type, "divide") == 0);
    }

    auto execute(const moltcat::MoltTask& task, moltcat::MoltContext& ctx) -> moltcat::MoltResult override {
        (void)ctx;
        moltcat::MoltResult result;

        // Simplified implementation: assumes payload format is "{\"a\": 10, \"b\": 5}"
        double a = 0, b = 0;
        std::sscanf(task.get_payload(), "{\"a\": %lf, \"b\": %lf}", &a, &b);

        double value = 0;
        const char* task_type = task.get_type();

        if (std::strcmp(task_type, "add") == 0) {
            value = a + b;
        } else if (std::strcmp(task_type, "subtract") == 0) {
            value = a - b;
        } else if (std::strcmp(task_type, "multiply") == 0) {
            value = a * b;
        } else if (std::strcmp(task_type, "divide") == 0) {
            if (b != 0) {
                value = a / b;
            } else {
                result.success = false;
                result.error_message = moltcat::IString::create("Division by zero");
                return result;
            }
        }

        result.success = true;
        // Build JSON result
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), R"({"result": %g})", value);
        result.data = moltcat::IString::create(buffer);
        result.status = moltcat::TaskStatus::COMPLETED;

        return result;
    }
};

int main() {
    std::cout << "MoltCat Simple Agent Example\n";
    std::cout << "============================\n\n";

    CalculatorSkill calculator;

    std::cout << "Skill name: " << calculator.name() << "\n";
    std::cout << "Version: " << calculator.version() << "\n";
    std::cout << "Description: " << calculator.description() << "\n\n";

    // Create test task
    moltcat::MoltTask task;
    task.id = moltcat::IString::create("task-001");
    task.type = moltcat::IString::create("add");
    task.payload = moltcat::IString::create(R"({"a": 15, "b": 27})");
    task.priority = moltcat::TaskPriority::NORMAL;
    task.timeout_ms = 5000;
    task.created_at = moltcat::ITimestamp::now();

    std::cout << "Executing task: " << task.get_type() << "\n";
    std::cout << "Arguments: " << task.get_payload() << "\n";

    moltcat::MoltContext ctx;
    auto result = calculator.execute(task, ctx);

    std::cout << "\nResult: " << (result.success ? "Success" : "Failed") << "\n";
    std::cout << "Data: " << result.get_data() << "\n";

    // Cleanup
    result.cleanup();
    task.id->destroy();
    task.type->destroy();
    task.payload->destroy();
    task.created_at->destroy();

    return 0;
}
