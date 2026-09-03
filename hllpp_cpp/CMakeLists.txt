cmake_minimum_required(VERSION 3.10)
project(hllpp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

include(GNUInstallDirs)

install(TARGETS hllpp
    EXPORT hllppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(PROGRAMS
    scripts/hllpp_g++
    scripts/hllpp_clang++
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Export CMake config files
install(EXPORT hllppTargets
    FILE hllppTargets.cmake
    NAMESPACE hllpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hllpp
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hllppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/hllppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hllppConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hllpp
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hllppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hllppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hllpp
)