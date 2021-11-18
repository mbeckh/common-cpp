cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project("common-cpp" VERSION 0.1.2
    DESCRIPTION "Common C++ library"
    HOMEPAGE_URL "https://github.com/mbeckh/common-cpp"
    LANGUAGES CXX)

find_package(fmt REQUIRED)
include("cmake/EventLog.cmake")

add_library(m3c
    "src/ClassFactory.cpp"
    "src/COM.cpp"
    "src/com_heap_ptr.cpp"
    "src/com_ptr.cpp"
    "src/ComObject.cpp"
    "src/exception.cpp"
    "src/format.cpp"
    "src/Handle.cpp"
    "src/lazy_string.cpp"
    "src/Log.cpp"
    "src/LogArgs.cpp"
    "src/LogData.cpp"
    "src/mutex.cpp"
    "src/PropVariant.cpp"
    "src/rpc_string.cpp"
    "src/string_encode.cpp"
    "src/type_traits.cpp"
    "src/unique_ptr.cpp"
    "include/m3c/ClassFactory.h"
    "include/m3c/COM.h"
    "include/m3c/com_heap_ptr.h"
    "include/m3c/com_ptr.h"
    "include/m3c/ComObject.h"
    "include/m3c/exception.h"
    "include/m3c/finally.h"
    "include/m3c/format.h"
    "include/m3c/Handle.h"
    "include/m3c/lazy_string.h"
    "include/m3c/Log.h"
    "include/m3c/LogArgs.h"
    "include/m3c/LogData.h"
    "include/m3c/mutex.h"
    "include/m3c/PropVariant.h"
    "include/m3c/rpc_string.h"
    "include/m3c/source_location.h"
    "include/m3c/string_encode.h"
    "include/m3c/type_traits.h"
    "include/m3c/unique_ptr.h"
    )
add_library(common-cpp::m3c ALIAS m3c)

target_sources(m3c INTERFACE "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/Log-config.cpp>>"
                             "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<INSTALL_INTERFACE:include/m3c/Log-config.cpp>>")

target_compile_definitions(m3c PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
target_compile_features(m3c PUBLIC cxx_std_20)
target_precompile_headers(m3c PRIVATE "src/pch.h")
set_property(SOURCE ${BINARY_DIR}/m3c.dir/cmake_pch.cxx APPEND PROPERTY COMPILE_OPTIONS $<$<CONFIG:Release>:/Yl>)

target_include_directories(m3c PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")

set_target_properties(m3c PROPERTIES
    DEBUG_POSTFIX d
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(m3c PUBLIC fmt::fmt PRIVATE rpcrt4 propsys)
target_events(m3c "src/m3c.events.man")

include(CMakePackageConfigHelpers)
configure_package_config_file("cmake/common-cpp-config.cmake.in" "common-cpp-config.cmake" INSTALL_DESTINATION "share/common-cpp")
write_basic_package_version_file("${PROJECT_BINARY_DIR}/common-cpp-config-version.cmake" VERSION ${common-cpp_VERSION} COMPATIBILITY SameMajorVersion)

install(TARGETS m3c EXPORT m3c-targets)
install(DIRECTORY "include/m3c" TYPE INCLUDE)
install(FILES "src/Logger-config.cpp" DESTINATION "include/m3c")
install(EXPORT m3c-targets DESTINATION "share/common-cpp" NAMESPACE "common-cpp::")
install(FILES "${PROJECT_BINARY_DIR}/common-cpp-config.cmake" "${PROJECT_BINARY_DIR}/common-cpp-config-version.cmake"
              "cmake/EventLog.cmake" "cmake/merge-events.vbs" "cmake/process-events.cmake" "src/m3c.events.man"
        DESTINATION "share/common-cpp")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    enable_testing()

    find_package(GTest REQUIRED)
    find_package(detours-gmock REQUIRED)
    find_package(common-cpp-testing REQUIRED)

    add_executable(m3c_Test
        "test/ClassFactory.test.cpp"
        "test/com_heap_ptr.test.cpp"
        "test/com_ptr.test.cpp"
        "test/ComObject.test.cpp"
        "test/ComObjects.cpp"
        "test/ComObjects.h"
        "test/exception.test.cpp"
        "test/finally.test.cpp"
        "test/format.test.cpp"
        "test/Handle.test.cpp"
        "test/lazy_string.test.cpp"
        "test/Log.test.cpp"
        "test/LogData.test.cpp"
        "test/main.cpp"
        "test/mutex.test.cpp"
        "test/PropVariant.test.cpp"
        "test/rpc_string.test.cpp"
        "test/string_encode.test.cpp"
        "test/type_traits.test.cpp"
        "test/unique_ptr.test.cpp"
     )

    add_executable(m3c_Test_Log_Print
        "test/Log.test.cpp"
        "test/main.cpp"
     )

    add_executable(m3c_Test_Log_Event
        "test/Log.test.cpp"
        "test/main.cpp"
     )

    target_compile_definitions(m3c_Test PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
    target_compile_definitions(m3c_Test_Log_Print PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
    target_compile_definitions(m3c_Test_Log_Event PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)

    target_compile_features(m3c_Test PRIVATE cxx_std_20)
    target_compile_features(m3c_Test_Log_Print PRIVATE cxx_std_20)
    target_compile_features(m3c_Test_Log_Event PRIVATE cxx_std_20)

    target_precompile_headers(m3c_Test PRIVATE "test/pch.h")
    target_precompile_headers(m3c_Test_Log_Print PRIVATE "test/pch_Log.h")
    target_precompile_headers(m3c_Test_Log_Event PRIVATE "test/pch_Log.h")

    set_target_properties(m3c_Test m3c_Test_Log_Print m3c_Test_Log_Event PROPERTIES
        DEBUG_POSTFIX d
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(m3c_Test PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock)
    target_link_libraries(m3c_Test_Log_Print PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock)
    target_link_libraries(m3c_Test_Log_Event PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock)

    target_events(m3c_Test "test/test.events.man" LEVEL Trace PRINT EVENT)
    target_events(m3c_Test_Log_Print "test/test.events.man" LEVEL Debug PRINT)
    target_events(m3c_Test_Log_Event "test/test.events.man" LEVEL Debug EVENT)

    add_test(NAME m3c_Test_PASS COMMAND m3c_Test)
    add_test(NAME m3c_Test_Log_Print_PASS COMMAND m3c_Test_Log_Print)
    add_test(NAME m3c_Test_Log_Event_PASS COMMAND m3c_Test_Log_Event)
endif()
