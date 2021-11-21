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

#include "m3c/string_encode.h"

#include "m3c/exception.h"

#include "m3c.events.h"

#include <windows.h>

#include <cassert>
#include <limits>
#include <string>

namespace m3c {

std::string EncodeUtf8(_In_z_ const wchar_t* __restrict const str) {
	return EncodeUtf8(str, std::char_traits<wchar_t>::length(str));
}

std::string EncodeUtf8(const std::wstring& str) {
	return EncodeUtf8(str.c_str(), str.size());
}

std::string EncodeUtf8(const std::wstring_view& str) {
	return EncodeUtf8(str.data(), str.size());
}

std::string EncodeUtf8(_In_reads_(length) const wchar_t* __restrict const str, const std::size_t length) {
	if (!length) {
		[[unlikely]];
		return "";
	}
	assert(length <= std::numeric_limits<int>::max());

	if (constexpr std::size_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		char result[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, static_cast<int>(length), result, sizeof(result), nullptr, nullptr);
		if (sizeInBytes) {
			[[likely]];
			return std::string(result, sizeInBytes);
		}
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			[[unlikely]];
			throw windows_error(lastError) + evt::Encode_E << length;
		}
	}

	const int sizeInBytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
	if (sizeInBytes) {
		[[likely]];
		std::string result;
		result.resize(sizeInBytes);
		const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str, static_cast<int>(length), result.data(), sizeInBytes, nullptr, nullptr);
		if (count) {
			[[likely]];
			return result;
		}
	}
	throw windows_error() + evt::Encode_E << length;
}


std::wstring EncodeUtf16(_In_z_ const char* __restrict const str) {
	return EncodeUtf16(str, std::char_traits<char>::length(str));
}

std::wstring EncodeUtf16(const std::string& str) {
	return EncodeUtf16(str.c_str(), str.size());
}

std::wstring EncodeUtf16(const std::string_view& str) {
	return EncodeUtf16(str.data(), str.size());
}

std::wstring EncodeUtf16(_In_reads_(length) const char* __restrict const str, const std::size_t length) {
	if (!length) {
		[[unlikely]];
		return L"";
	}
	assert(length <= std::numeric_limits<int>::max());

	if (constexpr std::size_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		wchar_t result[kFixedBufferSize];
		const int sizeInChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, static_cast<int>(length), result, kFixedBufferSize);
		if (sizeInChars) {
			[[likely]];
			return std::wstring(result, sizeInChars);
		}
		const DWORD lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			[[unlikely]];
			throw windows_error(lastError) + evt::Encode_E << length;
		}
	}

	const int sizeInChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, static_cast<int>(length), nullptr, 0);
	if (sizeInChars) {
		[[likely]];
		std::wstring result;
		result.resize(sizeInChars);
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, static_cast<int>(length), result.data(), sizeInChars);
		if (count) {
			[[likely]];
			return result;
		}
	}
	throw windows_error() + evt::Encode_E << length;
}

}  // namespace m3c
