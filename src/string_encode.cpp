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

#include "m3c_events.h"

#include "m3c/exception.h"

#include <windows.h>

#include <cassert>
#include <cwchar>
#include <limits>

namespace m3c {

std::string EncodeUtf8(_In_z_ const wchar_t* __restrict const wstr) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return EncodeUtf8(wstr, std::wcslen(wstr));
}

std::string EncodeUtf8(_In_reads_(length) const wchar_t* __restrict const wstr, const std::size_t length) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!length) [[unlikely]] {
		return "";
	}
	assert(length <= std::numeric_limits<int>::max());

	if (constexpr std::size_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		char result[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), result, sizeof(result), nullptr, nullptr);
		if (sizeInBytes) [[likely]] {
			return std::string(result, sizeInBytes);
		}
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) [[unlikely]] {
			throw windows_error(lastError) + evt::WideCharToMultiByte << length;
		}
	}

	const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
	if (sizeInBytes) [[likely]] {
		std::string result;
		result.resize(sizeInBytes);
		const int count = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), result.data(), sizeInBytes, nullptr, nullptr);
		if (count) [[likely]] {
			return result;
		}
	}
	throw windows_error(GetLastError()) + evt::WideCharToMultiByte << length;
}

}  // namespace m3c
