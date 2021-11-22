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
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project("common-cpp" VERSION 0.1.2
    DESCRIPTION "Common C++ library"
    HOMEPAGE_URL "https://github.com/mbeckh/common-cpp"
    LANGUAGES CXX)

include(GNUInstallDirs)

add_subdirectory(src)

include(CMakePackageConfigHelpers)
configure_package_config_file("cmake/common-cpp-config.cmake.in" "common-cpp-config.cmake" INSTALL_DESTINATION "share/common-cpp")
write_basic_package_version_file("common-cpp-config-version.cmake" VERSION ${common-cpp_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_BINARY_DIR}/common-cpp-config.cmake" "${CMAKE_BINARY_DIR}/common-cpp-config-version.cmake"
        DESTINATION "share/common-cpp")
install(EXPORT m3c-targets DESTINATION "share/common-cpp" NAMESPACE "common-cpp::")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
