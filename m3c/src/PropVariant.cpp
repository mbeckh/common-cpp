/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

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

#include <llamalog/custom_types.h>
#include <llamalog/llamalog.h>

#include <propidl.h>
#include <windows.h>
#include <wtypes.h>

#include <cstring>
#include <string>
#include <string_view>

namespace m3c {

std::string VariantTypeToString(const PROPVARIANT& pv) {
#pragma push_macro("VT_")
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


PropVariant::PropVariant() noexcept {
	PropVariantInit(this);
}

PropVariant::PropVariant(const PropVariant& pv)
	: PropVariant(static_cast<const PROPVARIANT&>(pv)) {
	// empty
}

PropVariant::PropVariant(PropVariant&& pv) noexcept
	: PropVariant(static_cast<const PROPVARIANT&&>(pv)) {
	// empty
}

PropVariant::PropVariant(const PROPVARIANT& pv) {
	PropVariantInit(this);
	COM_HR(PropVariantCopy(this, &pv), "PropVariantCopy for {}", pv);
}

PropVariant::PropVariant(PROPVARIANT&& pv) noexcept {
	std::memcpy(this, &pv, sizeof(PROPVARIANT));
	PropVariantInit(&pv);
}

PropVariant::~PropVariant() noexcept {
	const HRESULT hr = PropVariantClear(this);
	if (FAILED(hr)) {
		LOG_ERROR("PropVariantClear: {}", lg::error_code{hr});
	}
}

}  // namespace m3c

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const m3c::PropVariant& arg) {
	return logLine.AddCustomArgument(arg);
}
