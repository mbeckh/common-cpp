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

#include "m3c/PropVariant.h"

#include "m3c/com_heap_ptr.h"
#include "m3c/exception.h"
#include "m3c/string_encode.h"
#include "m3c/types_fmt.h"  // IWYU pragma: keep

#include <fmt/core.h>
#include <llamalog/LogLine.h>
#include <llamalog/custom_types.h>
#include <llamalog/llamalog.h>

#include <oleauto.h>
#include <propidl.h>
#include <propvarutil.h>
#include <windows.h>
#include <wtypes.h>

#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace m3c {

std::string VariantTypeToString(const VARTYPE vt) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
#pragma push_macro("VT_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define VT_(vt_)            \
	case VARENUM::VT_##vt_: \
		type = #vt_;        \
		break

	std::string_view type;
	switch (vt & VARENUM::VT_TYPEMASK) {
		VT_(EMPTY);
		VT_(NULL);
		VT_(I2);
		VT_(I4);
		VT_(R4);
		VT_(R8);
		VT_(CY);
		VT_(DATE);
		VT_(BSTR);
		VT_(DISPATCH);
		VT_(ERROR);
		VT_(BOOL);
		VT_(VARIANT);
		VT_(UNKNOWN);
		VT_(DECIMAL);
		VT_(I1);
		VT_(UI1);
		VT_(UI2);
		VT_(UI4);
		VT_(I8);
		VT_(UI8);
		VT_(INT);
		VT_(UINT);
		VT_(VOID);
		VT_(HRESULT);
		VT_(PTR);
		VT_(SAFEARRAY);
		VT_(CARRAY);
		VT_(USERDEFINED);
		VT_(LPSTR);
		VT_(LPWSTR);
		VT_(RECORD);
		VT_(INT_PTR);
		VT_(UINT_PTR);
		VT_(FILETIME);
		VT_(BLOB);
		VT_(STREAM);
		VT_(STORAGE);
		VT_(STREAMED_OBJECT);
		VT_(STORED_OBJECT);
		VT_(BLOB_OBJECT);
		VT_(CF);
		VT_(CLSID);
		VT_(VERSIONED_STREAM);
		VT_(BSTR_BLOB);
	default:
		return "ILLEGAL";
	}

#undef VT_
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define VT_(vt_)            \
	case VARENUM::VT_##vt_: \
		return std::string(type) += "|" #vt_

	switch (vt & ~VARENUM::VT_TYPEMASK) {
		VT_(VECTOR);
		VT_(ARRAY);
		VT_(BYREF);
		VT_(RESERVED);
	}
#pragma pop_macro("VT_")

	return std::string(type);
}

//
// Variant
//

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): VARIANT is initialized using VariantInit.
Variant::Variant() noexcept {
	// ensure that abstraction does not add to memory requirements
	static_assert(sizeof(Variant) == sizeof(VARIANT));

	VariantInit(static_cast<VARIANT*>(this));
}

Variant::Variant(const Variant& oth)
	: Variant(static_cast<const VARIANT&>(oth)) {
	// empty
}

Variant::Variant(Variant&& oth) noexcept
	: Variant(static_cast<VARIANT&&>(oth)) {
	// empty
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): VARIANT is initialized using VariantInit.
Variant::Variant(const VARIANT& pv) {
	VariantInit(static_cast<VARIANT*>(this));
	COM_HR(VariantCopy(static_cast<VARIANT*>(this), &pv), "VariantCopy");
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): VARIANT is initialized using std::memcpy.
Variant::Variant(VARIANT&& pv) noexcept {
	static_assert(std::is_trivially_copyable_v<VARIANT>, "VARIANT is not trivially copyable");
	std::memcpy(static_cast<VARIANT*>(this), &pv, sizeof(VARIANT));

	// VariantInit, not VariantClear because data has been copied
	VariantInit(&pv);
}

Variant::~Variant() noexcept {
	const HRESULT hr = VariantClear(static_cast<VARIANT*>(this));
	if (FAILED(hr)) {
		SLOG_ERROR("VariantClear: {}", lg::error_code{hr});
	}
}

