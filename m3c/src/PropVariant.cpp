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

#include <m3c/exception.h>
#include <m3c/types_fmt.h>  // IWYU pragma: keep
#include <m3c/types_log.h>  // IWYU pragma: keep

#include <llamalog/LogLine.h>
#include <llamalog/custom_types.h>
#include <llamalog/llamalog.h>

#include <propidl.h>
#include <windows.h>
#include <wtypes.h>

#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace m3c {

std::string VariantTypeToString(const PROPVARIANT& pv) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
#pragma push_macro("VT_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define VT_(vt_)            \
	case VARENUM::VT_##vt_: \
		type = #vt_;        \
		break

	std::string_view type;
	switch (pv.vt & VARENUM::VT_TYPEMASK) {
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

	switch (pv.vt & ~VARENUM::VT_TYPEMASK) {
		VT_(VECTOR);
		VT_(ARRAY);
		VT_(BYREF);
		VT_(RESERVED);
	}
#pragma pop_macro("VT_")

	return std::string(type);
}

// ensure that abstraction does not add to memory requirements
static_assert(sizeof(PropVariant) == sizeof(PROPVARIANT));

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant() noexcept {
	PropVariantInit(static_cast<PROPVARIANT*>(this));
}

PropVariant::PropVariant(const PropVariant& oth)
	: PropVariant(static_cast<const PROPVARIANT&>(oth)) {
	// empty
}

PropVariant::PropVariant(PropVariant&& oth) noexcept
	: PropVariant(std::move(static_cast<PROPVARIANT&&>(oth))) {
	// empty
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant(const PROPVARIANT& pv) {
	PropVariantInit(static_cast<PROPVARIANT*>(this));
	COM_HR(PropVariantCopy(static_cast<PROPVARIANT*>(this), &pv), "PropVariantCopy for {}", pv);
}

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

PropVariant& PropVariant::operator=(const PropVariant& oth) {
	return *this = static_cast<const PROPVARIANT&>(oth);
}

PropVariant& PropVariant::operator=(PropVariant&& oth) {
	return *this = std::move(static_cast<PROPVARIANT&&>(oth));
}

PropVariant& PropVariant::operator=(const PROPVARIANT& pv) {
	COM_HR(PropVariantClear(static_cast<PROPVARIANT*>(this)), "PropVariantClear for {}", static_cast<const PROPVARIANT&>(*this));
	COM_HR(PropVariantCopy(static_cast<PROPVARIANT*>(this), &pv), "PropVariantCopy for {}", pv);

	return *this;
}

PropVariant& PropVariant::operator=(PROPVARIANT&& pv) {
	COM_HR(PropVariantClear(static_cast<PROPVARIANT*>(this)), "PropVariantClear for {}", static_cast<const PROPVARIANT&>(*this));

	static_assert(std::is_trivially_copyable_v<PROPVARIANT>, "PROPVARIANT is not trivially copyable");
	std::memcpy(static_cast<PROPVARIANT*>(this), &pv, sizeof(PROPVARIANT));

	// PropVariantInit, not PropVariantClear because data has been copied
	PropVariantInit(&pv);

	return *this;
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PropVariant& arg) {
	return logLine.AddCustomArgument(static_cast<const PROPVARIANT&>(arg));
}

}  // namespace m3c
