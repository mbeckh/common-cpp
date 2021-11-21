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

#include "m3c/Log.h"
#include "m3c/LogArgs.h"
#include "m3c/PropVariant.h"
#include "m3c/exception.h"
#include "m3c/format.h"
#include "m3c/string_encode.h"
#include "m3c/type_traits.h"

#include "m3c.events.h"

#include <windows.h>
#include <oleauto.h>
#include <propvarutil.h>
#include <wtypes.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace m3c::internal {
//
// Definitions
//

namespace {

using logdata::Align;
using logdata::Length;
using logdata::Size;

using logdata::FunctionTable;
using logdata::FunctionTableNonTrivial;
using logdata::FunctionTableNoThrowConstructible;

static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= std::numeric_limits<Align>::max(), "type LogLine::Align is too small");

/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct TriviallyCopyable final {
	TriviallyCopyable() = delete;
};

/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct NonTriviallyCopyableNoThrowConstructible final {
	NonTriviallyCopyableNoThrowConstructible() = delete;
};

struct NonTriviallyCopyable final {
	NonTriviallyCopyable() = delete;
};

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
    long double,
    const void*,     // MUST NOT cast this back to any object because the object might no longer exist when the message is logged
    const char*,     // string is stored WITHOUT a terminating null character
    const wchar_t*,  // string is stored WITHOUT a terminating null character
    GUID,
    FILETIME,
    SYSTEMTIME,
    SID,
    win32_error,
    rpc_status,
    hresult,
    TriviallyCopyable,
    NonTriviallyCopyableNoThrowConstructible,
    NonTriviallyCopyable>;

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
constexpr TypeId kTypeId = TypeIndex<T, Types>::kValue;

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
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(long double),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(void*),
    sizeof(TypeId) + sizeof(Length) /* + std::byte[padding] + char[std::strlen(str)] + 0x00 */,
    sizeof(TypeId) + sizeof(Length) /* + std::byte[padding] + wchar_t[std::wcslen(str)] + 0x00 0x00 */,
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(GUID),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(FILETIME),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(SYSTEMTIME),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(SID) /* + DWORD[n] */,
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(win32_error),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(rpc_status),
    sizeof(TypeId) /*+ std::byte[padding] */ + sizeof(hresult),
    sizeof(TypeId) + sizeof(FunctionTable*) /* + std::byte[padding] + std::byte[sizeof(arg)] */,                      // NOLINT(bugprone-sizeof-expression): Get size of pointer.
    sizeof(TypeId) + sizeof(FunctionTableNoThrowConstructible*) /* + std::byte[padding] + std::byte[sizeof(arg)] */,  // NOLINT(bugprone-sizeof-expression): Get size of pointer.
    sizeof(TypeId) + sizeof(FunctionTableNonTrivial*) /* + std::byte[padding] + std::byte[sizeof(arg)] */             // NOLINT(bugprone-sizeof-expression): Get size of pointer.
};

static_assert(std::tuple_size_v<Types> == sizeof(kTypeSizes) / sizeof(kTypeSizes[0]), "length of kTypeSizes does not match Types");

/// @brief A constant to get the (basic) buffer size of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
constexpr std::uint8_t kTypeSize = kTypeSizes[kTypeId<T>];


/// @brief The number of bytes to add to the argument buffer after it became too small.
constexpr Size kGrowBytes = 512;

