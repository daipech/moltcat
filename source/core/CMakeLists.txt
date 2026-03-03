# Core library
add_library(moltcat_core STATIC
    executor.cpp
)

target_include_directories(moltcat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_core
    PUBLIC
        moltcat_utils
        moltcat_model
)

# C++20 standard feature
target_compile_features(moltcat_core PUBLIC cxx_std_20)
