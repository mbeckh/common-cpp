/*
/*
Copyright 2020 Michael Beckh

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

#include "m3c/LogData.h"
//#include "llamalog/custom_types.h"
#include <fmt/core.h>

#include <evntprov.h>
#include <sal.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>
#include <vector>

namespace m3c::internal::buffer {

/// @brief Marker type for a `nullptr` value for pointers.
/// @details Required as a separate type to ignore formatting placeholders. It is only used for (safe) type punning.
/// Therefore all constructors, destructors and assignment operators deleted.
// struct NullValue final {
//	NullValue() = delete;
// };

#if 0
/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct TriviallyCopyable final {
	TriviallyCopyable() = delete;
};

/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct NonTriviallyCopyable final {
	NonTriviallyCopyable() = delete;
};

#endif
/// @brief The types supported by the logger as arguments. Use `#kTypeId` to get the `#TypeId`.
/// @copyright This type is derived from `SupportedTypes` from NanoLog.
using Types = std::tuple<
	bool,
	char,
	wchar_t,
	signed char,
	unsigned char,
	signed short,
	unsigned short,
	signed int,
	unsigned int,
	signed long,
	unsigned long,
	signed long long,
	unsigned long long,
	float,
	double,
	const void*,     // MUST NOT cast this back to any object because the object might no longer exist when the message is logged
	const char*,     // string is stored WITHOUT a terminating null character
	const wchar_t*,  // string is stored WITHOUT a terminating null character
	GUID,
	FILETIME,
	SYSTEMTIME,
	SID,
	win32_error,
	rpc_status,
	hresult
#if 0
	,
	TriviallyCopyable,
	NonTriviallyCopyable
#endif
	>;

/// @brief Get `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
/// @tparam Types Supply `#Types` for this parameter.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename Types>
struct TypeIndex;

/// @brief Get `#TypeId` at compile time. @details Specialization for the final step in recursive evaluation.
/// @tparam T The type to get the id for.
/// @tparam Types The tuple of types.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename... Types>
struct TypeIndex<T, std::tuple<T, Types...>> {
	static constexpr std::uint8_t kValue = 0;  ///< The tuple index.
};

/// @brief Get `#TypeId` at compile time. @details Specialization for recursive evaluation.
/// @tparam T The type to get the id for.
/// @tparam U The next type required for recursive evaluation.
/// @tparam Types The remaining tuple of types.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename U, typename... Types>
struct TypeIndex<T, std::tuple<U, Types...>> {
	static constexpr std::uint8_t kValue = 1 + TypeIndex<T, std::tuple<Types...>>::kValue;  ///< The tuple index.
};

/// @brief Type of the type marker in the argument buffer.
using TypeId = std::uint8_t;
static_assert(std::tuple_size_v<Types> <= std::numeric_limits<TypeId>::max(), "too many types for type of TypeId");

/// @brief A constant to get the `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
inline constexpr TypeId kTypeId = TypeIndex<T, Types>::kValue;

/// @brief Pre-calculated array of sizes required to store values in the buffer. Use `#kTypeSize` to get the size in code.
/// @hideinitializer
constexpr std::uint8_t kTypeSizes[] = {
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(bool),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(char),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(wchar_t),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(signed char),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(unsigned char),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(signed short),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(unsigned short),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(signed int),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(unsigned int),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(signed long),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(unsigned long),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(signed long long),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(unsigned long long),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(float),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(double),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(void*),
	sizeof(TypeId) + sizeof(LogData::Length) /* + std::byte[padding] + char[std::strlen(str)] + 0x00 */,
	sizeof(TypeId) + sizeof(LogData::Length) /* + std::byte[padding] + wchar_t[std::wcslen(str)] + 0x00 0x00 */,
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(GUID),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(FILETIME),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(SYSTEMTIME),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(SID) /* + DWORD[n] */,
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(win32_error),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(rpc_status),
	sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(hresult)
#if 0
	,
	sizeof(TypeId) + sizeof(LogData::Align) + sizeof(void (*)()) + sizeof(LogData::Size) /* + std::byte[padding] + sizeof(arg) */,
	sizeof(TypeId) + sizeof(LogData::Align) + sizeof(internal::FunctionTable*) + sizeof(LogData::Size) /* + std::byte[padding] + sizeof(arg) */
