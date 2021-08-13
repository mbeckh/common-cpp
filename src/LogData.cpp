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
/// @copyright Code marked with "from NanoLog" is based on NanoLog (https://github.com/Iyengar111/NanoLog, commit
/// 40a53c36e0336af45f7664abeb939f220f78273e), copyright 2016 Karthik Iyengar and distributed under the MIT License.

/*

Distributed under the MIT License (MIT)

    Copyright (c) 2016 Karthik Iyengar

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "m3c/LogData.h"

#include "buffer_management.h"
#include "m3c_events.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <fmt/core.h>
//#include "llamalog/custom_types.h"
//#include "llamalog/exception.h"
#include "m3c/format.h"

//#include <fmt/format.h>

#include <windows.h>
#include <evntprov.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <new>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace m3c::internal {

using buffer::CallDestructors;
using buffer::CopyArgumentsFromBufferTo;
using buffer::CopyObjects;
using buffer::GetNextChunk;
using buffer::GetPadding;
using buffer::kTypeId;
using buffer::kTypeSize;
using buffer::MoveObjects;
using buffer::TypeId;

// using buffer::NullValue;
#if 0
using buffer::NonTriviallyCopyable;
using buffer::TriviallyCopyable;
#endif

//
// Definitions
//

namespace {

static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= std::numeric_limits<LogData::Align>::max(), "type LogLine::Align is too small");

#if 0
/// @brief Get information for `std::error_code`.
/// @note The function MUST be called from within a catch block to get the object, else `nullptr` is returned.
/// @return A pointer which is set if the exception is of type `std::system_error` or `system_error` (else `nullptr`).
[[nodiscard]] _Ret_maybenull_ const std::error_code* GetCurrentExceptionCode() noexcept {
	try {
		throw;
	} catch (const system_error& e) {
		return &e.code();  // using address is valid because code() returns reference
	} catch (const std::system_error& e) {
		return &e.code();  // using address is valid because code() returns reference
	} catch (...) {
		return nullptr;
	}
}
#endif

}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogData::LogData(const LogData& other)
	: m_hasNonTriviallyCopyable(other.m_hasNonTriviallyCopyable)
	, m_used(other.m_used)
	, m_size(other.m_size) {
	if (other.m_heapBuffer) [[unlikely]] {
		m_heapBuffer = std::make_unique<std::byte[]>(m_used);
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			CopyObjects(other.m_heapBuffer.get(), m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), other.m_heapBuffer.get(), m_used);
		}
	} else {
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			CopyObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogData::LogData(LogData&& other) noexcept
	: m_hasNonTriviallyCopyable(other.m_hasNonTriviallyCopyable)
	, m_used(other.m_used)
	, m_size(other.m_size)
	, m_heapBuffer(std::move(other.m_heapBuffer)) {
	if (!m_heapBuffer) [[likely]] {
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			MoveObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
	// leave source in a consistent state
	other.m_used = 0;
	other.m_size = sizeof(m_stackBuffer);
}

LogData::~LogData() noexcept {
	// ensure proper memory layout
	static_assert(sizeof(LogData) == M3C_LOGDATA_SIZE, "size of LogLine");
	static_assert(offsetof(LogData, m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0, "alignment of LogLine::m_stackBuffer");

	static_assert(offsetof(LogData, m_stackBuffer) == 0, "offset of m_stackBuffer");
#if UINTPTR_MAX == UINT64_MAX
	static_assert(offsetof(LogData, m_hasNonTriviallyCopyable) == M3C_LOGDATA_SIZE - 17, "offset of m_hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_used) == M3C_LOGDATA_SIZE - 16, "offset of m_used");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_size) == M3C_LOGDATA_SIZE - 12, "offset of m_size");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_heapBuffer) == M3C_LOGDATA_SIZE - 8, "offset of m_heapBuffer");                             // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.

#elif UINTPTR_MAX == UINT32_MAX
	static_assert(offsetof(LogData, m_hasNonTriviallyCopyable) == M3C_LOGDATA_SIZE - 13, "offset of m_hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_used) == M3C_LOGDATA_SIZE - 12, "offset of m_used");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_size) == M3C_LOGDATA_SIZE - 8, "offset of m_size");                                         // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogData, m_heapBuffer) == M3C_LOGDATA_SIZE - 4, "offset of m_heapBuffer");                             // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
#else
	static_assert(false, "layout assertions not defined");
#endif

	if (m_hasNonTriviallyCopyable) [[unlikely]] {
		CallDestructors(GetBuffer(), m_used);
	}
}

// NOLINTNEXTLINE(bugprone-unhandled-self-assignment, cert-oop54-cpp): Assert that type is never assigned to itself.
LogData& LogData::operator=(const LogData& other) {
	assert(&other != this);

	m_hasNonTriviallyCopyable = other.m_hasNonTriviallyCopyable;
	m_used = other.m_used;
	m_size = other.m_size;
	if (other.m_heapBuffer) [[unlikely]] {
		m_heapBuffer = std::make_unique<std::byte[]>(m_used);
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			CopyObjects(other.m_heapBuffer.get(), m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), other.m_heapBuffer.get(), m_used);
		}
	} else {
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			CopyObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
	return *this;
}

LogData& LogData::operator=(LogData&& other) noexcept {
	m_hasNonTriviallyCopyable = other.m_hasNonTriviallyCopyable;
	m_used = other.m_used;
	m_size = other.m_size;
	m_heapBuffer = std::move(other.m_heapBuffer);
	if (!m_heapBuffer) [[likely]] {
		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			MoveObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
	// leave source in a consistent state
	other.m_used = 0;
	other.m_size = sizeof(m_stackBuffer);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogData& LogData::operator<<(const bool arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogData& LogData::operator<<(const char arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogData& LogData::operator<<(const wchar_t arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogData& LogData::operator<<(const signed char arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogData& LogData::operator<<(const unsigned char arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogData& LogData::operator<<(const signed short arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogData& LogData::operator<<(const unsigned short arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogData& LogData::operator<<(const signed int arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogData& LogData::operator<<(const unsigned int arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogData& LogData::operator<<(const signed long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogData& LogData::operator<<(const unsigned long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
LogData& LogData::operator<<(const signed long long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogData& LogData::operator<<(const unsigned long long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogData& LogData::operator<<(const float arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogData& LogData::operator<<(const double arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogData& LogData::operator<<(_In_opt_ const void* __restrict const arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogData& LogData::operator<<(std::nullptr_t /* arg */) {
	Write<const void*>(nullptr);
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(_In_z_ const char* __restrict const arg) {
	WriteString(arg, std::strlen(arg));
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(_In_z_ const wchar_t* __restrict const arg) {
	WriteString(arg, std::wcslen(arg));
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(const std::string& arg) {
	WriteString(arg.c_str(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(const std::wstring& arg) {
	WriteString(arg.c_str(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(const std::string_view& arg) {
	WriteString(arg.data(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogData& LogData::operator<<(const std::wstring_view& arg) {
	WriteString(arg.data(), arg.length());
	return *this;
}

LogData& LogData::operator<<(const GUID& arg) {
	Write(arg);
	return *this;
}

LogData& LogData::operator<<(const FILETIME& arg) {
	Write(arg);
	return *this;
}

LogData& LogData::operator<<(const SYSTEMTIME& arg) {
	Write(arg);
	return *this;
}

LogData& LogData::operator<<(const SID& arg) {
	WriteSID(arg);
	return *this;
}

LogData& LogData::operator<<(const win32_error& arg) {
	Write(arg);
	return *this;
}

LogData& LogData::operator<<(const rpc_status& arg) {
	Write(arg);
	return *this;
}

LogData& LogData::operator<<(const hresult& arg) {
	Write(arg);
	return *this;
}

/// @brief The single specialization of `CopyArgumentsTo`.
/// @param args The `std::vector` to receive the message arguments.
template <>
std::uint32_t LogData::CopyArgumentsTo(std::vector<EVENT_DATA_DESCRIPTOR>& args) const {
	return CopyArgumentsFromBufferTo(GetBuffer(), m_used, args);
}

template <>
std::uint32_t LogData::CopyArgumentsTo(fmt::dynamic_format_arg_store<fmt::format_context>& args) const {
	return CopyArgumentsFromBufferTo(GetBuffer(), m_used, args);
}


// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogData::GetBuffer() noexcept {
	return !m_heapBuffer ? m_stackBuffer : m_heapBuffer.get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) const std::byte* LogData::GetBuffer() const noexcept {
	return !m_heapBuffer ? m_stackBuffer : m_heapBuffer.get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogData::GetWritePosition(const LogData::Size additionalBytes) {
	const std::size_t requiredSize = static_cast<std::size_t>(m_used) + additionalBytes;
	if (requiredSize > std::numeric_limits<LogData::Size>::max()) [[unlikely]] {
		throw std::length_error("LogData buffer") + evt::LogData_GetWritePosition << additionalBytes << requiredSize;
	}

	if (requiredSize <= m_size) {
		return !m_heapBuffer ? &m_stackBuffer[m_used] : &(m_heapBuffer.get())[m_used];
	}

	m_size = GetNextChunk(static_cast<std::uint32_t>(requiredSize));
	if (!m_heapBuffer) [[likely]] {
		m_heapBuffer = std::make_unique_for_overwrite<std::byte[]>(m_size);

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			MoveObjects(m_stackBuffer, m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), m_stackBuffer, m_used);
		}
	} else {
		std::unique_ptr<std::byte[]> newHeapBuffer(std::make_unique_for_overwrite<std::byte[]>(m_size));

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(newHeapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) [[unlikely]] {
			MoveObjects(m_heapBuffer.get(), newHeapBuffer.get(), m_used);
		} else {
			std::memcpy(newHeapBuffer.get(), m_heapBuffer.get(), m_used);
		}
		m_heapBuffer = std::move(newHeapBuffer);
	}
	return &(m_heapBuffer.get())[m_used];
}

// Derived from both methods `NanoLogLine::encode` from NanoLog.
template <typename T>
void LogData::Write(const T arg) {
	constexpr TypeId typeId = kTypeId<T>;
	constexpr auto kArgSize = kTypeSize<T>;

	std::byte* __restrict buffer = GetWritePosition(kArgSize);
	const LogData::Align padding = GetPadding<T>(&buffer[sizeof(typeId)]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(kArgSize + padding);
	}

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId) + padding], &arg, sizeof(arg));

	m_used += kArgSize + padding;
}

#if 0
template <typename T>
void LogData::WritePointer(const T* const arg) {
	if (arg) {
		const TypeId typeId = GetTypeId<T, true>();
		constexpr auto kArgSize = kTypeSize<T>;

		std::byte* __restrict buffer = GetWritePosition(kArgSize);
		const LogData::Align padding = GetPadding<T>(&buffer[sizeof(typeId)]);
		if (padding) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(kArgSize + padding);
		}

		std::memcpy(buffer, &typeId, sizeof(typeId));
		std::memcpy(&buffer[sizeof(typeId)] + padding, arg, sizeof(*arg));

		m_used += kArgSize + padding;
	} else {
		WriteNullPointer();
	}
}

void LogData::WriteNullPointer() {
	constexpr TypeId typeId = kTypeId<std::nullptr_t>;
	constexpr auto kArgSize = kTypeSize<std::nullptr_t>;
	std::byte* __restrict const buffer = GetWritePosition(kArgSize);

	std::memcpy(buffer, &typeId, sizeof(typeId));

	m_used += kArgSize;
}

#endif

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
#if 0
template <typename T>
void LogData::WriteString(_In_reads_(len) const T* __restrict const arg, const std::size_t len) {
	const TypeId typeId = kTypeId<const T*>;
	constexpr auto kArgSize = kTypeSize<const T*>;
	const LogData::Length length = static_cast<LogData::Length>(std::min<std::size_t>(len, std::numeric_limits<LogData::Length>::max()));
	if (length < len) {
		//LLAMALOG_INTERNAL_WARN("String of length {} trimmed to {}", len, length);
	}
	const LogData::Size size = kArgSize + length * sizeof(T);

	std::byte* __restrict buffer = GetWritePosition(size);
	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize], arg, length * sizeof(char));

	m_used += size;
}
#endif

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
template <typename T>
void LogData::WriteString(_In_reads_(len) const T* __restrict const arg, const std::size_t len) {
	constexpr TypeId typeId = kTypeId<const T*>;
	constexpr auto kArgSize = kTypeSize<const T*>;
	const LogData::Length length = static_cast<LogData::Length>(std::min<std::size_t>(len, std::numeric_limits<LogData::Length>::max()));
	if (length < len) [[unlikely]] {
		Log::Warning(evt::LogData_WriteString, len, length);
	}
	const LogData::Size size = kArgSize + (length + 1) * sizeof(T);

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogData::Align padding = GetPadding<T>(&buffer[kArgSize]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize + padding], arg, length * sizeof(T));
	std::memset(&buffer[kArgSize + padding + length * sizeof(T)], 0, sizeof(T));

	m_used += size + padding;
}

void LogData::WriteSID(const SID& arg) {
	constexpr TypeId typeId = kTypeId<SID>;
	constexpr auto kArgSize = kTypeSize<SID>;
	const LogData::Size add = arg.SubAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);
	const LogData::Size size = kArgSize + add;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogData::Align padding = GetPadding<SID>(&buffer[sizeof(typeId)]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId) + padding], &arg, sizeof(SID) + add);

	m_used += size + padding;
}

