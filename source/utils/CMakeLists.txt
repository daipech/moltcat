# Basic utility library
add_library(moltcat_utils STATIC
    string_utils.cpp
    json_utils.cpp
    ed25519.cpp
    rate_limiter.cpp
)

target_include_directories(moltcat_utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(moltcat_utils
    PUBLIC
        unofficial-sodium::sodium
)