#endif
};
// static_assert(sizeof(TypeId) + sizeof(LogData::Align) + sizeof(internal::FunctionTable::CreateFormatArg) + sizeof(LogData::Size) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
// static_assert(sizeof(TypeId) + sizeof(LogData::Align) + sizeof(internal::FunctionTable*) + sizeof(LogData::Size) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(std::tuple_size_v<Types> == sizeof(kTypeSizes) / sizeof(kTypeSizes[0]), "length of kTypeSizes does not match Types");

/// @brief A constant to get the (basic) buffer size of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
inline constexpr std::uint8_t kTypeSize = kTypeSizes[kTypeId<T>];


/// @brief The number of bytes to add to the argument buffer after it became too small.
constexpr LogData::Size kGrowBytes = 512u;

/// @brief Get the next allocation chunk, i.e. the next block which is a multiple of `#kGrowBytes`.
/// @param value The required size.
/// @return The value rounded up to multiples of `#kGrowBytes`.
constexpr __declspec(noalias) LogData::Size GetNextChunk(const LogData::Size value) noexcept {
	constexpr LogData::Size kMask = kGrowBytes - 1u;
	static_assert((kGrowBytes & kMask) == 0, "kGrowBytes must be a multiple of 2");

	return value + ((kGrowBytes - (value & kMask)) & kMask);
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @tparam T The type.
/// @param ptr The target address.
/// @return The padding to account for a properly aligned type.
template <typename T>
[[nodiscard]] __declspec(noalias) LogData::Align GetPadding(_In_ const std::byte* __restrict const ptr) noexcept {
	if constexpr (alignof(T) == 1) {
		return 0;
	} else {
		static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of type is too large");
		constexpr LogData::Align kMask = alignof(T) - 1u;
		return static_cast<LogData::Align>(alignof(T) - (reinterpret_cast<std::uintptr_t>(ptr) & kMask)) & kMask;
	}
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @param ptr The target address.
/// @param align The required alignment in number of bytes.
/// @return The padding to account for a properly aligned type.
[[nodiscard]] __declspec(noalias) LogData::Align GetPadding(_In_ const std::byte* __restrict ptr, LogData::Align align) noexcept;

/// @brief Read a value from the buffer.
/// @details Use std::memcpy to comply with strict aliasing and alignment rules.
/// @tparam T The target type to read.
/// @param buffer The source address to read from.
/// @return The read value.
template <typename T>
[[nodiscard]] __declspec(noalias) T GetValue(_In_ const std::byte* __restrict const buffer) noexcept requires(std::is_trivially_copyable_v<T>&& std::is_trivially_constructible_v<T> && (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>) ) {
	T result;
	std::memcpy(&result, buffer, sizeof(T));  // NOLINT(bugprone-sizeof-expression): Deliberately supporting sizeof pointers.
	return result;
}

/// @brief Copy arguments from a `LogInformation` buffer into a `std::vector`.
/// @param buffer The buffer holding the data.
/// @param used The number of valid bytes in @p buffer.
/// @param args The `std::vector` to receive the message arguments.
/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
template <typename T>
std::uint32_t CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict buffer, LogData::Size used, T& args);

extern template std::uint32_t CopyArgumentsFromBufferTo<std::vector<EVENT_DATA_DESCRIPTOR>>(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogData::Size used, std::vector<EVENT_DATA_DESCRIPTOR>& args);
extern template std::uint32_t CopyArgumentsFromBufferTo<fmt::dynamic_format_arg_store<fmt::format_context>>(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogData::Size used, fmt::dynamic_format_arg_store<fmt::format_context>& args);

/// @brief Copy all objects from one buffer to another.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict src, _Out_writes_bytes_(used) std::byte* __restrict dst, LogData::Size used);

/// @brief Move all objects from one buffer to another. @details The function also calls the moved-from's destructor.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict src, _Out_writes_bytes_(used) std::byte* __restrict dst, LogData::Size used) noexcept;

/// @brief Call all the destructors of non-trivially copyable custom arguments in a buffer.
/// @param buffer The buffer.
/// @param used The number of bytes used in the buffer.
void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogData::Size used) noexcept;

}  // namespace m3c::internal::buffer
