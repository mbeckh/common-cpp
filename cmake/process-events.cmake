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

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

file(READ "${FILE}" content)

if("${MODE}" STREQUAL "HEADER")
    string(REPLACE [[#pragma once]] [[
#pragma once

#include <windows.h>
#include <evntprov.h>
#include <guiddef.h>

#include <map>

namespace m3c {
]] content "${content}")

    string(REPLACE [[EXTERN_C __declspec(selectany) const]] [[constexpr]] content "${content}")
    string(REGEX REPLACE "(\n(//[^\n]*\n)+)(constexpr EVENT_DESCRIPTOR )" [[\n}  // namespace m3c\n\1@INSERT@\n\3]] content "${content}")
    string(REGEX MATCHALL "constexpr EVENT_DESCRIPTOR [^;]+" matches "${content}")
    string(REGEX REPLACE "constexpr EVENT_DESCRIPTOR [^\n]+\n" "" content "${content}")
    while(matches)
        list(GET matches 0 event)
        if(NOT event MATCHES "(constexpr EVENT_DESCRIPTOR )([a-zA-Z0-9]+)_")
            string(REGEX REPLACE "constexpr EVENT_DESCRIPTOR ([a-zA-Z0-9_]+).*\$" "\\1" event "${event}")
            message(FATAL_ERROR "Event requires namespace: ${event}")
        endif()
        set(namespace "${CMAKE_MATCH_2}")
        set(copy "${matches}")
        list(FILTER copy INCLUDE REGEX "^constexpr EVENT_DESCRIPTOR ${namespace}_")
        list(REMOVE_ITEM matches ${copy})

        list(TRANSFORM copy REPLACE "^(constexpr EVENT_DESCRIPTOR )${namespace}_" "\\1")
        list(JOIN copy ";\n" copy)
        string(REPLACE [[@INSERT@]] "namespace ${namespace}::evt {\n${copy};\n}  // namespace ${namespace}::evt\n\n@INSERT@" content "${content}")
    endwhile()
    string(REPLACE "@INSERT@\n" "" content "${content}")
    string(APPEND content [[

namespace m3c::internal {
extern const std::map<USHORT, DWORD> kEventMessages;
extern const std::map<UCHAR, DWORD> kLevelNames;
}  // namespace m3c::internal
]])
elseif("${MODE}" STREQUAL "MAP")
    function(create_message_map element map type var)
        string(CONFIGURE [[<[ \r\n\t]*@element@[ \r\n\t]+[^>]*>]] pattern @ONLY)
        string(REGEX MATCHALL "${pattern}" matches "${content}" )

        unset(result)
        foreach(match IN LISTS matches)
            string(CONFIGURE "^<[ \r\n\t]*@element@([ \r\n\t]+[a-z0-9_-]+=\"[^\">]*\")*[ \r\n\t]+value=\"([^\">]*)\"([ \r\n\t]+[a-z0-9_-]+=\"[^\">]*\")*[ \r\n\t]*/?[ \r\n\t]*>\$" pattern @ONLY)
            string(REGEX MATCH "${pattern}" id "${match}" )
            set(id "${CMAKE_MATCH_2}")
            string(CONFIGURE "^<[ \r\n\t]*@element@([ \r\n\t]+[a-z0-9_-]+=\"[^\">]*\")*[ \r\n\t]+message=\"([^\">]*)\"([ \r\n\t]+[a-z0-9_-]+=\"[^\">]*\")*[ \r\n\t]*/?[ \r\n\t]*>\$" pattern @ONLY)
            string(REGEX MATCH "${pattern}" message "${match}" )
            set(message "${CMAKE_MATCH_2}")

            if(message MATCHES [[\$\([Ss]tring\.(.*)\)$]])
                set(message "${CMAKE_MATCH_1}")
            else()
                message(FATAL_ERROR "Unknown message id: ${message}")
            endif()

            string(MAKE_C_IDENTIFIER "${message}" message)

            list(APPEND result "    { ${id}, MSG_${message} }")
        endforeach()
        list(JOIN result ",\n" result)
        string(PREPEND result "const std::map<${type}, DWORD> ${map} = {\n")
        string(APPEND result "\n};\n")
        set("${var}" "${result}" PARENT_SCOPE)
    endfunction()

    create_message_map(event kEventMessages USHORT eventMap)
    create_message_map(level kLevelNames UCHAR levelMap)

    cmake_path(REPLACE_EXTENSION OUT LAST_ONLY "h" OUTPUT_VARIABLE include)
    cmake_path(GET OUT PARENT_PATH path)
    cmake_path(RELATIVE_PATH include BASE_DIRECTORY "${path}")

    string(CONFIGURE [[
#include "@include@"

#include <windows.h>

#include <map>

namespace m3c::internal {

@eventMap@

@levelMap@

}  // namespace m3c::internal
]] content @ONLY)
else()
    message(FATAL_ERROR "Unknown mode: ${MODE}")
endif()

file(WRITE "${OUT}" "${content}")
