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

#include "m3c/types_fmt.h"

#include "m3c/PropVariant.h"
#include "m3c/com_heap_ptr.h"
#include "m3c/com_ptr.h"
#include "m3c/exception.h"
#include "m3c/finally.h"
#include "m3c/rpc_string.h"
#include "m3c/string_encode.h"

#include <fmt/core.h>
#include <llamalog/llamalog.h>

#include <objbase.h>
#include <propidl.h>
#include <propsys.h>
#include <propvarutil.h>
#include <rpc.h>
#include <wincodec.h>
#include <windows.h>
#include <wtypes.h>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <string>

namespace m3c::internal {

fmt::format_parse_context::iterator BaseFormatter::parse(fmt::format_parse_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	auto it = ctx.begin();
	const auto last = ctx.end();
	if (it != last && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}

	m_format.reserve(end - it + 3);
	m_format.assign("{:");
	m_format.append(it, end);
	m_format.push_back('}');
	return end;
}

}  // namespace m3c::internal

//
// UUID
//

fmt::format_context::iterator fmt::formatter<UUID>::format(const UUID& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	m3c::rpc_string rpc;
	const RPC_STATUS status = UuidToStringA(&arg, &rpc);
	if (status != RPC_S_OK) {
		THROW(m3c::rpc_exception(status), "UuidToStringA: {}", lg::error_code{status});
	}

	return fmt::format_to(ctx.out(), GetFormat(), rpc.as_native());
}


//
// PROPVARIANT
//

fmt::format_context::iterator fmt::formatter<PROPVARIANT>::format(const PROPVARIANT& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	const std::string vt = m3c::VariantTypeToString(arg.vt);
	std::string value;
	try {
		m3c::com_heap_ptr<wchar_t> pwsz;
		COM_HR(PropVariantToStringAlloc(arg, &pwsz), "PropVariantToStringAlloc for {}", vt);

		value = m3c::EncodeUtf8(pwsz.get());
	} catch (const m3c::com_exception& e) {
		LLAMALOG_INTERNAL_ERROR("{}", e);
		return fmt::format_to(ctx.out(), "({})", vt);
	}
	return fmt::format_to(ctx.out(), "({}: {})", vt, value);
}


//
// IUnknown, IStream
//

namespace {

fmt::format_context::iterator Format(IUnknown* arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!arg) {
		*ctx.out() = '0';
		*ctx.out() = 'x';
		*ctx.out() = '0';
		return ctx.out();
	}

	// AddRef to get the ref count
	const ULONG ref = arg->AddRef();
	arg->Release();
	// do not count AddRef and the reference held by the logger
	return fmt::format_to(ctx.out(), "(ref={}, this={})", ref - 2, static_cast<void*>(arg));
}

fmt::format_context::iterator Format(IStream* arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!arg) {
		*ctx.out() = '0';
		*ctx.out() = 'x';
		*ctx.out() = '0';
		return ctx.out();
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
		COM_HR(arg->Stat(&statstg, STATFLAG_DEFAULT), "IStream.Stat");
		name = statstg.pwcsName ? m3c::EncodeUtf8(statstg.pwcsName) : "(IStream)";
	} catch (const m3c::com_exception& e) {
		LLAMALOG_INTERNAL_ERROR("{}", e);
		name = "(ERROR)";
	}


	// do not count AddRef and the reference held by the logger
	return fmt::format_to(ctx.out(), "({}, ref={}, this={})", name, ref - 2, static_cast<void*>(arg));
}

}  // namespace


fmt::format_context::iterator fmt::formatter<m3c::fmt_ptr<IUnknown>>::format(const m3c::fmt_ptr<IUnknown>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	// there is nothing such as a const COM object :-)
	return Format(arg.get(), ctx);
}

fmt::format_context::iterator fmt::formatter<m3c::fmt_ptr<IStream>>::format(const m3c::fmt_ptr<IStream>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	// there is nothing such as a const COM object :-)
	return Format(arg.get(), ctx);
}


fmt::format_context::iterator fmt::formatter<m3c::com_ptr<IUnknown>>::format(const m3c::com_ptr<IUnknown>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	return Format(arg.get(), ctx);
}

fmt::format_context::iterator fmt::formatter<m3c::com_ptr<IStream>>::format(const m3c::com_ptr<IStream>& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	return Format(arg.get(), ctx);
}


//
// PROPERTYKEY
//

fmt::format_context::iterator fmt::formatter<PROPERTYKEY>::format(const PROPERTYKEY& arg, fmt::format_context& ctx) {  // NOLINT(readability-convert-member-functions-to-static): Specialization of fmt::formatter.
	m3c::com_heap_ptr<wchar_t> key;
	COM_HR(PSGetNameFromPropertyKey(arg, &key), "PSGetNameFromPropertyKey");

	return fmt::format_to(ctx.out(), GetFormat(), m3c::EncodeUtf8(key.get()));
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
