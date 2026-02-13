# MoltCat 测试配置
# 使用 Google Test 框架

find_package(GTest CONFIG REQUIRED)

# 测试可执行文件
add_executable(moltcat_tests
    main.cpp
)

target_link_libraries(moltcat_tests
    PRIVATE
        moltcat_core
        GTest::gtest
        GTest::gtest_main
)

# 包含测试目录
target_include_directories(moltcat_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 注册测试
discover_tests(moltcat_tests)
