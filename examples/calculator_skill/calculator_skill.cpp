/**
 * @file calculator_skill.cpp
 * @brief Calculator skill plugin example
 * @note This is a plugin DLL example demonstrating MOLT_EXPORT_SKILL usage
 *
 * Build instructions:
 * - Windows: cl /DMOLTCAT_BUILD /LD calculator_skill.cpp /I include /link /OUT:calculator_skill.dll
 * - Linux: g++ -DMOLTCAT_BUILD -shared -fPIC calculator_skill.cpp -I include -o calculator_skill.so
 */

#include <moltcat/molt_export.hpp>
#include <cstring>
#include <cstdio>

/**
 * @brief Calculator skill implementation
 */
class CalculatorSkill : public moltcat::IMoltSkill {
public:
    auto name() const -> const char* override {
        return "Calculator";
    }

    auto version() const -> const char* override {
        return "1.0.0";
    }

    auto description() const -> const char* override {
        return "Performs basic arithmetic operations";
    }

    auto author() const -> const char* override {
        return "MoltCat Team";
    }

    auto can_handle(const char* task_type) const -> bool override {
        return std::strcmp(task_type, "add") == 0 ||
               std::strcmp(task_type, "subtract") == 0 ||
               std::strcmp(task_type, "multiply") == 0 ||
               std::strcmp(task_type, "divide") == 0;
    }

    auto supported_task_types() -> moltcat::IList<moltcat::IString*>* override {
        auto list = moltcat::IList<moltcat::IString*>::create();
        list->add(moltcat::IString::create("add"));
        list->add(moltcat::IString::create("subtract"));
        list->add(moltcat::IString::create("multiply"));
        list->add(moltcat::IString::create("divide"));
        return list;
    }

    auto execute(const moltcat::MoltTask& task, moltcat::MoltContext& ctx) -> moltcat::MoltResult override {
        (void)ctx;
        moltcat::MoltResult{};

        // Parse payload: {"a": 10, "b": 5}
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
                moltcat::MoltResult error;
                error.success = false;
                error.error_message = moltcat::IString::create("Division by zero");
                return error;
            }
        }

        moltcat::MoltResult result;
        result.success = true;
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "{\"result\": %g}", value);
        result.data = moltcat::IString::create(buffer);
        result.status = moltcat::TaskStatus::COMPLETED;
        return result;
    }
};

/**
 * @brief Calculator skill factory
 */
class CalculatorSkillFactory : public moltcat::IMoltSkillFactory {
public:
    auto create() -> moltcat::IMoltSkill* override {
        return new CalculatorSkill();
    }

    void destroy(moltcat::IMoltSkill* skill) override {
        delete skill;
    }

    auto type_name() const -> const char* override {
        return "CalculatorSkill";
    }

    auto get_metadata() const -> moltcat::SkillMetadata override {
        moltcat::SkillMetadata meta;
        meta.name = moltcat::IString::create("Calculator");
        meta.version = moltcat::IString::create("1.0.0");
        meta.description = moltcat::IString::create("Performs basic arithmetic operations");
        meta.author = moltcat::IString::create("MoltCat Team");
        meta.supported_task_types = moltcat::IList<moltcat::IString*>::create();
        meta.supported_task_types->add(moltcat::IString::create("add"));
        meta.supported_task_types->add(moltcat::IString::create("subtract"));
        meta.supported_task_types->add(moltcat::IString::create("multiply"));
        meta.supported_task_types->add(moltcat::IString::create("divide"));
        return meta;
    }

    auto get_api_version() const -> const char* override {
        return "1.0.0";
    }
};

// Export the plugin using the macro
MOLT_EXPORT_SKILL(CalculatorSkillFactory, "Calculator Skill", "1.0.0")
