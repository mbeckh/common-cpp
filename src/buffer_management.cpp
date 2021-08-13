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

#include "buffer_management.h"

#include "m3c/LogData.h"
#include "m3c/format.h"
#include "m3c/string_encode.h"
#include "m3c/type_traits.h"
//#include "llamalog/custom_types.h"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>

namespace m3c::internal::buffer {

__declspec(noalias) LogData::Align GetPadding(_In_ const std::byte* __restrict const ptr, const LogData::Align align) noexcept {
	assert(align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	const LogData::Align mask = align - 1u;
	return static_cast<LogData::Align>(align - (reinterpret_cast<std::uintptr_t>(ptr) & mask)) & mask;
}

namespace {

using EventArgs = std::vector<EVENT_DATA_DESCRIPTOR>;
using FormatArgs = fmt::dynamic_format_arg_store<fmt::format_context>;

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
requires(!is_any_v<T, const char*, const wchar_t*, SID
#if 0
	, TriviallyCopyable, NonTriviallyCopyable
#endif
                   >) void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	const LogData::Size pos = position + sizeof(TypeId);
	const LogData::Align padding = GetPadding<T>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	if constexpr (std::is_same_v<A, EventArgs>) {
		EventDataDescCreate(&args.emplace_back(), pData, sizeof(T));
	} else if constexpr (std::is_same_v<A, FormatArgs>) {
		const T arg = GetValue<T>(pData);
		if constexpr (std::is_same_v<T, wchar_t>) {
			if (arg < 0x20) {
				args.push_back(static_cast<char>(arg));
			} else {
				args.push_back(EncodeUtf8(&arg, 1));
			}
		} else {
			args.push_back(arg);
		}
	} else {
		static_assert(false, "Unknown argument type");
	}

	position += kTypeSize<T> + padding;
}

#if 0
/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `nullptr` values stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<NullValue>(_Inout_ fmt::dynamic_format_arg_store<fmt::format_context>& args, _In_ const std::byte*, _Inout_ LogData::Size& position) {
	args.push_back(nullptr);
	position += kTypeSize<NullValue>;
}
#endif

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T, typename A>
requires is_any_v<T, const char*, const wchar_t*>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	using B = std::remove_const_t<std::remove_pointer_t<T>>;
	static_assert(is_any_v<B, char, wchar_t>);

	const LogData::Length length = GetValue<LogData::Length>(&buffer[position + sizeof(TypeId)]);
	const LogData::Align padding = GetPadding<B>(&buffer[position + kTypeSize<T>]);

	T const str = reinterpret_cast<T>(&buffer[position + sizeof(TypeId) + sizeof(LogData::Length) + padding]);
	if constexpr (std::is_same_v<A, EventArgs>) {
		EventDataDescCreate(&args.emplace_back(), str, length * sizeof(B) + sizeof(B));
	} else if constexpr (std::is_same_v<A, FormatArgs>) {
		if constexpr (std::is_same_v<B, char>) {
			args.push_back(std::string_view(str, length));
		} else if constexpr (std::is_same_v<B, wchar_t>) {
			args.push_back(EncodeUtf8(str, length));
		} else {
			static_assert(false, "Unknown string type");
		}
	} else {
		static_assert(false, "Unknown argument type");
	}
	position += kTypeSize<T> + padding + length * static_cast<LogData::Align>(sizeof(B)) + static_cast<LogData::Align>(sizeof(B));
}

template <typename T, typename A>
requires std::is_same_v<T, SID>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	const LogData::Size pos = position + sizeof(TypeId);
	const LogData::Align padding = GetPadding<SID>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	const auto subAuthorityCount = GetValue<decltype(SID::SubAuthorityCount)>(pData + offsetof(SID, SubAuthorityCount));
	const LogData::Size add = subAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);

	if constexpr (std::is_same_v<A, EventArgs>) {
		EventDataDescCreate(&args.emplace_back(), pData, sizeof(SID) + add);
	} else if constexpr (std::is_same_v<A, FormatArgs>) {
		const SID& arg = *reinterpret_cast<const SID*>(pData);
		args.push_back(std::ref(arg));
	} else {
		static_assert(false, "Unknown argument type");
	}

	position += kTypeSize<SID> + add + padding;
}

