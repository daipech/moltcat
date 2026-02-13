# Calculator Skill Plugin Example

This is a complete example demonstrating how to create a MoltCat skill plugin.

## Overview

The Calculator Skill plugin demonstrates:
- Implementing `IMoltSkill` interface
- Implementing `IMoltSkillFactory` interface
- Using `MOLT_EXPORT_SKILL` macro for plugin export
- Proper memory management with `IString*` and `IList<T>*` interfaces

## Supported Operations

- `add`: Add two numbers
- `subtract`: Subtract two numbers
- `multiply`: Multiply two numbers
- `divide`: Divide two numbers

## Task Format

```json
{
  "type": "add",
  "payload": "{\"a\": 10, \"b\": 5}"
}
```

## Building

### Windows (Visual Studio)

```cmd
# Define MOLTCAT_BUILD for DLL export
cl /DMOLTCAT_BUILD /LD calculator_skill.cpp /I ..\..\include /link /OUT:calculator_skill.dll
```

### Windows (CMake)

```cmd
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux/macOS (GCC/Clang)

```bash
# Define MOLTCAT_BUILD for shared library export
g++ -DMOLTCAT_BUILD -shared -fPIC calculator_skill.cpp -I ../../include -o calculator_skill.so
```

### Linux/macOS (CMake)

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## Plugin Structure

```cpp
// 1. Implement IMoltSkill interface
class CalculatorSkill : public moltcat::IMoltSkill {
    // Override required methods:
    // - name(), version(), description()
    // - can_handle(), supported_task_types()
    // - execute()
};

// 2. Implement IMoltSkillFactory interface
class CalculatorSkillFactory : public moltcat::IMoltSkillFactory {
    // Override required methods:
    // - create(), destroy()
    // - type_name(), get_metadata()
};

// 3. Export plugin using macro
MOLT_EXPORT_SKILL(CalculatorSkillFactory, "Calculator Skill", "1.0.0")
```

## Key Points

1. **Memory Management**:
   - Framework manages input parameters (don't free them)
   - Plugin creates output `IString*` using `IString::create()`
   - Framework frees output `IString*` after use

2. **ABI Compatibility**:
   - Use `IString*`, `IList<T>*` instead of `std::string`, STL containers
   - Avoid STL types in plugin interface to prevent DLL boundary issues

3. **Export Macro**:
   - `MOLTCAT_BUILD` must be defined when building the plugin
   - This triggers proper DLL export on Windows and visibility attributes on POSIX

4. **Task Execution**:
   - `execute()` receives `MoltTask` (input) and `MoltContext` (context)
   - Return `MoltResult` with success status and optional data/error_message
