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

#include "m3c/format.h"

#include "m3c/Log.h"
#include "m3c/PropVariant.h"
#include "m3c/com_heap_ptr.h"
#include "m3c/finally.h"
#include "m3c/rpc_string.h"
#include "m3c/string_encode.h"
#include "m3c/type_traits.h"

#include "m3c.events.h"

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>
#include <propvarutil.h>
#include <rpc.h>
#include <sddl.h>
#include <wtypes.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <span>
#include <string>
#include <utility>

#pragma push_macro("UuidToString")
#pragma push_macro("ConvertSidToStringSid")
#pragma push_macro("FormatMessage")
#undef UuidToString
#undef ConvertSidToStringSid
#undef FormatMessage

namespace m3c {
namespace {

template <typename CharT>
inline constexpr const CharT* kError = SelectString<CharT>(M3C_SELECT_STRING("<Error>"));


/// @brief Helper to select functions and types dependent on character type.
/// @tparam T The type used as a selector.
template <typename T>
struct FormatterTraits;

template <>
struct FormatterTraits<char> {
	using RpcStringType = rpc_string;

	static _Must_inspect_result_ RPC_STATUS UuidToString(_In_ const UUID* uuid, _Outptr_ RPC_CSTR* result) noexcept {
		return UuidToStringA(uuid, result);
	}

	static _Success_(return != FALSE) BOOL ConvertSidToStringSid(_In_ PSID sid, _Outptr_ LPSTR* result) noexcept {
		return ConvertSidToStringSidA(sid, result);
	}

	static _Success_(return != 0) DWORD FormatMessage(_In_ DWORD flags, _In_ DWORD messageId, _When_((flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0, _At_((LPSTR*) buffer, _Outptr_result_z_)) _When_((flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) == 0, _Out_writes_z_(size)) LPSTR buffer, _In_ DWORD size) noexcept {
		return FormatMessageA(flags, nullptr, messageId, 0, buffer, size, nullptr);
	}

	template <typename T>
	static auto ToString(T&& arg) {
		return fmt::to_string(std::forward<T>(arg));
	}
};

template <>
struct FormatterTraits<wchar_t> {
	using RpcStringType = rpc_wstring;

	static _Must_inspect_result_ RPC_STATUS UuidToString(_In_ const UUID* uuid, _Outptr_ RPC_WSTR* result) noexcept {
		return UuidToStringW(uuid, result);
	}

	static _Success_(return != FALSE) BOOL ConvertSidToStringSid(_In_ PSID sid, _Outptr_ LPWSTR* result) noexcept {
		return ConvertSidToStringSidW(sid, result);
	}

