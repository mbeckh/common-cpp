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

include_guard(GLOBAL)

function(z_common_cpp_target_events var target)
    get_target_property(library "${target}" TYPE)
    string(REGEX MATCH [[^(STATIC_LIBRARY|OBJECT_LIBRARY|INTERFACE_LIBRARY)$]] library "${library}")
    get_target_property(dependencies "${target}" LINK_LIBRARIES)
    get_target_property(interface_dependencies "${target}" INTERFACE_LINK_LIBRARIES)
    set(result "${${var}}")
    foreach(dependency IN LISTS dependencies)
        if (TARGET "${dependency}")
            get_target_property(interface_sources "${dependency}" INTERFACE_SOURCES)
            list(FILTER interface_sources INCLUDE REGEX [[\.man([>;]|$)]])
            if(interface_sources)
                if(library)
                    # libraries with event manifests MUST be PUBLIC INTERFACE_LINK_LIBRARIES to make use of transitive INTERFACE_SOURCES
                    list(FIND interface_dependencies "${dependency}" interface)
                    if(interface EQUAL -1)
                        message(FATAL_ERROR "Dependency ${dependency} of target ${target} MUST be PUBLIC to provide event manifests ${interface_sources}")
                    endif()
                endif()

                list(APPEND result "${interface_sources}")
            endif()
            z_common_cpp_target_events(result "${dependency}")
        endif()
    endforeach()
    set("${var}" "${result}" PARENT_SCOPE)
endfunction()