#if 0
/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for wide character strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T, typename A, std::enable_if_t<std::is_same_v<T, const wchar_t*>, int> = 0>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	const LogData::Length length = GetValue<LogData::Length>(&buffer[position + sizeof(TypeId)]);
	const LogData::Align padding = GetPadding<wchar_t>(&buffer[position + kTypeSize<const wchar_t*>]);

	const wchar_t* const wstr = reinterpret_cast<const wchar_t*>(&buffer[position + sizeof(TypeId) + sizeof(LogData::Length) + padding]);
	if constexpr (std::is_same_v<A, EventArgs>) {
		args.emplace_back();
		EventDataDescCreate(&args.back(), wstr, length * sizeof(wchar_t));
	} else if constexpr (std::is_same_v<A, FormatArgs>) {
		args.push_back(EncodeUtf8(wstr, length));
	} else {
		static_assert(false, "Unknown argument type");
	}
	position += kTypeSize<const wchar_t*> + padding + length * static_cast<LogData::Size>(sizeof(wchar_t));
}
#endif

#if 0

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T, typename A, std::enable_if_t<std::is_same_v<T, TriviallyCopyable>, int> = 0>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable::CreateFormatArg createFormatArg = GetValue<internal::FunctionTable::CreateFormatArg>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogData::Size size = GetValue<LogData::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(createFormatArg)]);

	---
		if constexpr (std::is_same_v<A, EventArgs>) {
		args.emplace_back();
		EventDataDescCreate(&args.back(), str, length * sizeof(B));
	}
	else if constexpr (std::is_same_v<A, FormatArgs>) {
		args.push_back(std::string_view(str, length));
	}
	else {
		static_assert(false, "Unknown argument type");
	}

	---
	args.push_back(createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}


