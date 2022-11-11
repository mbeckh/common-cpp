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

project("common-cpp" VERSION 0.1.3
    DESCRIPTION "Common C++ library"
    HOMEPAGE_URL "https://github.com/mbeckh/common-cpp"
    LANGUAGES CXX)

include(GNUInstallDirs)

add_subdirectory(src)

include(CMakePackageConfigHelpers)
configure_package_config_file("cmake/${PROJECT_NAME}-config.cmake.in" "${PROJECT_NAME}-config.cmake" INSTALL_DESTINATION "share/${PROJECT_NAME}")
write_basic_package_version_file("${PROJECT_NAME}-config-version.cmake" VERSION ${${PROJECT_NAME}_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config.cmake" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
        DESTINATION "share/${PROJECT_NAME}")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