	static _Success_(return != 0) DWORD FormatMessage(_In_ DWORD flags, _In_ DWORD messageId, _When_((flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0, _At_((LPWSTR*) buffer, _Outptr_result_z_)) _When_((flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) == 0, _Out_writes_z_(size)) LPWSTR buffer, _In_ DWORD size) noexcept {
		return FormatMessageW(flags, nullptr, messageId, 0, buffer, size, nullptr);
	}

	template <typename T>
	static auto ToString(T&& arg) {
		return fmt::to_wstring(std::forward<T>(arg));
	}
};

template <>
struct FormatterTraits<VARIANT> {
	static _Check_return_ HRESULT VariantToStringAlloc(_In_ const VARIANT& arg, _Outptr_result_nullonfailure_ PWSTR* result) noexcept {
		return ::VariantToStringAlloc(arg, result);
	}
};

template <>
struct FormatterTraits<PROPVARIANT> {
	static _Check_return_ HRESULT VariantToStringAlloc(_In_ const PROPVARIANT& arg, _Outptr_result_nullonfailure_ PWSTR* result) noexcept {
		return PropVariantToStringAlloc(arg, result);
	}
};

/// @brief Helper to decode a value if required.
/// @tparam From The source character type.
/// @tparam To The target character type.
template <typename From, typename To>
struct EncodingTraits {
	template <typename T>
	constexpr static T&& Encode(T&& arg) noexcept {
		return std::forward<T>(arg);
	}
};

template <>
struct EncodingTraits<char, wchar_t> {
	template <typename T>
	static std::wstring Encode(T&& arg) {
		try {
			return EncodeUtf16(std::forward<T>(arg));
		} catch (const std::exception&) {
			Log::ErrorExceptionOnce(evt::Format);
			return kError<wchar_t>;
		}
	}
};

template <>
struct EncodingTraits<wchar_t, char> {
	template <typename T>
	static std::string Encode(T&& arg) {
		try {
			return EncodeUtf8(std::forward<T>(arg));
		} catch (const std::exception&) {
			Log::ErrorExceptionOnce(evt::Format);
			return kError<char>;
		};
	}
};

}  // namespace

template class fmt_encode<char>;
template class fmt_encode<wchar_t>;

}  // namespace m3c


//
// String encoding
//

template <typename T, typename CharT>
requires requires {
	requires !std::same_as<CharT, T>;
}
std::basic_string<CharT> fmt::formatter<m3c::fmt_encode<T>, CharT>::encode(const std::basic_string_view<T>& arg) {
	return m3c::EncodingTraits<T, CharT>::Encode(arg);
}

template struct fmt::formatter<m3c::fmt_encode<char>, char>;
template struct fmt::formatter<m3c::fmt_encode<char>, wchar_t>;
template struct fmt::formatter<m3c::fmt_encode<wchar_t>, char>;
template struct fmt::formatter<m3c::fmt_encode<wchar_t>, wchar_t>;


//
// GUID
//

template <typename CharT>
std::basic_string<CharT> fmt::formatter<GUID, CharT>::to_string(const GUID& arg) {
	typename m3c::FormatterTraits<CharT>::RpcStringType rpc;
	const RPC_STATUS status = m3c::FormatterTraits<CharT>::UuidToString(&arg, &rpc);

	if (status != RPC_S_OK) {
		[[unlikely]];
		m3c::Log::ErrorOnce(m3c::evt::FormatUuid_R, arg, m3c::rpc_status(status));
		return FMT_FORMAT(
		    m3c::SelectString<CharT>(M3C_SELECT_STRING("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}")),
		    arg.Data1, arg.Data2, arg.Data3, arg.Data4[0], arg.Data4[1], arg.Data4[2], arg.Data4[3], arg.Data4[4], arg.Data4[5], arg.Data4[6], arg.Data4[7]);  // NOLINT(readability-magic-numbers): Fixed GUID format.
	}
	return rpc.c_str();
}

template struct fmt::formatter<GUID, char>;
template struct fmt::formatter<GUID, wchar_t>;


//
// FILETIME, SYSTEMTIME
//

template <typename CharT>
std::basic_string<CharT> fmt::formatter<FILETIME, CharT>::to_string(const FILETIME& arg) {
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&arg, &st)) {
		[[unlikely]];
		m3c::Log::ErrorOnce(m3c::evt::FormatFileTime_E, arg, m3c::last_error());
		return m3c::FormatterTraits<CharT>::ToString(ULARGE_INTEGER{.u{.LowPart = arg.dwLowDateTime, .HighPart = arg.dwHighDateTime}}.QuadPart);
	}

	return fmt::formatter<SYSTEMTIME, CharT>::to_string(st);
}

template struct fmt::formatter<FILETIME, char>;
template struct fmt::formatter<FILETIME, wchar_t>;


template <typename CharT>
std::basic_string<CharT> fmt::formatter<SYSTEMTIME, CharT>::to_string(const SYSTEMTIME& arg) {
	return FMT_FORMAT(
	    m3c::SelectString<CharT>(M3C_SELECT_STRING("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z")),
	    arg.wYear, arg.wMonth, arg.wDay, arg.wHour, arg.wMinute, arg.wSecond, arg.wMilliseconds);
}

template struct fmt::formatter<SYSTEMTIME, char>;
template struct fmt::formatter<SYSTEMTIME, wchar_t>;


