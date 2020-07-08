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
#include "m3c/exception.h"
#include "m3c/types_log.h"

#include <llamalog/llamalog.h>

#include <unknwn.h>
#include <windows.h>


namespace m3c::internal {

AbstractComObject::AbstractComObject() noexcept {
	InterlockedIncrement(&COM::m_objectCount);
}

AbstractComObject::~AbstractComObject() noexcept {
	InterlockedDecrement(&COM::m_objectCount);
}

//
// IUnknown
//

HRESULT AbstractComObject::QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	if (!ppObject) {
		return LOG_TRACE_HRESULT(E_INVALIDARG, "hr={}, riid={}", riid);
	}

	*ppObject = FindInterface(riid);
	if (*ppObject) {
		static_cast<IUnknown*>(*ppObject)->AddRef();
		return LOG_TRACE_HRESULT(S_OK, "hr={}, riid={}", riid);
	}
	return LOG_TRACE_HRESULT(E_NOINTERFACE, "hr={}, riid={}", riid);
}

ULONG AbstractComObject::AddRef() noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return LOG_TRACE_RESULT(InterlockedIncrement(&m_refCount), "ref={}, this={}", static_cast<const void*>(this));
}

ULONG AbstractComObject::Release() noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	const void* const ths = this;              // do not touch this after possible deletion
	const ULONG refCount = InterlockedDecrement(&m_refCount);
	if (!refCount) {
		delete this;
	}
	return LOG_TRACE_RESULT(refCount, "ref={}, this={}, objects={}", ths, COM::m_objectCount);
}

//
// Custom methods
//

_Ret_notnull_ void* AbstractComObject::QueryInterface(REFIID riid) {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	void* const pInterface = FindInterface(riid);
	if (pInterface) {
		static_cast<IUnknown*>(pInterface)->AddRef();
		return pInterface;
	}
	THROW(com_exception(E_NOINTERFACE), "IID={}", riid);
}

}  // namespace m3c::internal
