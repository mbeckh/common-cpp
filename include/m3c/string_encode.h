/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file
#pragma once

#include <sal.h>

#include <cstddef>
#include <string>

namespace m3c {

/// @brief Convert a wide character string to a UTF-8 encoded string.
/// @param wstr The source string.
/// @return The source string in UTF-8 encoding.
std::string EncodeUtf8(_In_z_ const wchar_t* __restrict wstr);  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief Convert part of a wide character string to a UTF-8 encoded string.
/// @param wstr The source string.
/// @param length The number of characters to convert starting at @p wstr.
/// @return The first cchLen characters of the source string in UTF-8 encoding.
std::string EncodeUtf8(_In_reads_(length) const wchar_t* __restrict wstr, std::size_t length);  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

}  // namespace m3c