//
// SID
//

template <typename CharT>
std::basic_string<CharT> fmt::formatter<SID, CharT>::to_string(const SID& arg) {
	// ConvertSidToStringSid requires non-const argument
	_SE_SID sid;  // NOLINT(cppcoreguidelines-pro-type-member-init): Initialized by std::memcpy.
	std::memcpy(&sid, &arg, SECURITY_SID_SIZE(arg.SubAuthorityCount));
	CharT* str;  // NOLINT(cppcoreguidelines-init-variables): Initialized by call to ConvertSidToStringSidA.
	const BOOL result = m3c::FormatterTraits<CharT>::ConvertSidToStringSid(&sid, &str);
	if (!result) {
		[[unlikely]];
		m3c::Log::ErrorOnce(m3c::evt::FormatSid_E, arg, m3c::last_error());
		const std::uint64_t authority = (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[0]) << 40)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[1]) << 32)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[2]) << 24)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[3]) << 16)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[4]) << 8)
		                                | static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[5]);
		return FMT_FORMAT(
		    m3c::SelectString<CharT>(M3C_SELECT_STRING("S-{}-{}-{}")),
		    arg.Revision, authority, fmt::join(std::span(arg.SubAuthority, arg.SubAuthorityCount), m3c::SelectString<CharT>(M3C_SELECT_STRING("-"))));
	}

	auto f = m3c::finally([str]() noexcept {
		if (LocalFree(str)) {
			[[unlikely]];
			m3c::Log::ErrorOnce(m3c::evt::MemoryLeak_E, m3c::last_error());
		}
	});
	return str;
}

template struct fmt::formatter<SID, char>;
template struct fmt::formatter<SID, wchar_t>;


//
// Formatting of errors
//

namespace m3c {
namespace {

/// @brief Remove trailing line feeds from an error message.
/// @remarks This function is a helper for `#FormatSystemErrorCode()`.
/// @tparam CharT The character type of the formatter.
/// @param message The error message.
/// @param length The length of the error message NOT including a terminating null character.
/// @result The error message as a string.
template <typename CharT>
std::basic_string<CharT> PostProcessErrorMessage(_In_reads_(length) CharT* __restrict const message, std::size_t length) {
	while (length > 0 && (message[length - 1] == '\r' || message[length - 1] == '\n' || message[length - 1] == ' ')) {
		[[likely]];
		--length;
	}
	return std::basic_string<CharT>(message, length);
}

/// @brief Create an error message from a system error code.
/// @tparam CharT The character type of the formatter.
/// @param errorCode The system error code.
/// @result The error message as a string.
template <typename CharT>
std::basic_string<CharT> FormatSystemErrorCode(const std::uint32_t errorCode) {
	constexpr std::size_t kDefaultBufferSize = 256;
	CharT buffer[kDefaultBufferSize];
	DWORD length = FormatterTraits<CharT>::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, errorCode, buffer, sizeof(buffer) / sizeof(*buffer));
	if (length) {
		[[likely]];
		return PostProcessErrorMessage(buffer, length);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		CharT* pBuffer = nullptr;

		auto freeBuffer = finally([&pBuffer]() noexcept {
			if (LocalFree(pBuffer)) {
				[[unlikely]];
				Log::ErrorOnce(evt::MemoryLeak_E, last_error());
			}
		});
		length = FormatterTraits<CharT>::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, errorCode, reinterpret_cast<CharT*>(&pBuffer), 0);
		if (length) {
			[[likely]];
			return PostProcessErrorMessage(pBuffer, length);
		}
		lastError = GetLastError();
	}
	Log::ErrorOnce(evt::FormatMessageId_E, errorCode, win32_error(lastError));
	return kError<CharT>;
}

}  // namespace

namespace internal {

template class error<DWORD>;
template class error<HRESULT>;
template class error<RPC_STATUS>;

}  // namespace internal

}  // namespace m3c


