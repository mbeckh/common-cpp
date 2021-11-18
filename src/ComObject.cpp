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

#include "m3c/ComObject.h"

#include "m3c/COM.h"
#include "m3c/Log.h"
#include "m3c/exception.h"

#include "m3c.events.h"

#include <windows.h>
#include <unknwn.h>

namespace m3c::internal {

HRESULT Unknown::UnknownImpl::QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept {
	return GetAbstractComObject()->QueryInterfaceNonDelegated(riid, ppObject);
}
ULONG Unknown::UnknownImpl::AddRef() noexcept {
	return GetAbstractComObject()->AddRefNonDelegated();
}
ULONG Unknown::UnknownImpl::Release() noexcept {
	return GetAbstractComObject()->ReleaseNonDelegated();
}

AbstractComObject* Unknown::UnknownImpl::GetAbstractComObject() noexcept {
	static_assert(offsetof(Unknown, m_unknown) == 0);
#pragma warning(suppress : 26491)  // valid as checked by static_assert
	return static_cast<AbstractComObject*>(reinterpret_cast<Unknown*>(this));
}


AbstractComObject::AbstractComObject() noexcept {
	InterlockedIncrement(&COM::s_objectCount);
}

AbstractComObject::AbstractComObject(_In_opt_ IUnknown* const pOuter) noexcept
    : m_pUnknown(pOuter ? pOuter : &m_unknown) {
	InterlockedIncrement(&COM::s_objectCount);
}

AbstractComObject::~AbstractComObject() noexcept {
	InterlockedDecrement(&COM::s_objectCount);
}

//
// Non-Delegated methods of IUnknown
//

HRESULT AbstractComObject::QueryInterfaceNonDelegated(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept {
	if (!ppObject) {
		[[unlikely]];
		return Log::TraceHResult(E_INVALIDARG, "hr={}, riid={}", riid);
	}

	if (IsEqualIID(riid, IID_IUnknown)) {
		*ppObject = &m_unknown;
		AddRefNonDelegated();
		return Log::TraceHResult(S_OK, "hr={}, riid={}", riid);
	}

	*ppObject = FindInterfaceInternal(riid);
	if (*ppObject) {
		[[likely]];
		static_cast<IUnknown*>(*ppObject)->AddRef();
		return Log::TraceHResult(S_OK, "hr={}, riid={}", riid);
	}
	return Log::TraceHResult(E_NOINTERFACE, "hr={}, riid={}", riid);
}

ULONG AbstractComObject::AddRefNonDelegated() noexcept {
	return Log::TraceResult(InterlockedIncrement(&m_refCount), "ref={}, this={}", static_cast<const void*>(this));
}

ULONG AbstractComObject::ReleaseNonDelegated() noexcept {
	const void* const ths = this;  // do not touch this after possible deletion

	const ULONG refCount = InterlockedDecrement(&m_refCount);
	if (!refCount) {
		// Required as of https://docs.microsoft.com/en-us/windows/win32/com/aggregation
		// Inner object might trigger calls to AddRef and Release on itself
		InterlockedIncrement(&m_refCount);
		delete this;
	}
	return Log::TraceResult(refCount, "ref={}, this={}, objects={}", ths, COM::s_objectCount);
}

//
// Custom methods
//

_Ret_notnull_ void* AbstractComObject::QueryInterface(REFIID riid) {
	if (IsEqualIID(riid, IID_IUnknown)) {
		AddRefNonDelegated();
		return &m_unknown;
	}

	void* const pInterface = FindInterfaceInternal(riid);
	if (pInterface) {
		[[likely]];
		static_cast<IUnknown*>(pInterface)->AddRef();
		return pInterface;
	}
	throw com_error(E_NOINTERFACE) + evt::IUnknown_QueryInterface_H << riid;
}

}  // namespace m3c::internal