//
// PropVariant
//

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant() noexcept {
	// ensure that abstraction does not add to memory requirements
	static_assert(sizeof(PropVariant) == sizeof(PROPVARIANT));

	PropVariantInit(static_cast<PROPVARIANT*>(this));
}

PropVariant::PropVariant(const PropVariant& oth)
	: PropVariant(static_cast<const PROPVARIANT&>(oth)) {
	// empty
}

PropVariant::PropVariant(PropVariant&& oth) noexcept
	: PropVariant(static_cast<PROPVARIANT&&>(oth)) {
	// empty
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant(const PROPVARIANT& pv) {
	PropVariantInit(static_cast<PROPVARIANT*>(this));
	COM_HR(PropVariantCopy(static_cast<PROPVARIANT*>(this), &pv), "PropVariantCopy");
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using std::memcpy.
PropVariant::PropVariant(PROPVARIANT&& pv) noexcept {
	static_assert(std::is_trivially_copyable_v<PROPVARIANT>, "PROPVARIANT is not trivially copyable");
	std::memcpy(static_cast<PROPVARIANT*>(this), &pv, sizeof(PROPVARIANT));

	// PropVariantInit, not PropVariantClear because data has been copied
	PropVariantInit(&pv);
}

PropVariant::~PropVariant() noexcept {
	const HRESULT hr = PropVariantClear(static_cast<PROPVARIANT*>(this));
	if (FAILED(hr)) {
		SLOG_ERROR("PropVariantClear: {}", lg::error_code{hr});
	}
}

namespace {

/// @brief Helper type for formatting `PropVariant` objects with reference types.
struct VariantWrapper {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	/// @brief Create a new `VariantWrapper` object.
	/// @param varType The `VARTYPE` of the original object.
	explicit VariantWrapper(VARTYPE varType) noexcept
		: vt(varType) {
		// empty
	}
	VARTYPE vt;       ///< @brief The type of the original `PropVariant`.
	Variant variant;  ///< @brief The data container for logging.
};

}  // namespace

}  // namespace m3c


/// @brief A formatter for `m3c::VariantWrapper` objects.
template <>
struct fmt::formatter<m3c::VariantWrapper> {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	// NOLINTNEXTLINE(readability-convert-member-functions-to-static=: Specialization of fmt::formatter.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) {
		return ctx.end();
	}

	/// @brief Format the `m3c::VariantWrapper`.
	/// @param arg A `m3c::VariantWrapper` object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	// NOLINTNEXTLINE(readability-convert-member-functions-to-static=: Specialization of fmt::formatter.
	fmt::format_context::iterator format(const m3c::VariantWrapper& arg, fmt::format_context& ctx) {
		const std::string vt = m3c::VariantTypeToString(arg.vt);
		std::string value;
		try {
			m3c::com_heap_ptr<wchar_t> pwsz;
			COM_HR(VariantToStringAlloc(arg.variant, &pwsz), "VariantToStringAlloc for {}", vt);

			value = m3c::EncodeUtf8(pwsz.get());
		} catch (const m3c::com_exception& e) {
			LLAMALOG_INTERNAL_ERROR("{}", e);
			return fmt::format_to(ctx.out(), "({})", vt);
		}

		return fmt::format_to(ctx.out(), "({}: {})", vt, value);
	}
};

namespace m3c {

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PropVariant& arg) {
	if (arg.vt & VT_BYREF) {
		// ensure that references are still valid when logging asynchronously
		assert((arg.vt & VT_VECTOR) == 0);

		Variant variant;
		COM_HR(PropVariantToVariant(&arg, &variant), "PropVariantToVariant for {}", arg.GetVariantType());

		VariantWrapper wrapper(arg.vt);
		COM_HR(VariantCopyInd(&wrapper.variant, &variant), "VariantCopyInd for {}", variant.GetVariantType());

		return logLine.AddCustomArgument(wrapper);
	}

	return logLine.AddCustomArgument(arg);
}

}  // namespace m3c