#if 0
void LogData::WriteTriviallyCopyable(_In_reads_bytes_(objectSize) const std::byte* __restrict const ptr, const LogData::Size objectSize, const LogData::Align align, _In_ void (*const format)()) {
	constexpr TypeId typeId = kTypeId<TriviallyCopyable>;
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;
	const LogData::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogData::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding)], &format, sizeof(format));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding) + sizeof(format)], &objectSize, sizeof(objectSize));
	std::memcpy(&buffer[kArgSize + padding], ptr, objectSize);

	m_used += size + padding;
}
#endif
#if 0
__declspec(restrict) std::byte* LogLine::WriteNonTriviallyCopyable(const LogLine::Size objectSize, const LogLine::Align align, _In_ const void* const functionTable) {
	static_assert(sizeof(functionTable) == sizeof(internal::FunctionTable*));

	const TypeId typeId = GetTypeId<NonTriviallyCopyable>(m_escape);
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;
	const LogLine::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding != 0) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding)], &functionTable, sizeof(functionTable));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding) + sizeof(functionTable)], &objectSize, sizeof(objectSize));
	std::byte* result = &buffer[kArgSize + padding];

	m_hasNonTriviallyCopyable = true;
	m_used += size + padding;

	return result;
}
#endif

}  // namespace m3c::internal

