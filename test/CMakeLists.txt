# Copyright 2021 Michael Beckh
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(common-cpp-testing REQUIRED)
find_package(detours-gmock REQUIRED)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)

add_executable(m3c_Test
    "ClassFactory.test.cpp"
    "com_heap_ptr.test.cpp"
    "com_ptr.test.cpp"
    "ComObject.test.cpp"
    "ComObjects.cpp"
    "ComObjects.h"
    "exception.test.cpp"
    "finally.test.cpp"
    "format.test.cpp"
    "Handle.test.cpp"
    "lazy_string.test.cpp"
    "Log.test.cpp"
    "LogData.test.cpp"
    "main.cpp"
    "mutex.test.cpp"
    "PropVariant.test.cpp"
    "rpc_string.test.cpp"
    "string_encode.test.cpp"
    "type_traits.test.cpp"
    "unique_ptr.test.cpp"
    )

add_executable(m3c_Test_Log_Print
    "Log.test.cpp"
    "main.cpp"
    )

add_executable(m3c_Test_Log_Event
    "Log.test.cpp"
    "main.cpp"
    )

target_compile_definitions(m3c_Test PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
target_compile_definitions(m3c_Test_Log_Print PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
target_compile_definitions(m3c_Test_Log_Event PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)

target_compile_features(m3c_Test PRIVATE cxx_std_20)
target_compile_features(m3c_Test_Log_Print PRIVATE cxx_std_20)
target_compile_features(m3c_Test_Log_Event PRIVATE cxx_std_20)

target_precompile_headers(m3c_Test PRIVATE "pch.h")
target_precompile_headers(m3c_Test_Log_Print PRIVATE "pch_Log.h")
target_precompile_headers(m3c_Test_Log_Event PRIVATE "pch_Log.h")

set_target_properties(m3c_Test m3c_Test_Log_Print m3c_Test_Log_Event PROPERTIES
    DEBUG_POSTFIX d
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(m3c_Test PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock fmt::fmt)
target_link_libraries(m3c_Test_Log_Print PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock)
target_link_libraries(m3c_Test_Log_Event PRIVATE common-cpp::m3c common-cpp-testing::m4t GTest::gmock detours-gmock::detours-gmock)

target_events(m3c_Test "test.events.man" LEVEL Trace PRINT EVENT)
target_events(m3c_Test_Log_Print "test.events.man" LEVEL Debug PRINT)
target_events(m3c_Test_Log_Event "test.events.man" LEVEL Debug EVENT)

add_test(NAME m3c_Test_PASS COMMAND m3c_Test)
add_test(NAME m3c_Test_Log_Print_PASS COMMAND m3c_Test_Log_Print)
add_test(NAME m3c_Test_Log_Event_PASS COMMAND m3c_Test_Log_Event)