/// @brief Get the next allocation chunk, i.e. the next block which is a multiple of `#kGrowBytes`.
/// @param value The required size.
/// @return The value rounded up to multiples of `#kGrowBytes`.
[[nodiscard]] __declspec(noalias) constexpr Size GetNextChunk(const Size value) noexcept {
	constexpr Size kMask = kGrowBytes - 1;
	static_assert((kGrowBytes & kMask) == 0, "kGrowBytes must be a multiple of 2");

	return value + ((kGrowBytes - (value & kMask)) & kMask);
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @tparam T The type.
/// @param ptr The target address.
/// @return The padding to account for a properly aligned type.
template <typename T>
[[nodiscard]] __declspec(noalias) Align GetPadding(_In_ const std::byte* __restrict const ptr) noexcept {
	if constexpr (alignof(T) == 1) {
		return 0;
	} else {
		static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of type is too large");
		constexpr Align kMask = alignof(T) - 1;
		return static_cast<Align>(alignof(T) - (reinterpret_cast<std::uintptr_t>(ptr) & kMask)) & kMask;
	}
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @param ptr The target address.
/// @param align The required alignment in number of bytes.
/// @return The padding to account for a properly aligned type.
[[nodiscard]] __declspec(noalias) Align GetPadding(_In_ const std::byte* __restrict const ptr, const Align align) noexcept {
	assert(align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	const Align mask = align - 1u;
	return static_cast<Align>(align - (reinterpret_cast<std::uintptr_t>(ptr) & mask)) & mask;
}

/// @brief Read a value from the buffer.
/// @details Use std::memcpy to comply with strict aliasing and alignment rules.
/// @tparam T The target type to read.
/// @param buffer The source address to read from.
/// @return The read value.
template <typename T>
requires std::is_trivially_copyable_v<T> && std::is_trivially_constructible_v<T> && requires {
	requires std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>;
}
[[nodiscard]] __declspec(noalias) T GetValue(_In_ const std::byte* __restrict const buffer) noexcept {
	T result;                                 // NOLINT(cppcoreguidelines-pro-type-member-init): Initialized using std::memcpy.
	std::memcpy(&result, buffer, sizeof(T));  // NOLINT(bugprone-sizeof-expression): Deliberately supporting sizeof pointers.
	return result;
}


//
// Decoding Arguments
//

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// @tparam T The type of the argument.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T, typename A>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ Size& position) {
	const Size pos = position + sizeof(TypeId);
	const Align padding = GetPadding<T>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	if constexpr (std::is_same_v<A, LogEventArgs>) {
		args << *reinterpret_cast<const T*>(pData);
	} else if constexpr (std::is_same_v<A, LogFormatArgs>) {
		const T arg = GetValue<T>(pData);
		if constexpr (std::is_same_v<T, wchar_t>) {
			if (arg < 0x20) {
				args << static_cast<char>(arg);
			} else {
				// string types (but not string_views) are copied into format args
				args << EncodeUtf8(&arg, 1);
			}
		} else {
			args << arg;
		}
	} else {
		static_assert_no_clang(false, "Unknown argument type");
	}

	position += kTypeSize<T> + padding;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding. This is the specialization for strings.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <AnyOf<const char*, const wchar_t*> T, typename A>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ Size& position) {
	using B = std::remove_const_t<std::remove_pointer_t<T>>;
	static_assert(is_any_of_v<B, char, wchar_t>);

	const Length length = GetValue<Length>(&buffer[position + sizeof(TypeId)]);
	const Align padding = GetPadding<B>(&buffer[position + kTypeSize<T>]);

	T const str = reinterpret_cast<T>(&buffer[position + sizeof(TypeId) + sizeof(Length) + padding]);
	if constexpr (std::is_same_v<A, LogEventArgs>) {
		args << std::span(str, length + 1);
	} else if constexpr (std::is_same_v<A, LogFormatArgs>) {
		if constexpr (std::is_same_v<B, char>) {
			args << std::string_view(str, length);
		} else if constexpr (std::is_same_v<B, wchar_t>) {
			args << EncodeUtf8(str, length);
		} else {
			static_assert_no_clang(false, "Unknown string type");
		}
	} else {
		static_assert_no_clang(false, "Unknown argument type");
	}
	position += kTypeSize<T> + padding + length * static_cast<Align>(sizeof(B)) + static_cast<Align>(sizeof(B));
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding. This is the specialization for SID objects.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <std::same_as<SID>, typename A>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ Size& position) {
	const Size pos = position + sizeof(TypeId);
	const Align padding = GetPadding<SID>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	const auto subAuthorityCount = GetValue<decltype(SID::SubAuthorityCount)>(pData + offsetof(SID, SubAuthorityCount));
	const Size add = subAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);

	if constexpr (std::is_same_v<A, LogEventArgs>) {
		args << std::span<const std::byte>(pData, sizeof(SID) + add);
	} else if constexpr (std::is_same_v<A, LogFormatArgs>) {
		const SID& arg = *reinterpret_cast<const SID*>(pData);
		args << arg;
	} else {
		static_assert_no_clang(false, "Unknown argument type");
	}

	position += kTypeSize<SID> + padding + add;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding. This is the specialization for custom types.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <AnyOf<TriviallyCopyable, NonTriviallyCopyableNoThrowConstructible, NonTriviallyCopyable> T, typename A>
void DecodeArgument(_Inout_ A& args, _Inout_ const std::byte* __restrict const buffer, _Inout_ Size& position) {
	constexpr auto kArgSize = kTypeSize<T>;

	const Size pos = position + sizeof(TypeId);
	const FunctionTable* const pFunctionTable = GetValue<const FunctionTable*>(&buffer[pos]);
	const Align padding = GetPadding(&buffer[position + kArgSize], pFunctionTable->align);

	if constexpr (std::is_same_v<A, LogEventArgs>) {
		pFunctionTable->addEventData(args, &buffer[position + kArgSize + padding]);
	} else if constexpr (std::is_same_v<A, LogFormatArgs>) {
		pFunctionTable->addFormatArgs(args, &buffer[position + kArgSize + padding]);
	} else {
		static_assert_no_clang(false, "Unknown argument type");
	}
	position += kArgSize + padding + pFunctionTable->size;
}


//
// Skipping Arguments
//

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @tparam T The character type, i.e. either `char` or `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <typename T>
void SkipInlineString(_In_ const std::byte* __restrict buffer, _Inout_ Size& position) noexcept {
	using B = std::remove_const_t<std::remove_pointer_t<T>>;
	static_assert(is_any_of_v<B, char, wchar_t>);

	const Length length = GetValue<Length>(&buffer[position + sizeof(TypeId)]);
	const Align padding = GetPadding<B>(&buffer[position + kTypeSize<T>]);

	position += kTypeSize<T> + padding + length * static_cast<Length>(sizeof(B)) + static_cast<Length>(sizeof(B));
}

/// @brief Skip a log argument of type SID.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
void SkipSID(_In_ const std::byte* __restrict buffer, _Inout_ Size& position) noexcept {
	const Size pos = position + sizeof(TypeId);
	const Align padding = GetPadding<SID>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	const auto subAuthorityCount = GetValue<decltype(SID::SubAuthorityCount)>(pData + offsetof(SID, SubAuthorityCount));
	const Size add = subAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);

	position += kTypeSize<SID> + padding + add;
}

/// @brief Skip a log argument of custom type.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
void SkipTriviallyCopyable(_Inout_ const std::byte* __restrict const buffer, _Inout_ Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	const Size pos = position + sizeof(TypeId);
	const FunctionTable* __restrict const pFunctionTable = GetValue<const FunctionTable*>(&buffer[pos]);
	const Align padding = GetPadding(&buffer[position + kArgSize], pFunctionTable->align);

	position += kTypeSize<TriviallyCopyable> + padding + pFunctionTable->size;
}


//
// Copying Arguments
//

/// @brief Copies a custom type by calling construct (new) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyNonTriviallyCopyableNoThrowConstructible(_Inout_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyableNoThrowConstructible>;

	const Size pos = position + sizeof(TypeId);
	const FunctionTableNoThrowConstructible* __restrict const pFunctionTable = GetValue<const FunctionTableNoThrowConstructible*>(&src[pos]);
	const Align padding = GetPadding(&src[position + kArgSize], pFunctionTable->align);

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const Size offset = position + kArgSize + padding;
	pFunctionTable->copy(&src[offset], &dst[offset]);

	position = offset + pFunctionTable->size;
}

/// @brief Copies the buffer from @p src to @p dst.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param used The number of valid data bytes in the buffer.
void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const Size used) noexcept {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	Size start = 0;
	for (Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP")
#define SKIP(type_)                                                                        \
	case kTypeId<type_>:                                                                   \
		position += kTypeSize<type_> + GetPadding<type_>(&src[position + sizeof(TypeId)]); \
		break
		/// @endcond

		switch (typeId) {
			SKIP(bool);
			SKIP(char);
			SKIP(wchar_t);
			SKIP(signed char);
			SKIP(unsigned char);
			SKIP(signed short);
			SKIP(unsigned short);
			SKIP(signed int);
			SKIP(unsigned int);
			SKIP(signed long);
			SKIP(unsigned long);
			SKIP(signed long long);
			SKIP(unsigned long long);
			SKIP(float);
			SKIP(double);
			SKIP(long double);
			SKIP(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(src, position);
			break;
			SKIP(GUID);
			SKIP(FILETIME);
			SKIP(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(src, position);
			break;
			SKIP(win32_error);
			SKIP(rpc_status);
			SKIP(hresult);
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyableNoThrowConstructible>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyNonTriviallyCopyableNoThrowConstructible(src, dst, position);
			start = position;
			break;
		case kTypeId<NonTriviallyCopyable>:
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}


//
// Moving Arguments
//

/// @brief Moves a custom type by calling construct (new) and destruct (old) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveNonTriviallyCopyableNoThrowConstructible(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyableNoThrowConstructible>;

	const Size pos = position + sizeof(TypeId);
	const FunctionTableNoThrowConstructible* __restrict const pFunctionTable = GetValue<const FunctionTableNoThrowConstructible*>(&src[pos]);
	const Align padding = GetPadding(&src[position + kArgSize], pFunctionTable->align);

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const Size offset = position + kArgSize + padding;
	pFunctionTable->move(&src[offset], &dst[offset]);
	// and destruct the copied-from version
	pFunctionTable->destruct(&src[offset]);

	position = offset + pFunctionTable->size;
}

/// @brief Moves the buffer from @p src to @p dst.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param used The number of valid data bytes in the buffer.
void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const Size used) noexcept {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	Size start = 0;
	for (Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP")
#define SKIP(type_)                                                                        \
	case kTypeId<type_>:                                                                   \
		position += kTypeSize<type_> + GetPadding<type_>(&src[position + sizeof(TypeId)]); \
		break
		/// @endcond

		switch (typeId) {
			SKIP(bool);
			SKIP(char);
			SKIP(wchar_t);
			SKIP(signed char);
			SKIP(unsigned char);
			SKIP(signed short);
			SKIP(unsigned short);
			SKIP(signed int);
			SKIP(unsigned int);
			SKIP(signed long);
			SKIP(unsigned long);
			SKIP(signed long long);
			SKIP(unsigned long long);
			SKIP(float);
			SKIP(double);
			SKIP(long double);
			SKIP(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(src, position);
			break;
			SKIP(GUID);
			SKIP(FILETIME);
			SKIP(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(src, position);
			break;
			SKIP(win32_error);
			SKIP(rpc_status);
			SKIP(hresult);
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyableNoThrowConstructible>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveNonTriviallyCopyableNoThrowConstructible(src, dst, position);
			start = position;
			break;
		case kTypeId<NonTriviallyCopyable>:
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}


//
// Calling Argument Destructors
//

/// @brief Call the destructor of a custom type and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
template <typename T>
void DestructNonTriviallyCopyable(_In_ std::byte* __restrict const buffer, _Inout_ Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<T>;

	const Size pos = position + sizeof(TypeId);
	const FunctionTableNonTrivial* const pFunctionTable = GetValue<const FunctionTableNonTrivial*>(&buffer[pos]);
	const Align padding = GetPadding(&buffer[position + kArgSize], pFunctionTable->align);

	const Size offset = position + kArgSize + padding;
	pFunctionTable->destruct(&buffer[offset]);

	position = offset + pFunctionTable->size;
}

/// @brief Calls destructors for any non-trivial objects in the buffer.
/// @param buffer The argument buffer.
/// @param used The number of valid data bytes in the buffer.
void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, Size used) noexcept {
	for (Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("SKIP")
#define SKIP(type_)                                                                           \
	case kTypeId<type_>:                                                                      \
		position += kTypeSize<type_> + GetPadding<type_>(&buffer[position + sizeof(TypeId)]); \
		break
		/// @endcond

		switch (typeId) {
			SKIP(bool);
			SKIP(char);
			SKIP(wchar_t);
			SKIP(signed char);
			SKIP(unsigned char);
			SKIP(signed short);
			SKIP(unsigned short);
			SKIP(signed int);
			SKIP(unsigned int);
			SKIP(signed long);
			SKIP(unsigned long);
			SKIP(signed long long);
			SKIP(unsigned long long);
			SKIP(float);
			SKIP(double);
			SKIP(long double);
			SKIP(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(buffer, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(buffer, position);
			break;
			SKIP(GUID);
			SKIP(FILETIME);
			SKIP(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(buffer, position);
			break;
			SKIP(win32_error);
			SKIP(rpc_status);
			SKIP(hresult);
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(buffer, position);
			break;
		case kTypeId<NonTriviallyCopyableNoThrowConstructible>:
			DestructNonTriviallyCopyable<NonTriviallyCopyableNoThrowConstructible>(buffer, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			DestructNonTriviallyCopyable<NonTriviallyCopyable>(buffer, position);
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP")
	}
}

}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogDataBase::LogDataBase(const LogDataBase& other) noexcept
    : m_hasHeapBuffer(other.m_hasHeapBuffer)
    , m_hasNonTriviallyCopyable(other.m_hasNonTriviallyCopyable)
    , m_used(other.m_used) {
	if (m_hasHeapBuffer) {
		[[unlikely]];
		std::construct_at(&GetHeapBuffer(), other.GetHeapBuffer());
		GetHeapBufferSize() = other.GetHeapBufferSize();
	} else {
		if (m_hasNonTriviallyCopyable) {
			[[unlikely]];
			CopyObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogDataBase::LogDataBase(LogDataBase&& other) noexcept
    : m_hasHeapBuffer(other.m_hasHeapBuffer)
    , m_hasNonTriviallyCopyable(other.m_hasNonTriviallyCopyable)
    , m_used(other.m_used) {
	// copy
	if (m_hasHeapBuffer) {
		[[unlikely]];
		std::construct_at(&GetHeapBuffer(), std::move(other.GetHeapBuffer()));
		GetHeapBufferSize() = other.GetHeapBufferSize();
		std::destroy_at(&other.GetHeapBuffer());
		other.m_hasHeapBuffer = false;
	} else {
		if (m_hasNonTriviallyCopyable) {
			[[unlikely]];
			MoveObjects(other.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
		}
	}
	// leave source in a consistent state
	other.m_hasNonTriviallyCopyable = false;
	other.m_used = 0;
}

LogDataBase::~LogDataBase() noexcept {
	// ensure proper memory layout
	static_assert(sizeof(LogDataBase) == M3C_LOGDATA_SIZE, "size of LogLine");

	static_assert_no_clang(offsetof(LogDataBase, m_stackBuffer) == 0, "offset of m_stackBuffer");
#if UINTPTR_MAX == UINT64_MAX
	static_assert_no_clang(offsetof(LogDataBase, m_used) == M3C_LOGDATA_SIZE - 4, "offset of m_used");
#elif UINTPTR_MAX == UINT32_MAX
	static_assert_no_clang(offsetof(LogDataBase, m_used) == M3C_LOGDATA_SIZE - 4, "offset of m_used");
#else
	static_assert_no_clang(false, "layout assertions not defined");
#endif
	if ((!m_hasHeapBuffer || GetHeapBuffer().use_count() == 1) && m_hasNonTriviallyCopyable) {
		[[unlikely]];
		CallDestructors(GetBuffer(), m_used);
	}
	if (m_hasHeapBuffer) {
		[[unlikely]];
		std::destroy_at(&GetHeapBuffer());
	}
}

LogDataBase& LogDataBase::operator=(const LogDataBase& other) noexcept {
	if (std::addressof(other) != this) {
		[[likely]];
		// clean self
		if ((!m_hasHeapBuffer || GetHeapBuffer().use_count() == 1) && m_hasNonTriviallyCopyable) {
			[[unlikely]];
			CallDestructors(GetBuffer(), m_used);
		}

		// copy
		m_hasNonTriviallyCopyable = other.m_hasNonTriviallyCopyable;
		m_used = other.m_used;
		if (other.m_hasHeapBuffer) {
			[[unlikely]];
			if (m_hasHeapBuffer) {
				[[unlikely]];
				GetHeapBuffer() = other.GetHeapBuffer();
			} else {
				std::construct_at(&GetHeapBuffer(), other.GetHeapBuffer());
				m_hasHeapBuffer = true;
			}
			GetHeapBufferSize() = other.GetHeapBufferSize();
		} else {
			if (m_hasHeapBuffer) {
				[[unlikely]];
				std::destroy_at(&GetHeapBuffer());
				m_hasHeapBuffer = false;
			}
			if (m_hasNonTriviallyCopyable) {
				[[unlikely]];
				CopyObjects(other.m_stackBuffer, m_stackBuffer, m_used);
			} else {
				std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
			}
		}
	}
	return *this;
}

LogDataBase& LogDataBase::operator=(LogDataBase&& other) noexcept {
	if (std::addressof(other) != this) {
		[[likely]];
		// clean self
		if ((!m_hasHeapBuffer || GetHeapBuffer().use_count() == 1) && m_hasNonTriviallyCopyable) {
			[[unlikely]];
			CallDestructors(GetBuffer(), m_used);
		}

		// copy
		m_hasNonTriviallyCopyable = other.m_hasNonTriviallyCopyable;
		m_used = other.m_used;
		if (other.m_hasHeapBuffer) {
			[[unlikely]];
			if (m_hasHeapBuffer) {
				[[unlikely]];
				GetHeapBuffer() = std::move(other.GetHeapBuffer());
			} else {
				std::construct_at(&GetHeapBuffer(), std::move(other.GetHeapBuffer()));
				m_hasHeapBuffer = true;
			}
			GetHeapBufferSize() = other.GetHeapBufferSize();
			std::destroy_at(&other.GetHeapBuffer());
			other.m_hasHeapBuffer = false;
		} else {
			if (m_hasHeapBuffer) {
				[[unlikely]];
				std::destroy_at(&GetHeapBuffer());
				m_hasHeapBuffer = false;
			}
			if (m_hasNonTriviallyCopyable) {
				[[unlikely]];
				MoveObjects(other.m_stackBuffer, m_stackBuffer, m_used);
			} else {
				std::memcpy(m_stackBuffer, other.m_stackBuffer, m_used);
			}
		}
		// leave source in a consistent state
		other.m_hasNonTriviallyCopyable = false;
		other.m_used = 0;
	}
	return *this;
}

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
template <LogArgs A>
void LogDataBase::CopyArgumentsTo(_Inout_ A& args) const {
	const std::byte* __restrict const buffer = GetBuffer();
	const Size used = m_used;
	for (Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("DECODE")
#define DECODE(type_)                                     \
	case kTypeId<type_>:                                  \
		DecodeArgument<type_, A>(args, buffer, position); \
		break
		/// @endcond
		switch (typeId) {
			DECODE(bool);
			DECODE(char);
			DECODE(wchar_t);
			DECODE(signed char);
			DECODE(unsigned char);
			DECODE(signed short);
			DECODE(unsigned short);
			DECODE(signed int);
			DECODE(unsigned int);
			DECODE(signed long);
			DECODE(unsigned long);
			DECODE(signed long long);
			DECODE(unsigned long long);
			DECODE(float);
			DECODE(double);
			DECODE(long double);
			DECODE(const void*);
			DECODE(const char*);
			DECODE(const wchar_t*);
			DECODE(GUID);
			DECODE(FILETIME);
			DECODE(SYSTEMTIME);
			DECODE(SID);
			DECODE(win32_error);
			DECODE(rpc_status);
			DECODE(hresult);
		case kTypeId<TriviallyCopyable>:
			DecodeArgument<TriviallyCopyable, A>(args, buffer, position);
			break;
		case kTypeId<NonTriviallyCopyableNoThrowConstructible>:
			DecodeArgument<NonTriviallyCopyableNoThrowConstructible, A>(args, buffer, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			DecodeArgument<NonTriviallyCopyable, A>(args, buffer, position);
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("DECODE")
	}
}

template void LogDataBase::CopyArgumentsTo<LogEventArgs>(_Inout_ LogEventArgs& args) const;
template void LogDataBase::CopyArgumentsTo<LogFormatArgs>(_Inout_ LogFormatArgs& args) const;


LogDataBase::HeapBuffer& LogDataBase::GetHeapBuffer() noexcept {
	return reinterpret_cast<HeapBufferInfo*>(m_stackBuffer)->heapBuffer;
}

const LogDataBase::HeapBuffer& LogDataBase::GetHeapBuffer() const noexcept {
	assert(m_hasHeapBuffer);
	return reinterpret_cast<const HeapBufferInfo*>(m_stackBuffer)->heapBuffer;
}

Size& LogDataBase::GetHeapBufferSize() noexcept {
	assert(m_hasHeapBuffer);
	return reinterpret_cast<HeapBufferInfo*>(m_stackBuffer)->size;
}

const Size& LogDataBase::GetHeapBufferSize() const noexcept {
	return reinterpret_cast<const HeapBufferInfo*>(m_stackBuffer)->size;
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogDataBase::GetBuffer() noexcept {
	return !m_hasHeapBuffer ? m_stackBuffer : GetHeapBuffer().get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) const std::byte* LogDataBase::GetBuffer() const noexcept {
	return !m_hasHeapBuffer ? m_stackBuffer : GetHeapBuffer().get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogDataBase::GetWritePosition(const Size additionalBytes, const bool forceHeap) {
	if (m_hasHeapBuffer && GetHeapBuffer().use_count() > 1) {
		[[unlikely]];
		throw std::logic_error("LogData") + evt::Default;
	}
	const std::size_t requiredSize = static_cast<std::size_t>(m_used) + additionalBytes;
	if (requiredSize > std::numeric_limits<Size>::max()) {
		[[unlikely]];
		throw std::length_error("LogData") + evt::LogData_BufferSize << additionalBytes << requiredSize;
	}

	if (!m_hasHeapBuffer) {
		[[likely]];
		if (!forceHeap && requiredSize <= sizeof(m_stackBuffer)) {
			[[likely]];
			return &m_stackBuffer[m_used];
		}
	} else {
		if (requiredSize <= GetHeapBufferSize()) {
			[[likely]];
			return &(GetHeapBuffer().get())[m_used];
		}
	}

	const Size size = GetNextChunk(static_cast<std::uint32_t>(requiredSize));
	HeapBuffer newHeapBuffer = std::make_shared_for_overwrite<std::byte[]>(size);
	if (!m_hasHeapBuffer) {
		[[likely]];
		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(newHeapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			[[unlikely]];
			MoveObjects(m_stackBuffer, newHeapBuffer.get(), m_used);
		} else {
			std::memcpy(newHeapBuffer.get(), m_stackBuffer, m_used);
		}
		std::construct_at(&GetHeapBuffer(), std::move(newHeapBuffer));
		m_hasHeapBuffer = true;
	} else {
		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(GetHeapBuffer().get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(newHeapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			[[unlikely]];
			MoveObjects(GetHeapBuffer().get(), newHeapBuffer.get(), m_used);
		} else {
			std::memcpy(newHeapBuffer.get(), GetHeapBuffer().get(), m_used);
		}
		GetHeapBuffer() = std::move(newHeapBuffer);
	}
	GetHeapBufferSize() = size;
	return &(GetHeapBuffer().get())[m_used];
}

// Derived from both methods `NanoLogLine::encode` from NanoLog.
template <typename T>
void LogDataBase::Write(const T arg) {
	static_assert(!std::is_reference_v<T>);
	constexpr TypeId kId = kTypeId<T>;
	constexpr auto kArgSize = kTypeSize<T>;

	std::byte* __restrict buffer = GetWritePosition(kArgSize);
	const Align padding = GetPadding<T>(&buffer[sizeof(kId)]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(kArgSize + padding);
	}

	std::memcpy(buffer, &kId, sizeof(kId));
	std::memcpy(&buffer[sizeof(kId) + padding], std::addressof(arg), sizeof(arg));

	m_used += kArgSize + padding;
}

template void LogDataBase::Write<bool>(bool);
template void LogDataBase::Write<char>(char);
template void LogDataBase::Write<wchar_t>(wchar_t);
template void LogDataBase::Write<signed char>(signed char);
template void LogDataBase::Write<unsigned char>(unsigned char);
template void LogDataBase::Write<signed short>(signed short);
template void LogDataBase::Write<unsigned short>(unsigned short);
template void LogDataBase::Write<signed int>(signed int);
template void LogDataBase::Write<unsigned int>(unsigned int);
template void LogDataBase::Write<signed long>(signed long);
template void LogDataBase::Write<unsigned long>(unsigned long);
template void LogDataBase::Write<signed long long>(signed long long);
template void LogDataBase::Write<unsigned long long>(unsigned long long);
template void LogDataBase::Write<float>(float);
template void LogDataBase::Write<double>(double);
template void LogDataBase::Write<long double>(long double);
template void LogDataBase::Write<const void*>(const void*);
template void LogDataBase::Write<GUID>(const GUID);
template void LogDataBase::Write<FILETIME>(const FILETIME);
template void LogDataBase::Write<SYSTEMTIME>(const SYSTEMTIME);
// no SID
template void LogDataBase::Write<win32_error>(const win32_error);
template void LogDataBase::Write<rpc_status>(const rpc_status);
template void LogDataBase::Write<hresult>(const hresult);

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
template <typename T>
void LogDataBase::WriteString(_In_reads_(len) const T* __restrict const arg, const std::size_t len) {
	constexpr TypeId kId = kTypeId<const T*>;
	constexpr auto kArgSize = kTypeSize<const T*>;
	const Length length = static_cast<Length>(std::min<std::size_t>(len, std::numeric_limits<Length>::max()));
	if (length < len) {
		[[unlikely]];
		Log::Warning(evt::LogData_Truncation, len, length);
	}
	const Size size = kArgSize + (length + 1) * sizeof(T);

	std::byte* __restrict buffer = GetWritePosition(size);
	const Align padding = GetPadding<T>(&buffer[kArgSize]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert((!m_hasHeapBuffer ? sizeof(m_stackBuffer) : GetHeapBufferSize()) - m_used >= size + padding);

	std::memcpy(buffer, &kId, sizeof(kId));
	std::memcpy(&buffer[sizeof(kId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize + padding], arg, length * sizeof(T));
	std::memset(&buffer[kArgSize + padding + length * sizeof(T)], 0, sizeof(T));

	m_used += size + padding;
}

template void LogDataBase::WriteString(const char* __restrict, std::size_t);
template void LogDataBase::WriteString(const wchar_t* __restrict, std::size_t);

void LogDataBase::WriteSID(const SID& arg) {
	constexpr TypeId kId = kTypeId<SID>;
	constexpr auto kArgSize = kTypeSize<SID>;
	const Size add = arg.SubAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);
	const Size size = kArgSize + add;

	std::byte* __restrict buffer = GetWritePosition(size);
	const Align padding = GetPadding<SID>(&buffer[sizeof(kId)]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert((!m_hasHeapBuffer ? sizeof(m_stackBuffer) : GetHeapBufferSize()) - m_used >= size + padding);

	std::memcpy(buffer, &kId, sizeof(kId));
	std::memcpy(&buffer[sizeof(kId) + padding], &arg, sizeof(SID) + add);

	m_used += size + padding;
}

template <bool kTriviallyCopyable, bool kNoThrowConstructible>
void* LogDataBase::WriteCustomType(_In_ const FunctionTable* __restrict const pFunctionTable) {
	using Type = std::conditional_t<kTriviallyCopyable, TriviallyCopyable, std::conditional_t<kNoThrowConstructible, NonTriviallyCopyableNoThrowConstructible, NonTriviallyCopyable>>;
	constexpr TypeId kId = kTypeId<Type>;
	constexpr auto kArgSize = kTypeSize<Type>;
	const Size size = kArgSize + pFunctionTable->size;

	std::byte* __restrict buffer = GetWritePosition(size, !kNoThrowConstructible);
	const Align padding = GetPadding(&buffer[kArgSize], pFunctionTable->align);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding, !kNoThrowConstructible);
	}
	assert((!m_hasHeapBuffer ? sizeof(m_stackBuffer) : GetHeapBufferSize()) - m_used >= size + padding);

	std::memcpy(buffer, &kId, sizeof(kId));
	std::memcpy(&buffer[sizeof(kId)], &const_cast<const FunctionTable* const&>(pFunctionTable), sizeof(pFunctionTable));  // NOLINT(bugprone-sizeof-expression, cppcoreguidelines-pro-type-const-cast): Get size of pointer, cast away qualifier.

	m_used += size + padding;
	if constexpr (!kTriviallyCopyable) {
		m_hasNonTriviallyCopyable = true;
	}
	return &buffer[kArgSize + padding];
}

template void* LogDataBase::WriteCustomType<false, false>(_In_ const FunctionTable* __restrict);
template void* LogDataBase::WriteCustomType<false, true>(_In_ const FunctionTable* __restrict);
template void* LogDataBase::WriteCustomType<true, true>(_In_ const FunctionTable* __restrict);
// true, false not required


namespace {

/// @brief Wrapper class for saving the original `VARTYPE` for logging.
class VariantWrapper {
public:
	/// @brief Create a new wrapper for a `VARIANT`.
	/// @param variant A `VARIANT`.
	explicit VariantWrapper(const VARIANT& variant)
	    : m_vt(variant.vt) {
		assert(m_vt & VT_BYREF);
		M3C_COM_HR(VariantCopyInd(&m_variant, &variant), evt::LogData_Variant_H, VariantTypeToString(variant.vt));
	}

	/// @brief Create a new wrapper for a `PROPVARIANT`.
	/// @param pv A `PROPVARIANT`.
	explicit VariantWrapper(const PROPVARIANT& pv)
	    : m_vt(pv.vt) {
		assert(m_vt & VT_BYREF);

		m3c::Variant variant;
		M3C_COM_HR(PropVariantToVariant(&pv, &variant), evt::LogData_Variant_H, VariantTypeToString(pv.vt));
		M3C_COM_HR(VariantCopyInd(&m_variant, &variant), evt::LogData_Variant_H, VariantTypeToString(variant.vt));
	}

	VariantWrapper(const VariantWrapper&) = default;
	VariantWrapper(VariantWrapper&&) = default;
	~VariantWrapper() = default;

public:
	VariantWrapper& operator=(const VariantWrapper&) = delete;
	VariantWrapper& operator=(VariantWrapper&&) = delete;

public:
	/// @brief Add type and value to log arguments.
	/// @param formatArgs The formatter arguments.
	void operator>>(_Inout_ LogFormatArgs& formatArgs) const {
		formatArgs << VariantTypeToString(m_vt) << FMT_FORMAT("{:v}", m_variant);
	}

	/// @brief Add type and value to log arguments.
	/// @param eventArgs The event arguments.
	void operator>>(_Inout_ LogEventArgs& eventArgs) const {
		eventArgs + VariantTypeToString(m_vt) + FMT_FORMAT(L"{:v}", m_variant);
	}

private:
	VARTYPE m_vt;       ///< @brief The original variant type.
	Variant m_variant;  ///< @brief A variant with copied references.
};

}  // namespace
}  // namespace m3c::internal


void operator>>(const VARIANT& arg, _Inout_ m3c::LogData& logData) {
	if (arg.vt & VT_BYREF) {
		[[unlikely]];
		// ensure that references are still valid when logging asynchronously
		assert((arg.vt & VT_VECTOR) == 0);

		logData << m3c::internal::VariantWrapper(arg);
		return;
	}
	// use AddCustomArgument to prevent recursion
	logData.AddCustomArgument(arg);
}

void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogData& logData) {
	if (arg.vt & VT_BYREF) {
		[[unlikely]];
		// ensure that references are still valid when logging asynchronously
		assert((arg.vt & VT_VECTOR) == 0);

		logData << m3c::internal::VariantWrapper(arg);
		return;
	}
	// use AddCustomArgument to prevent recursion
	logData.AddCustomArgument(arg);
}
