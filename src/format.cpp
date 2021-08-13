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

#include "m3c/format.h"

#include "m3c_events.h"

#include "m3c/Log.h"
#include "m3c/PropVariant.h"
#include "m3c/com_heap_ptr.h"
#include "m3c/exception.h"
#include "m3c/finally.h"
#include "m3c/rpc_string.h"
#if 0
#include "m3c/com_ptr.h"
#include "m3c/finally.h"
#include "m3c/format.h"
#endif
#include "m3c/string_encode.h"

#include <fmt/chrono.h>
#include <fmt/core.h>
//#include <llamalog/llamalog.h>

#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>
#include <propvarutil.h>
#include <rpc.h>
#include <sddl.h>
#include <wincodec.h>
#include <wtypes.h>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

fmt::format_context::iterator FormatSystemTime(const SYSTEMTIME& arg, const std::string format, fmt::format_context& ctx) {
	constexpr char kPattern[] = "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z";

	if (format.empty()) [[likely]] {
		return fmt::format_to(ctx.out(), kPattern, arg.wYear, arg.wMonth, arg.wDay, arg.wHour, arg.wMinute, arg.wSecond, arg.wMilliseconds);
	}
	const std::string str = fmt::format(kPattern, arg.wYear, arg.wMonth, arg.wDay, arg.wHour, arg.wMinute, arg.wSecond, arg.wMilliseconds);
	return fmt::format_to(ctx.out(), format, str);
}

/// @brief Remove trailing line feeds from an error message and print.
/// @remarks This function is a helper for `#FormatSystemErrorCodeTo()`.
/// @param message The error message.
/// @param length The length of the error message NOT including a terminating null character.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator PostProcessErrorMessage(_In_reads_(length) char* __restrict const message, std::size_t length, fmt::format_context& ctx) {
	while (length > 0 && (message[length - 1] == '\r' || message[length - 1] == '\n' || message[length - 1] == ' '))
		[[likely]] {
			--length;
		}
	return std::copy(message, message + length, ctx.out());
}

/// @brief Create an error message from a system error code and print.
/// @param errorCode The system error code.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator FormatSystemErrorCodeTo(const std::uint32_t errorCode, fmt::format_context& ctx) {
	constexpr std::size_t kDefaultBufferSize = 256;
	char buffer[kDefaultBufferSize];
	DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), nullptr);
	if (length) [[likely]] {
		return PostProcessErrorMessage(buffer, length, ctx);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		char* pBuffer = nullptr;

		auto freeBuffer = m3c::finally([&pBuffer]() noexcept {
			if (LocalFree(pBuffer)) [[unlikely]] {
				m3c::Log::ErrorOnce(m3c::evt::LocalFree, m3c::make_win32_error());
			}
		});
		length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, errorCode, 0, reinterpret_cast<char*>(&pBuffer), 0, nullptr);
		if (length) [[likely]] {
			return PostProcessErrorMessage(pBuffer, length, ctx);
		}
		lastError = GetLastError();
	}
	m3c::Log::ErrorOnce(m3c::evt::FormatMessageX, errorCode, m3c::make_win32_error(lastError));
	const std::string_view sv("<Error>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}

bool IsConvertibleToString(const PROPVARIANT& pv) noexcept {
	switch (pv.vt & ~VARENUM::VT_TYPEMASK) {
	case 0:
	case VARENUM::VT_VECTOR:
	case VARENUM::VT_BYREF:
		break;
	default:
		return false;
	}

	switch (pv.vt & VARENUM::VT_TYPEMASK) {
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
		return !(pv.vt & VARENUM::VT_VECTOR) && pv.pvarVal && IsConvertibleToString(*pv.pvarVal);
	default:
		return false;
	}
};

}  // namespace

//
// GUID
//

fmt::format_context::iterator fmt::formatter<GUID>::format(const GUID& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	m3c::rpc_string rpc;
	const RPC_STATUS status = UuidToStringA(&arg, &rpc);
	if (status != RPC_S_OK) [[unlikely]] {
		m3c::Log::ErrorOnce(m3c::evt::UuidToStringX, arg, m3c::make_rpc_status(status));
		constexpr char kPattern[] = "{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}";
		if (format.empty()) [[likely]] {
			return fmt::format_to(ctx.out(), kPattern, arg.Data1, arg.Data2, arg.Data3, arg.Data4[0], arg.Data4[1], arg.Data4[2], arg.Data4[3], arg.Data4[4], arg.Data4[5], arg.Data4[6], arg.Data4[7]);
		}
		const std::string str = fmt::format(kPattern, arg.Data1, arg.Data2, arg.Data3, arg.Data4[0], arg.Data4[1], arg.Data4[2], arg.Data4[3], arg.Data4[4], arg.Data4[5], arg.Data4[6], arg.Data4[7]);
		return fmt::format_to(ctx.out(), format, str);
	}

	if (format.empty()) [[likely]] {
		const std::string_view sv(rpc.as_native());
		return std::copy(sv.cbegin(), sv.cend(), ctx.out());
	}
	return fmt::format_to(ctx.out(), format, rpc.as_native());
}

