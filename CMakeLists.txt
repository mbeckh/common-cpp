cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" IS_MAIN_PROJECT)
option(BUILD_TESTING "Build tests" ${IS_MAIN_PROJECT})

project("common-cpp" VERSION 0.1.2
    DESCRIPTION "Common C++ library"
    HOMEPAGE_URL "https://github.com/mbeckh/common-cpp"
    LANGUAGES CXX)

include("cmake/EventLog.cmake")

add_library(m3c
    "src/buffer_management.cpp"
    "src/buffer_management.h"
    "src/ClassFactory.cpp"
    "src/com_heap_ptr.cpp"
    "src/com_ptr.cpp"
    "src/ComObject.cpp"
    "src/exception.cpp"
    "src/format.cpp"
    "src/Handle.cpp"
    "src/Log.cpp"
    "src/LogData.cpp"
    "src/mutex.cpp"
    "src/PropVariant.cpp"
    "src/rpc_string.cpp"
    "src/string_encode.cpp"
#    "src/com_heap_ptr.cpp"
#    "src/format.cpp"
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
    "include/m3c/LogData.h"
    "include/m3c/mutex.h"
    "include/m3c/PropVariant.h"
    "include/m3c/rpc_string.h"
    "include/m3c/source_location.h"
    "include/m3c/string_encode.h"
    "include/m3c/type_traits.h"
    "include/m3c/unique_ptr.h"
#    "include/m3c/com_heap_ptr.h"
#    "include/m3c/format.h"
     )
add_library(m3c::m3c ALIAS m3c)

target_sources(m3c INTERFACE "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/Log-config.cpp>>"
                             "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<INSTALL_INTERFACE:include/m3c/Log-config.cpp>>")
target_events(m3c "src/m3c_events.man")

target_compile_definitions(m3c PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
target_compile_features(m3c PUBLIC cxx_std_17 PRIVATE cxx_std_20)
#target_precompile_headers(m3c PRIVATE "src/pch.h")
target_include_directories(m3c PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:share/m3c>")

set_target_properties(m3c PROPERTIES
    DEBUG_POSTFIX d
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)


find_package(fmt REQUIRED)
target_link_libraries(m3c PUBLIC fmt::fmt rpcrt4.lib)

if(IS_MAIN_PROJECT)
    include(CMakePackageConfigHelpers)
    configure_package_config_file("cmake/m3c-config.cmake.in" "m3c-config.cmake" INSTALL_DESTINATION "share/m3c")
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/m3c-config-version.cmake" VERSION ${m3c_VERSION} COMPATIBILITY SameMajorVersion)

    install(TARGETS m3c EXPORT m3c-targets)
    install(DIRECTORY "include/m3c" TYPE INCLUDE)
	install(FILES "src/Logger-config.cpp" DESTINATION "include/m3c")
    install(EXPORT m3c-targets DESTINATION "share/m3c" NAMESPACE "m3c::")
    install(FILES "${PROJECT_BINARY_DIR}/m3c-config.cmake" "${PROJECT_BINARY_DIR}/m3c-config-version.cmake"
                  "cmake/EventLog.cmake" "src/m3c_events.man" DESTINATION "share/m3c")
endif()


if(BUILD_TESTING AND IS_MAIN_PROJECT)
    include(GoogleTest)
    enable_testing()

    add_executable(m3c_Test
        "test/Bar.cpp"
        "test/Bar.h"
        "test/ClassFactory_Test.cpp"
        "test/com_heap_ptr_Test.cpp"
        "test/com_ptr_Test.cpp"
        "test/ComObject_Test.cpp"
        "test/exception_Test.cpp"
        "test/finally_Test.cpp"
        "test/Foo.cpp"
        "test/Foo.h"
        "test/format_Test.cpp"
        "test/Handle_Test.cpp"
        "test/lazy_string_Test.cpp"
        "test/Log_Test.cpp"
        "test/LogData_Test.cpp"
        "test/main.cpp"
        "test/PropVariant_Test.cpp"
        "test/rpc_string_Test.cpp"
        "test/string_encode_Test.cpp"
        "test/type_traits_Test.cpp"
        "test/unique_ptr_Test.cpp"
#        "test/exception2_Test.cpp"
#        "test/test.cpp"
       "test/source_location_Test.cpp" "test/mutex_Test.cpp")

    add_executable(m3c_Test_Log_Debug
            "test/Log_Test.cpp"
            "test/main.cpp"
    )

    add_executable(m3c_Test_Log_Stderr
            "test/Log_Test.cpp"
            "test/main.cpp"
    )

    add_executable(m3c_Test_Log_Event
            "test/Log_Test.cpp"
            "test/main.cpp"
    )

    target_compile_definitions(m3c_Test PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
    target_compile_definitions(m3c_Test_Log_Debug PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
    target_compile_definitions(m3c_Test_Log_Stderr PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
    target_compile_definitions(m3c_Test_Log_Event PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)

    target_compile_features(m3c_Test PRIVATE cxx_std_20)
    target_compile_features(m3c_Test_Log_Debug PRIVATE cxx_std_20)
    target_compile_features(m3c_Test_Log_Stderr PRIVATE cxx_std_20)
    target_compile_features(m3c_Test_Log_Event PRIVATE cxx_std_20)
    
    #target_precompile_headers(m3c_Test PRIVATE "test/pch.h")
    set_target_properties(m3c_Test m3c_Test_Log_Debug m3c_Test_Log_Stderr m3c_Test_Log_Event PROPERTIES
        DEBUG_POSTFIX d
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    find_package(GTest REQUIRED)
    find_package(detours-gmock REQUIRED)
    find_package(m4t REQUIRED)
    target_link_libraries(m3c_Test PRIVATE m3c::m3c m4t::m4t GTest::gmock detours::gmock)
    target_link_libraries(m3c_Test_Log_Debug PRIVATE m3c::m3c m4t::m4t GTest::gmock detours::gmock)
    target_link_libraries(m3c_Test_Log_Stderr PRIVATE m3c::m3c m4t::m4t GTest::gmock detours::gmock)
    target_link_libraries(m3c_Test_Log_Event PRIVATE m3c::m3c m4t::m4t GTest::gmock detours::gmock)

    target_events(m3c_Test "test/testevents.man" LEVEL Trace DEBUG STDERR EVENT)
    target_events(m3c_Test_Log_Debug "test/testevents.man" LEVEL Debug DEBUG)
    target_events(m3c_Test_Log_Stderr "test/testevents.man" LEVEL Debug STDERR)
    target_events(m3c_Test_Log_Event "test/testevents.man" LEVEL Debug EVENT)

    #gtest_discover_tests(m3c_Test)
    add_test(NAME m3c_Test_PASS COMMAND m3c_Test)
    add_test(NAME m3c_Test_Log_Debug_PASS COMMAND m3c_Test_Log_Debug)
    add_test(NAME m3c_Test_Log_Stderr_PASS COMMAND m3c_Test_Log_Stderr)
    add_test(NAME m3c_Test_Log_Event_PASS COMMAND m3c_Test_Log_Event)
endif()
