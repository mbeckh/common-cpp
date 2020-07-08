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

#include "m3c/string_encode.h"

#include "m3c/exception.h"

#include <llamalog/llamalog.h>

#include <windows.h>

#include <cassert>
#include <cstdint>
#include <cwchar>
#include <limits>

namespace m3c {

std::string EncodeUtf8(_In_z_ const wchar_t* __restrict const wstr) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return EncodeUtf8(wstr, std::wcslen(wstr));
}

std::string EncodeUtf8(_In_reads_(length) const wchar_t* __restrict const wstr, const std::size_t length) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!length) {
		return "";
	}
	assert(length <= std::numeric_limits<int>::max());

	DWORD lastError;  // NOLINT(cppcoreguidelines-init-variables): Guaranteed to be initialized before first read.
	if (constexpr std::uint_fast16_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		char result[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), result, sizeof(result), nullptr, nullptr);
		if (sizeInBytes) {
			return std::string(result, sizeInBytes);
		}
		lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			goto error;  // NOLINT(cppcoreguidelines-avoid-goto, hicpp-avoid-goto): Yes, I DO want a goto here.
		}
	}
	{
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
		if (sizeInBytes) {
			std::string result;
			result.resize(sizeInBytes);
			const int count = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), result.data(), sizeInBytes, nullptr, nullptr);
			if (count) {
				return result;
			}
		}
		lastError = GetLastError();
	}

error:
	THROW(windows_exception(lastError), "WideCharToMultiByte for length {}", length);
}

}  // namespace m3c
