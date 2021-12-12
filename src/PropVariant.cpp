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

#include "m3c/PropVariant.h"

#include "m3c_events.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <windows.h>
#include <oleauto.h>
#include <propidl.h>
#include <wtypes.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace m3c {

std::string VariantTypeToString(const VARTYPE vt) {
#pragma push_macro("VT")
#define VT(vt_)             \
	case VARENUM::VT_##vt_: \
		type = #vt_;        \
		break

	std::string_view type;
	switch (vt & VARENUM::VT_TYPEMASK) {
		VT(EMPTY);
		VT(NULL);
		VT(I2);
		VT(I4);
		VT(R4);
		VT(R8);
		VT(CY);
		VT(DATE);
		VT(BSTR);
		VT(DISPATCH);
		VT(ERROR);
		VT(BOOL);
		VT(VARIANT);
		VT(UNKNOWN);
		VT(DECIMAL);
		VT(I1);
		VT(UI1);
		VT(UI2);
		VT(UI4);
		VT(I8);
		VT(UI8);
		VT(INT);
		VT(UINT);
		VT(VOID);
		VT(HRESULT);
		VT(PTR);
		VT(SAFEARRAY);
		VT(CARRAY);
		VT(USERDEFINED);
		VT(LPSTR);
		VT(LPWSTR);
		VT(RECORD);
		VT(INT_PTR);
		VT(UINT_PTR);
		VT(FILETIME);
		VT(BLOB);
		VT(STREAM);
		VT(STORAGE);
		VT(STREAMED_OBJECT);
		VT(STORED_OBJECT);
		VT(BLOB_OBJECT);
		VT(CF);
		VT(CLSID);
		VT(VERSIONED_STREAM);
		VT(BSTR_BLOB);
		[[unlikely]] default : return FMT_FORMAT("ILLEGAL(0x{:x})", vt);
	}

#undef VT
#define VT(vt_)             \
	case VARENUM::VT_##vt_: \
		return std::string(type) += "|" #vt_

	switch (vt & ~VARENUM::VT_TYPEMASK) {
	case 0:
		return std::string(type);
		VT(VECTOR);
		VT(ARRAY);
		VT(BYREF);
		VT(RESERVED);
		[[unlikely]] default : return FMT_FORMAT("ILLEGAL(0x{:x})", vt);
	}
#pragma pop_macro("VT")
}


//
// Variant
//

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): VARIANT is initialized using VariantInit.
Variant::Variant() noexcept {
	// ensure that abstraction does not add to memory requirements
	static_assert(sizeof(Variant) == sizeof(VARIANT));

	VariantInit(this);
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): VARIANT is initialized using VariantInit.
Variant::Variant(const VARIANT& var) {
	VariantInit(this);
	M3C_COM_HR(VariantCopy(this, &var), evt::VariantCopy_H, var.vt);
}

Variant::Variant(VARIANT&& var) noexcept
    : VARIANT(var) {  // copy as bytes
	static_assert(std::is_trivially_copyable_v<VARIANT>, "VARIANT is not trivially copyable");

	// VariantInit, not VariantClear because data has been moved
	VariantInit(&var);
}

Variant::Variant(const Variant& oth)
    : Variant(static_cast<const VARIANT&>(oth)) {
	// empty
}

Variant::Variant(Variant&& oth) noexcept
    : Variant(static_cast<VARIANT&&>(oth)) {
	// empty
}

Variant::~Variant() noexcept {
	const HRESULT hr = VariantClear(this);
	if (FAILED(hr)) {
		[[unlikely]];
		Log::Error(evt::VariantLeak_H, vt, hresult(hr));
	}
}


//
// PropVariant
//

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant() noexcept {
	// ensure that abstraction does not add to memory requirements
	static_assert(sizeof(PropVariant) == sizeof(PROPVARIANT));

	PropVariantInit(this);
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): PROPVARIANT is initialized using PropVariantInit.
PropVariant::PropVariant(const PROPVARIANT& pv) {
	PropVariantInit(this);
	M3C_COM_HR(PropVariantCopy(this, &pv), evt::VariantCopy_H, pv.vt);
}

PropVariant::PropVariant(PROPVARIANT&& pv) noexcept
    : PROPVARIANT(pv) {  // copy as bytes
	static_assert(std::is_trivially_copyable_v<PROPVARIANT>, "PROPVARIANT is not trivially copyable");

	// PropVariantInit, not PropVariantClear because data has been moved
	PropVariantInit(&pv);
}

PropVariant::PropVariant(const PropVariant& oth)
    : PropVariant(static_cast<const PROPVARIANT&>(oth)) {
	// empty
}

PropVariant::PropVariant(PropVariant&& oth) noexcept
    : PropVariant(static_cast<PROPVARIANT&&>(oth)) {
	// empty
}

PropVariant::~PropVariant() noexcept {
	const HRESULT hr = PropVariantClear(this);
	if (FAILED(hr)) {
		[[unlikely]];
		Log::Error(evt::VariantLeak_H, vt, hresult(hr));
	}
}

}  // namespace m3c

template struct fmt::formatter<m3c::Variant, char>;
template struct fmt::formatter<m3c::Variant, wchar_t>;
template struct fmt::formatter<m3c::PropVariant, char>;
template struct fmt::formatter<m3c::PropVariant, wchar_t>;