template <m3c::AnyOf<m3c::win32_error, m3c::hresult, m3c::rpc_status> T, typename CharT>
std::basic_string<CharT> fmt::formatter<T, CharT>::to_string(const T& arg) {
	const auto& code = arg.code();
	if constexpr (std::is_same_v<T, m3c::hresult>) {
		return FMT_FORMAT(
		    m3c::SelectString<CharT>(M3C_SELECT_STRING("{} (0x{:X})")),
		    m3c::FormatSystemErrorCode<CharT>(code), static_cast<std::make_unsigned_t<HRESULT>>(code));
	} else {
		return FMT_FORMAT(
		    m3c::SelectString<CharT>(M3C_SELECT_STRING("{} ({})")),
		    m3c::FormatSystemErrorCode<CharT>(code), code);
	}
}

template struct fmt::formatter<m3c::win32_error, char>;
template struct fmt::formatter<m3c::win32_error, wchar_t>;
template struct fmt::formatter<m3c::hresult, char>;
template struct fmt::formatter<m3c::hresult, wchar_t>;
template struct fmt::formatter<m3c::rpc_status, char>;
template struct fmt::formatter<m3c::rpc_status, wchar_t>;


//
// VARIANT, PROPVARIANT
//

namespace m3c::internal {

namespace {

/// @brief Check if a `VARIANT` or `PROPVARIANT` object is convertible to a string.
/// @tparam T The type of the object.
/// @param arg The object.
/// @return `true` if the object has a string representation, else `false`.
template <AnyOf<VARIANT, PROPVARIANT> T>
bool IsConvertibleToString(const T& arg) noexcept {
	switch (arg.vt & ~VARENUM::VT_TYPEMASK) {
	case 0:
	case VARENUM::VT_VECTOR:
	case VARENUM::VT_BYREF:
		break;
	default:
		return false;
	}

	switch (arg.vt & VARENUM::VT_TYPEMASK) {
	case VARENUM::VT_I2:
	case VARENUM::VT_I4:
	case VARENUM::VT_R4:
	case VARENUM::VT_R8:
	case VARENUM::VT_DATE:
	case VARENUM::VT_BSTR:
	case VARENUM::VT_BOOL:
	case VARENUM::VT_DECIMAL:
	case VARENUM::VT_I1:
	case VARENUM::VT_UI1:
	case VARENUM::VT_UI2:
	case VARENUM::VT_UI4:
	case VARENUM::VT_I8:
	case VARENUM::VT_UI8:
	case VARENUM::VT_INT:
	case VARENUM::VT_UINT:
	case VARENUM::VT_VOID:
	case VARENUM::VT_HRESULT:
	case VARENUM::VT_PTR:
	case VARENUM::VT_LPSTR:
	case VARENUM::VT_LPWSTR:
	case VARENUM::VT_INT_PTR:
	case VARENUM::VT_UINT_PTR:
	case VARENUM::VT_FILETIME:
	case VARENUM::VT_CLSID:
		return true;
	case VARENUM::VT_VARIANT:
		return (arg.vt & VARENUM::VT_VECTOR) == 0 && arg.pvarVal != nullptr && IsConvertibleToString(*arg.pvarVal);
	default:
		return false;
	}
}

}  // namespace


template <AnyOf<VARIANT, PROPVARIANT> T, typename CharT>
std::basic_string<CharT> BaseVariantFormatter<T, CharT>::to_string(const T& arg) const {
	std::string vt;
	if (m_presentation != 'v') {
		vt = VariantTypeToString(arg.vt);
		if (arg.vt == (VARENUM::VT_VARIANT | VARENUM::VT_BYREF)) {
			vt += "->";
			vt += VariantTypeToString(arg.pvarVal->vt);
		}
	}
	if (m_presentation == 't') {
		return EncodingTraits<char, CharT>::Encode(std::move(vt));
	}
	if (IsConvertibleToString(arg)) {
		com_heap_ptr<wchar_t> pwsz;
		const HRESULT hr = FormatterTraits<T>::VariantToStringAlloc(arg, &pwsz);
		if (FAILED(hr)) {
			[[unlikely]];
			Log::ErrorOnce(evt::FormatVariant_H, vt, hresult(hr));
		} else {
			if (m_presentation == 'v') {
				return EncodingTraits<wchar_t, CharT>::Encode(pwsz.get());
			}
			return FMT_FORMAT(
			    SelectString<CharT>(M3C_SELECT_STRING("({}: {})")),
			    fmt_encode(vt), fmt_encode(pwsz.get()));
		}
	}
	// not convertible to string
	if (m_presentation == 'v') {
		if (arg.vt == VARENUM::VT_EMPTY || (arg.vt == (VARENUM::VT_VARIANT | VARENUM::VT_BYREF) && arg.pvarVal->vt == VARENUM::VT_EMPTY)) {
			return SelectString<CharT>(M3C_SELECT_STRING(""));
		}
		return SelectString<CharT>(M3C_SELECT_STRING("<?>"));
	}
	return FMT_FORMAT(SelectString<CharT>(M3C_SELECT_STRING("({})")), fmt_encode(vt));
}

template struct BaseVariantFormatter<VARIANT, char>;
template struct BaseVariantFormatter<VARIANT, wchar_t>;
template struct BaseVariantFormatter<PROPVARIANT, char>;
template struct BaseVariantFormatter<PROPVARIANT, wchar_t>;

}  // namespace m3c::internal


