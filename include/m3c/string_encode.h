/*
Copyright 2019-2021 Michael Beckh

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
#include <string_view>

namespace m3c {

/// @brief Convert a wide character string to a UTF-8 encoded string.
/// @param str The source string.
/// @return The source string in UTF-8 encoding.
[[nodiscard]] std::string EncodeUtf8(_In_z_ const wchar_t* __restrict str);

/// @brief Convert a wide character string to a UTF-8 encoded string.
/// @param str The source string.
/// @return The source string in UTF-8 encoding.
[[nodiscard]] std::string EncodeUtf8(const std::wstring& str);

/// @brief Convert a wide character string view to a UTF-8 encoded string.
/// @param str The source string view.
/// @return The source string in UTF-8 encoding.
[[nodiscard]] std::string EncodeUtf8(const std::wstring_view& str);

/// @brief Convert part of a wide character string to a UTF-8 encoded string.
/// @param str The source string.
/// @param length The number of characters to convert starting at @p str.
/// @return The first cchLen characters of the source string in UTF-8 encoding.
[[nodiscard]] std::string EncodeUtf8(_In_reads_(length) const wchar_t* __restrict str, std::size_t length);


/// @brief Convert a UTF-8 encoded string to a UTF-16 encoded character string.
/// @param str The source string.
/// @return The source string in UITF-16 encoding.
[[nodiscard]] std::wstring EncodeUtf16(_In_z_ const char* __restrict str);

/// @brief Convert a UTF-8 encoded string to a UTF-16 encoded character string.
/// @param str The source string.
/// @return The source string in UITF-16 encoding.
[[nodiscard]] std::wstring EncodeUtf16(const std::string& str);

/// @brief Convert a UTF-8 encoded string view to a UTF-16 encoded character string.
/// @param str The source string view.
/// @return The source string in UITF-16 encoding.
[[nodiscard]] std::wstring EncodeUtf16(const std::string_view& str);

/// @brief Convert part of a UTF-8 encoded character string to a UTF-16 encoded string.
/// @param str The source string.
/// @param length The number of characters to convert starting at @p wstr.
/// @return The first cchLen characters of the source string in UTF-16 encoding.
[[nodiscard]] std::wstring EncodeUtf16(_In_reads_(length) const char* __restrict str, std::size_t length);

}  // namespace m3c
