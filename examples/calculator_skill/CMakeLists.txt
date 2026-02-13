# Calculator Skill Plugin CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(CalculatorSkill VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ================================
# Platform-specific settings
# ================================

if(WIN32)
    # Windows DLL build
    add_definitions(-DMOLTCAT_BUILD)
elseif(UNIX)
    # POSIX shared library build
    add_compile_options(-fPIC)
    add_definitions(-DMOLTCAT_BUILD)
endif()

# ================================
# Create the plugin shared library
# ================================

add_library(calculator_skill SHARED
    calculator_skill.cpp
)

# Include MoltCat headers
target_include_directories(calculator_skill
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# ================================
# Output naming
# ================================

if(WIN32)
    set_target_properties(calculator_skill PROPERTIES
        PREFIX ""
        OUTPUT_NAME "calculator_skill"
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(calculator_skill PROPERTIES
        PREFIX ""
        OUTPUT_NAME "calculator_skill"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(calculator_skill PROPERTIES
        PREFIX ""
        OUTPUT_NAME "calculator_skill"
        SUFFIX ".so"
    )
endif()

# Set output directory
set_target_properties(calculator_skill PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/plugins
)