#if 0
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const std::align_val_t arg) {
	static_assert(
		(std::is_signed_v<std::underlying_type_t<std::align_val_t>> && (sizeof(std::align_val_t) == sizeof(std::int64_t) || sizeof(std::align_val_t) == sizeof(std::int32_t)))
			|| (!std::is_signed_v<std::underlying_type_t<std::align_val_t>> && (sizeof(std::align_val_t) == sizeof(std::uint64_t) || sizeof(std::align_val_t) == sizeof(std::uint32_t))),
		"cannot match type of std::align_val_t");

	if constexpr (std::is_signed_v<std::underlying_type_t<std::align_val_t>>) {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::int64_t)) {
			return logLine << static_cast<std::int64_t>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::int32_t)) {
			return logLine << static_cast<std::int32_t>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	} else {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::uint64_t)) {
			return logLine << static_cast<std::uint64_t>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::uint32_t)) {
			return logLine << static_cast<std::uint32_t>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	}
}

/// @brief Operator for printing pointers to std::align_val_t values.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The argument.
/// @return The @p logLine for method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const std::align_val_t* const arg) {
	static_assert(
		(std::is_signed_v<std::underlying_type_t<std::align_val_t>> && ((sizeof(std::align_val_t) == sizeof(std::int64_t) && alignof(std::align_val_t) == alignof(std::int64_t))                 // NOLINT(misc-redundant-expression): Paranoid static-assert.
																		|| (sizeof(std::align_val_t) == sizeof(std::int32_t) && alignof(std::align_val_t) == alignof(std::int32_t)))             // NOLINT(misc-redundant-expression): Paranoid static-assert.
		 ) || (!std::is_signed_v<std::underlying_type_t<std::align_val_t>> && ((sizeof(std::align_val_t) == sizeof(std::uint64_t) && alignof(std::align_val_t) == alignof(std::uint64_t))        // NOLINT(misc-redundant-expression): Paranoid static-assert.
																			   || (sizeof(std::align_val_t) == sizeof(std::uint32_t) && alignof(std::align_val_t) == alignof(std::uint32_t)))),  // NOLINT(misc-redundant-expression): Paranoid static-assert.
		"cannot match type of std::align_val_t");

	if constexpr (std::is_signed_v<std::underlying_type_t<std::align_val_t>>) {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::int64_t)) {
			return logLine << reinterpret_cast<const std::int64_t*>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::int32_t)) {
			return logLine << reinterpret_cast<const std::int32_t*>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	} else {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::uint64_t)) {
			return logLine << reinterpret_cast<const std::uint64_t*>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::uint32_t)) {
			return logLine << reinterpret_cast<const std::uint32_t*>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	}
}

#endif