fmt::format_context::iterator fmt::formatter<FILETIME>::format(const FILETIME& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();
	constexpr char kPattern[] = "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z";

	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&arg, &st)) [[unlikely]] {
		m3c::Log::ErrorOnce(m3c::evt::FileTimeToSystemTime, arg, m3c::make_win32_error());
		if (format.empty()) [[likely]] {
			return fmt::format_to(ctx.out(), "{}", ULARGE_INTEGER{.LowPart = arg.dwLowDateTime, .HighPart = arg.dwHighDateTime}.QuadPart);
		}
		const std::string str = fmt::format("{}", ULARGE_INTEGER{.LowPart = arg.dwLowDateTime, .HighPart = arg.dwHighDateTime}.QuadPart);
		return fmt::format_to(ctx.out(), format, str);
	}

	return FormatSystemTime(st, format, ctx);
}

fmt::format_context::iterator fmt::formatter<SYSTEMTIME>::format(const SYSTEMTIME& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	return FormatSystemTime(arg, GetFormat(), ctx);
}

fmt::format_context::iterator fmt::formatter<SID>::format(const SID& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	LPSTR sid;
	if (!ConvertSidToStringSidA(const_cast<SID*>(&arg), &sid)) [[unlikely]] {
		m3c::Log::ErrorOnce(m3c::evt::ConvertSidToStringSidX, arg, m3c::make_win32_error());
		const std::uint64_t authority = (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[0]) << 40)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[1]) << 32)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[2]) << 24)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[3]) << 16)
		                                | (static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[4]) << 8)
		                                | static_cast<std::uint64_t>(arg.IdentifierAuthority.Value[5]);
		if (format.empty()) [[likely]] {
			return fmt::format_to(ctx.out(), "S-{}-{}-{}", arg.Revision, authority, fmt::join(std::span(arg.SubAuthority, arg.SubAuthorityCount), "-"));
		}
		const std::string str = fmt::format("S-{}-{}-{}", arg.Revision, authority, fmt::join(std::span(arg.SubAuthority, arg.SubAuthorityCount), "-"));
		return fmt::format_to(ctx.out(), format, str);
	}

	auto f = m3c::finally([sid]() noexcept {
		if (LocalFree(sid)) [[unlikely]] {
			m3c::Log::ErrorOnce(m3c::evt::LocalFree, m3c::make_win32_error());
		}
	});
	std::string_view sv(sid);

	if (format.empty()) [[likely]] {
		return std::copy(sv.cbegin(), sv.cend(), ctx.out());
	}
	return fmt::format_to(ctx.out(), format, sv);
}

fmt::format_context::iterator fmt::formatter<m3c::internal::win32_error>::format(const m3c::internal::win32_error& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	if (format.empty()) [[likely]] {
		return fmt::format_to(FormatSystemErrorCodeTo(arg.code, ctx), " ({})", arg.code);
	}

	const std::string str = fmt::format("{}", m3c::make_win32_error(arg.code));
	return fmt::format_to(ctx.out(), format, str);
}

fmt::format_context::iterator fmt::formatter<m3c::internal::rpc_status>::format(const m3c::internal::rpc_status& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	if (format.empty()) [[likely]] {
		return fmt::format_to(FormatSystemErrorCodeTo(arg.code, ctx), " ({})", arg.code);
	}

	const std::string str = fmt::format("{}", m3c::make_rpc_status(arg.code));
	return fmt::format_to(ctx.out(), format, str);
}

fmt::format_context::iterator fmt::formatter<m3c::internal::hresult>::format(const m3c::internal::hresult& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	if (format.empty()) [[likely]] {
		return fmt::format_to(FormatSystemErrorCodeTo(arg.code, ctx), " (0x{:X})", static_cast<std::make_unsigned_t<HRESULT>>(arg.code));
	}

	const std::string str = fmt::format("{}", m3c::make_hresult(arg.code));
	return fmt::format_to(ctx.out(), format, str);
}

//
// PROPVARIANT
//

fmt::format_context::iterator fmt::formatter<PROPVARIANT>::format(const PROPVARIANT& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	const std::string vt = m3c::VariantTypeToString(arg.vt);
	if (IsConvertibleToString(arg)) {
		std::string value;
		try {
			m3c::com_heap_ptr<wchar_t> pwsz;
			M3C_COM_HR(PropVariantToStringAlloc(arg, &pwsz), m3c::evt::PropVariantToStringAlloc, vt);

			value = m3c::EncodeUtf8(pwsz.get());
		} catch (const std::exception&) {
			m3c::Log::ErrorExceptionOnce(m3c::evt::formatter_format);
			goto fallback;
		}
		if (format.empty()) [[likely]] {
			return fmt::format_to(ctx.out(), "({}: {})", vt, value);
		}
		const std::string str = fmt::format("({}: {})", vt, value);
		return fmt::format_to(ctx.out(), format, str);
	}

fallback:
	if (format.empty()) [[likely]] {
		return fmt::format_to(ctx.out(), "({})", vt);
	}
	const std::string str = fmt::format("({})", vt);
	return fmt::format_to(ctx.out(), format, str);
}


