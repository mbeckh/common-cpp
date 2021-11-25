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

include("${PROJECT_SOURCE_DIR}/cmake/EventLog.cmake")

find_package(fmt REQUIRED)

add_library(m3c
    "ClassFactory.cpp"
    "com_heap_ptr.cpp"
    "com_ptr.cpp"
    "ComObject.cpp"
    "exception.cpp"
    "format.cpp"
    "Handle.cpp"
    "lazy_string.cpp"
    "Log.cpp"
    "LogArgs.cpp"
    "LogData.cpp"
    "mutex.cpp"
    "PropVariant.cpp"
    "rpc_string.cpp"
    "string_encode.cpp"
    "unique_ptr.cpp"
    "../include/m3c/ClassFactory.h"
    "../include/m3c/COM.h"
    "../include/m3c/com_heap_ptr.h"
    "../include/m3c/com_ptr.h"
    "../include/m3c/ComObject.h"
    "../include/m3c/exception.h"
    "../include/m3c/finally.h"
    "../include/m3c/format.h"
    "../include/m3c/Handle.h"
    "../include/m3c/lazy_string.h"
    "../include/m3c/Log.h"
    "../include/m3c/LogArgs.h"
    "../include/m3c/LogData.h"
    "../include/m3c/mutex.h"
    "../include/m3c/PropVariant.h"
    "../include/m3c/rpc_string.h"
    "../include/m3c/source_location.h"
    "../include/m3c/string_encode.h"
    "../include/m3c/type_traits.h"
    "../include/m3c/unique_ptr.h"
    )
add_library(common-cpp::m3c ALIAS m3c)

target_sources(m3c INTERFACE "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Log-config.cpp>>"
                             "$<$<NOT:$<IN_LIST:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY;OBJECT_LIBRARY;INTERFACE_LIBRARY>>:$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/m3c/Log-config.cpp>>")

target_compile_definitions(m3c PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX=1)
target_compile_features(m3c PUBLIC cxx_std_20)
target_precompile_headers(m3c PRIVATE "pch.h")
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/m3c.dir/cmake_pch.cxx" APPEND PROPERTY COMPILE_OPTIONS $<$<CONFIG:Release>:/Yl>)

target_include_directories(m3c PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

set_target_properties(m3c PROPERTIES
    DEBUG_POSTFIX d
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(m3c PUBLIC fmt::fmt PRIVATE rpcrt4 propsys)
common_cpp_target_events(m3c "m3c.events.man" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/m3c")

install(TARGETS m3c EXPORT m3c-targets)
install(EXPORT m3c-targets DESTINATION "share/${PROJECT_NAME}" NAMESPACE "${PROJECT_NAME}::")
install(DIRECTORY "../include/m3c" TYPE INCLUDE)
install(FILES "Log-config.cpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/m3c")
install(FILES "${PROJECT_SOURCE_DIR}/cmake/EventLog.cmake"
              "${PROJECT_SOURCE_DIR}/cmake/merge-events.vbs"
              "${PROJECT_SOURCE_DIR}/cmake/process-events.cmake"
        DESTINATION "share/${PROJECT_NAME}")
