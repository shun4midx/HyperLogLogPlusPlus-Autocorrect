include_directories(include)
file(GLOB_RECURSE HLLPP_SOURCES src/*.cpp)

add_library(hllpp STATIC ${HLLPP_SOURCES})

target_include_directories(hllpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Public alias
add_library(hllpp::hllpp ALIAS hllpp)