//
// IUnknown, IStream
//

namespace {

fmt::format_context::iterator Format(IUnknown* arg, const std::string& format, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!arg) [[unlikely]] {
		if (format.empty()) [[likely]] {
			*ctx.out() = '0';
			*ctx.out() = 'x';
			*ctx.out() = '0';
			return ctx.out();
		}
		return fmt::format_to(ctx.out(), format, "0x0");
	}

	// AddRef to get the ref count
	const ULONG ref = arg->AddRef();
	arg->Release();
	// do not count AddRef
	if (format.empty()) [[likely]] {
		return fmt::format_to(ctx.out(), "(ref={}, this={})", ref - 1, static_cast<void*>(arg));
	}
	const std::string str = fmt::format("(ref={}, this={})", ref - 1, static_cast<void*>(arg));
	return fmt::format_to(ctx.out(), format, str);
}

fmt::format_context::iterator Format(IStream* arg, const std::string& format, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!arg) [[unlikely]] {
		if (format.empty()) [[likely]] {
			*ctx.out() = '0';
			*ctx.out() = 'x';
			*ctx.out() = '0';
			return ctx.out();
		}
		return fmt::format_to(ctx.out(), format, "0x0");
	}

	// AddRef to get the ref count
	const ULONG ref = arg->AddRef();
	arg->Release();

	std::string name;
	try {
		STATSTG statstg{};
		auto f = m3c::finally([&statstg]() noexcept {
			CoTaskMemFree(statstg.pwcsName);
		});
		M3C_COM_HR(arg->Stat(&statstg, STATFLAG_DEFAULT), m3c::evt::IStream_Stat);
		name = statstg.pwcsName ? m3c::EncodeUtf8(statstg.pwcsName) : "(IStream)";
	} catch (const std::exception&) {
		m3c::Log::ErrorExceptionOnce(m3c::evt::formatter_format);
		name = "<Error>";
	}

	// do not count AddRef
	if (format.empty()) [[likely]] {
		return fmt::format_to(ctx.out(), "({}, ref={}, this={})", name, ref - 1, static_cast<void*>(arg));
	}
	const std::string str = fmt::format("({}, ref={}, this={})", name, ref - 1, static_cast<void*>(arg));
	return fmt::format_to(ctx.out(), format, str);
}

}  // namespace


fmt::format_context::iterator fmt::formatter<m3c::fmt_ptr<IUnknown>>::format(const m3c::fmt_ptr<IUnknown>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	// there is nothing such as a const COM object :-)
	return Format(arg.get(), GetFormat(), ctx);
}

fmt::format_context::iterator fmt::formatter<m3c::fmt_ptr<IStream>>::format(const m3c::fmt_ptr<IStream>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	// there is nothing such as a const COM object :-)
	return Format(arg.get(), GetFormat(), ctx);
}


fmt::format_context::iterator fmt::formatter<m3c::com_ptr<IUnknown>>::format(const m3c::com_ptr<IUnknown>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	return Format(arg.get(), GetFormat(), ctx);
}

fmt::format_context::iterator fmt::formatter<m3c::com_ptr<IStream>>::format(const m3c::com_ptr<IStream>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	return Format(arg.get(), GetFormat(), ctx);
}


//
// PROPERTYKEY
//

fmt::format_context::iterator fmt::formatter<PROPERTYKEY>::format(const PROPERTYKEY& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string& format = GetFormat();

	std::string str;
	try {
		m3c::com_heap_ptr<wchar_t> key;
		M3C_COM_HR(PSGetNameFromPropertyKey(arg, &key), m3c::evt::PSGetNameFromPropertyKey, arg.fmtid);
		str = m3c::EncodeUtf8(key.get());
	} catch (std::exception&) {
		m3c::Log::ErrorExceptionOnce(m3c::evt::formatter_format);
		return fmt::format_to(ctx.out(), format.empty() ? "{}" : format, arg.fmtid);
	}

	if (format.empty()) [[likely]] {
		std::copy(str.cbegin(), str.cend(), ctx.out());
	}
	return fmt::format_to(ctx.out(), format, str);
}


//
// WICRect
//

fmt::format_parse_context::iterator fmt::formatter<WICRect>::parse(fmt::format_parse_context& ctx) {
	auto it = ctx.begin();
	const auto last = ctx.end();
	if (it != last && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}
	constexpr std::size_t kGroupingCharCount = 13;
	m_format.reserve((end - it + 3) * 4 + kGroupingCharCount);
	m_format.append("(@({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("}) / {:");
	m_format.append(it, end);
	m_format.append("} x {:");
	m_format.append(it, end);
	m_format.append("})");
	assert(m_format.size() == static_cast<std::size_t>((end - it + 3) * 4 + kGroupingCharCount));
	return end;
}

fmt::format_context::iterator fmt::formatter<WICRect>::format(const WICRect& arg, fmt::format_context& ctx) {
	return fmt::format_to(ctx.out(), m_format, arg.X, arg.Y, arg.Width, arg.Height);
}