function(common_cpp_target_events target events #[[ [ DESTINATION <path> ] [ LEVEL <level> ] [ PRINT ] [ EVENT ] ]])
    cmake_parse_arguments(PARSE_ARGV 2 arg "PRINT;EVENT" "DESTINATION;LEVEL" "")

    get_target_property(library "${target}" TYPE)
    string(REGEX MATCH [[^(STATIC_LIBRARY|OBJECT_LIBRARY|INTERFACE_LIBRARY)$]] library "${library}")
    get_target_property(src_dir "${target}" SOURCE_DIR)
    get_target_property(bin_dir "${target}" BINARY_DIR)

    if(library)
        if(NOT arg_DESTINATION)
            message(FATAL_ERROR "common_cpp_target_events requires DESTINATION for link-time dependency ${target}")
        endif()
        if(arg_LEVEL OR arg_PRINT OR arg_EVENT)
            message(FATAL_ERROR "common_cpp_target_events does not support LEVEL, PRINT or EVENT for link-time dependency ${target}")
        endif()
    else()
        if(arg_DESTINATION)
            message(FATAL_ERROR "common_cpp_target_events does not support DESTINATION for binary or shared library ${target}")
        endif()
    endif()

    z_common_cpp_target_events(includes "${target}")
    list(REMOVE_DUPLICATES includes)

    if(IS_ABSOLUTE "${events}")
        cmake_path(RELATIVE_PATH events BASE_DIRECTORY "${src_dir}" OUTPUT_VARIABLE file)
    else()
        set(file "${events}")
    endif()
    cmake_path(APPEND bin_dir "${target}.events")

    cmake_path(REPLACE_EXTENSION file LAST_ONLY "rc" OUTPUT_VARIABLE rc_file)
    cmake_path(REPLACE_EXTENSION file LAST_ONLY "th" OUTPUT_VARIABLE htmp_file)
    cmake_path(REPLACE_EXTENSION file LAST_ONLY "h" OUTPUT_VARIABLE h_file)
    cmake_path(REPLACE_EXTENSION file LAST_ONLY "cpp" OUTPUT_VARIABLE cpp_file)
    cmake_path(REPLACE_EXTENSION file LAST_ONLY "man" OUTPUT_VARIABLE man_file)

    cmake_path(ABSOLUTE_PATH rc_file BASE_DIRECTORY "${bin_dir}" OUTPUT_VARIABLE rc_file)
    cmake_path(ABSOLUTE_PATH htmp_file BASE_DIRECTORY "${bin_dir}" OUTPUT_VARIABLE htmp_file)
    cmake_path(ABSOLUTE_PATH h_file BASE_DIRECTORY "${bin_dir}" OUTPUT_VARIABLE h_file)
    cmake_path(ABSOLUTE_PATH cpp_file BASE_DIRECTORY "${bin_dir}" OUTPUT_VARIABLE cpp_file)
    cmake_path(ABSOLUTE_PATH man_file BASE_DIRECTORY "${bin_dir}" OUTPUT_VARIABLE man_file)

    cmake_path(GET file PARENT_PATH parent)
    if(parent)
        cmake_path(APPEND bin_dir "${parent}")
    endif()

    add_custom_command(OUTPUT "${man_file}"
        COMMAND "cscript" "//Nologo" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/merge-events.vbs" "${src_dir}/${file}" "${man_file}" ${includes}
        DEPENDS "${src_dir}/${file}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/merge-events.vbs" ${includes}
        COMMAND_EXPAND_LISTS)

    add_custom_command(OUTPUT "${cpp_file}"
        COMMAND "${CMAKE_COMMAND}" "-DMODE=MAP" "-DFILE=${man_file}" "-DOUT=${cpp_file}" -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/process-events.cmake"
        DEPENDS "${man_file}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/process-events.cmake")

    add_custom_command(OUTPUT "${rc_file}" "${htmp_file}"
        COMMAND mc -e th -h "${bin_dir}" -n -r "${bin_dir}" "${man_file}" 
        DEPENDS "${man_file}")

    add_custom_command(OUTPUT "${h_file}"
        COMMAND "${CMAKE_COMMAND}" "-DMODE=HEADER" "-DFILE=${htmp_file}" "-DOUT=${h_file}" -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/process-events.cmake"
        DEPENDS "${htmp_file}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/process-events.cmake")

    target_sources("${target}" PRIVATE "${h_file}" "${man_file}" "${events}")
    if(library)
        cmake_path(GET file FILENAME filename)
        target_sources("${target}" INTERFACE "$<BUILD_INTERFACE:${src_dir}/${file}>" "$<INSTALL_INTERFACE:${arg_DESTINATION}/${filename}>")
        install(FILES "${src_dir}/${file}" DESTINATION "${arg_DESTINATION}")
    else()
        target_sources("${target}" PRIVATE "${rc_file}" "${cpp_file}")
        # Setting OBJECT_DEPENDS _should not_ be required, but actually is as of CMake 3.20
        set_source_files_properties("${rc_file}" PROPERTIES OBJECT_DEPENDS "${man_file}")
        install(FILES "${man_file}" TYPE DATA)
    endif()

    target_include_directories("${target}" SYSTEM PRIVATE "${bin_dir}")
    string(TOUPPER "${arg_LEVEL}" arg_LEVEL)
    if(arg_LEVEL STREQUAL "CRITICAL")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Critical)
    elseif(arg_LEVEL STREQUAL "ERROR")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Error)
    elseif(arg_LEVEL STREQUAL "WARNING")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Warning)
    elseif(arg_LEVEL STREQUAL "INFO")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Info)
    elseif(arg_LEVEL STREQUAL "VERBOSE")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Verbose)
    elseif(arg_LEVEL STREQUAL "DEBUG")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Debug)
    elseif(arg_LEVEL STREQUAL "TRACE")
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Trace)
    else()
        target_compile_definitions("${target}" PRIVATE M3C_LOG_LEVEL=Info)
    endif()
    if(arg_PRINT)
        target_compile_definitions("${target}" PRIVATE M3C_LOG_OUTPUT_PRINT=1)
    endif()
    if(arg_EVENT)
        target_compile_definitions("${target}" PRIVATE M3C_LOG_OUTPUT_EVENT=1)
        target_link_libraries("${target}" PRIVATE wevtapi)
    endif()
endfunction()