/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for non-trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T, typename A, std::enable_if_t<std::is_same_v<T, NonTriviallyCopyable>, int> = 0>
void DecodeArgument(_Inout_ A& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogData::Size size = GetValue<LogData::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	args.push_back(pFunctionTable->createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}
#endif

//
// Skipping Arguments
//

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @tparam T The character type, i.e. either `char` or `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <typename T>
void SkipInlineString(_In_ const std::byte* __restrict buffer, _Inout_ LogData::Size& position) noexcept {
	using B = std::remove_const_t<std::remove_pointer_t<T>>;
	static_assert(is_any_v<B, char, wchar_t>);

	const LogData::Length length = GetValue<LogData::Length>(&buffer[position + sizeof(TypeId)]);
	const LogData::Align padding = GetPadding<B>(&buffer[position + kTypeSize<T>]);

	position += kTypeSize<T> + padding + length * static_cast<LogData::Length>(sizeof(B)) + static_cast<LogData::Length>(sizeof(B));
}

#if 0
/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `char`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<char>(_In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) noexcept {
	const LogData::Length length = GetValue<LogData::Length>(&buffer[position + sizeof(TypeId)]);

	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	position += kTypeSize<const char*> + length * sizeof(char);
}

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<wchar_t>(_In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) noexcept {
	const LogData::Length length = GetValue<LogData::Length>(&buffer[position + sizeof(TypeId)]);
	const LogData::Align padding = GetPadding<wchar_t>(&buffer[position + kTypeSize<const wchar_t*>]);

	position += kTypeSize<const wchar_t*> + padding + length * static_cast<LogData::Size>(sizeof(wchar_t));
}
#endif

void SkipSID(_In_ const std::byte* __restrict buffer, _Inout_ LogData::Size& position) noexcept {
	const LogData::Size pos = position + sizeof(TypeId);
	const LogData::Align padding = GetPadding<SID>(&buffer[pos]);
	const std::byte* __restrict const pData = &buffer[pos + padding];

	const auto subAuthorityCount = GetValue<decltype(SID::SubAuthorityCount)>(pData + offsetof(SID, SubAuthorityCount));
	const LogData::Size add = subAuthorityCount * sizeof(SID::SubAuthority[0]) - sizeof(SID::SubAuthority);

	position += kTypeSize<SID> + add + padding;
}


#if 0
/// @brief Skip a log argument of custom type.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipTriviallyCopyable(_In_ const std::byte* __restrict const buffer, _Inout_ LogData::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&buffer[position + sizeof(TypeId)]);
	const LogData::Size size = GetValue<LogData::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(internal::FunctionTable::CreateFormatArg)]);

	position += kArgSize + padding + size;
}

#endif
//
// Copying Arguments
//

#if 0
/// @brief Copies a custom type by calling construct (new) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyNonTriviallyCopyable(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogData::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&src[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<internal::FunctionTable*>(&src[position + sizeof(TypeId) + sizeof(padding)]);
	const LogData::Size size = GetValue<LogData::Size>(&src[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogData::Size offset = position + kArgSize + padding;
	pFunctionTable->copy(&src[offset], &dst[offset]);

	position = offset + size;
}

#endif
//
// Moving Arguments
//

#if 0
/// @brief Moves a custom type by calling construct (new) and destruct (old) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveNonTriviallyCopyable(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogData::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&src[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&src[position + sizeof(TypeId) + sizeof(padding)]);
	const LogData::Size size = GetValue<LogData::Size>(&src[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogData::Size offset = position + kArgSize + padding;
	pFunctionTable->move(&src[offset], &dst[offset]);
	// and destruct the copied-from version
	pFunctionTable->destruct(&src[offset]);

	position = offset + size;
}

#endif
//
// Calling Argument Destructors
//

#if 0
/// @brief Call the destructor of a custom type and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructNonTriviallyCopyable(_In_ std::byte* __restrict const buffer, _Inout_ LogData::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogData::Align padding = GetValue<LogData::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogData::Size size = GetValue<LogData::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	const LogData::Size offset = position + kArgSize + padding;
	pFunctionTable->destruct(&buffer[offset]);

	position = offset + size;
}
#endif

}  // namespace

//
// Buffer Management
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
template <typename T>
std::uint32_t CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogData::Size used, T& args) {
	std::uint32_t result = 0;
	for (LogData::Size position = 0; position < used;) {
		++result;
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("DECODE_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define DECODE_(type_)                                    \
	case kTypeId<type_>:                                  \
		DecodeArgument<type_, T>(args, buffer, position); \
		break
		/// @endcond
		switch (typeId) {
			DECODE_(bool);
			DECODE_(char);
			DECODE_(wchar_t);
			DECODE_(signed char);
			DECODE_(unsigned char);
			DECODE_(signed short);
			DECODE_(unsigned short);
			DECODE_(signed int);
			DECODE_(unsigned int);
			DECODE_(signed long);
			DECODE_(unsigned long);
			DECODE_(signed long long);
			DECODE_(unsigned long long);
			DECODE_(float);
			DECODE_(double);
			DECODE_(const void*);
			DECODE_(const char*);
			DECODE_(const wchar_t*);
			DECODE_(GUID);
			DECODE_(FILETIME);
			DECODE_(SYSTEMTIME);
			DECODE_(SID);
			DECODE_(win32_error);
			DECODE_(rpc_status);
			DECODE_(hresult);
#if 0
			DECODE_(TriviallyCopyable);     // case for ptr is handled in CreateFormatArg
			DECODE_(NonTriviallyCopyable);  // case for ptr is handled in CreateFormatArg
#endif
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("DECODE_")
	}
	return result;
}

template std::uint32_t CopyArgumentsFromBufferTo<EventArgs>(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogData::Size used, EventArgs& args);
template std::uint32_t CopyArgumentsFromBufferTo<FormatArgs>(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogData::Size used, FormatArgs& args);


void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const LogData::Size used) {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogData::Size start = 0;
	for (LogData::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(wchar_t);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(src, position);
			break;
			SKIP_(GUID);
			SKIP_(FILETIME);
			SKIP_(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(src, position);
			break;
			SKIP_(win32_error);
			SKIP_(rpc_status);
			SKIP_(hresult);
#if 0
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
#endif
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const LogData::Size used) noexcept {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogData::Size start = 0;
	for (LogData::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(wchar_t);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(src, position);
			break;
			SKIP_(GUID);
			SKIP_(FILETIME);
			SKIP_(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(src, position);
			break;
			SKIP_(win32_error);
			SKIP_(rpc_status);
			SKIP_(hresult);
#if 0
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
#endif
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogData::Size used) noexcept {
	for (LogData::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(wchar_t);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<const char*>(buffer, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<const wchar_t*>(buffer, position);
			break;
			SKIP_(GUID);
			SKIP_(FILETIME);
			SKIP_(SYSTEMTIME);
		case kTypeId<SID>:
			SkipSID(buffer, position);
			break;
			SKIP_(win32_error);
			SKIP_(rpc_status);
			SKIP_(hresult);
#if 0
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(buffer, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			DestructNonTriviallyCopyable(buffer, position);
			break;
#endif
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
}

}  // namespace m3c::internal::buffer