template struct fmt::formatter<VARIANT, char>;
template struct fmt::formatter<VARIANT, wchar_t>;
template struct fmt::formatter<PROPVARIANT, char>;
template struct fmt::formatter<PROPVARIANT, wchar_t>;


namespace m3c {

template class fmt_ptr<IUnknown>;
template class fmt_ptr<IStream>;

}  // namespace m3c


template struct fmt::formatter<m3c::fmt_ptr<IUnknown>, char>;
template struct fmt::formatter<m3c::fmt_ptr<IUnknown>, wchar_t>;

namespace m3c::internal {

//
// IUnknown, IStream
//

std::wstring BaseIStreamFormatter::GetName(IStream* const ptr) {
	if (!ptr) {
		[[unlikely]];
		return L"<Empty>";
	}

	STATSTG statstg{};
	auto f = finally([&statstg]() noexcept {
		CoTaskMemFree(statstg.pwcsName);
	});
	const HRESULT hr = ptr->Stat(&statstg, STATFLAG_DEFAULT);
	if (FAILED(hr)) {
		[[unlikely]];
		Log::ErrorOnce(evt::FormatIStream_H, hresult(hr));
		return kError<wchar_t>;
	}
	return statstg.pwcsName ? statstg.pwcsName : L"<IStream>";
}

}  // namespace m3c::internal

template struct fmt::formatter<m3c::fmt_ptr<IStream>, char>;
template struct fmt::formatter<m3c::fmt_ptr<IStream>, wchar_t>;


namespace m3c::internal {

//
// PROPERTYKEY
//

std::wstring BasePropertyKeyFormatter::GetName(const PROPERTYKEY& arg) {
	com_heap_ptr<wchar_t> key;
	const HRESULT hr = PSGetNameFromPropertyKey(arg, &key);
	if (FAILED(hr)) {
		[[unlikely]];
		Log::ErrorOnce(m3c::evt::FormatPropertyKey_H, arg.fmtid, hresult(hr));
		return fmt::to_wstring(arg.fmtid);
	}
	return key.get();
}

}  // namespace m3c::internal

template struct fmt::formatter<PROPERTYKEY, char>;
template struct fmt::formatter<PROPERTYKEY, wchar_t>;

template struct fmt::formatter<WICRect, char>;
template struct fmt::formatter<WICRect, wchar_t>;

template struct fmt::formatter<FILE_ID_128, char>;
template struct fmt::formatter<FILE_ID_128, wchar_t>;


#pragma pop_macro("UuidToString")
#pragma pop_macro("ConvertSidToStringSid")
#pragma pop_macro("FormatMessage")
