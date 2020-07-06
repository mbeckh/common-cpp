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

#include "m3c/exception.h"
#include "m3c/types_log.h"

#include <llamalog/llamalog.h>

#include <unknwn.h>
#include <windows.h>


namespace m3c {

AbstractComObject::AbstractComObject() noexcept {
	InterlockedIncrement(&m_objectCount);
}

AbstractComObject::~AbstractComObject() noexcept {
	InterlockedDecrement(&m_objectCount);
}


//
// IUnknown
//

HRESULT AbstractComObject::QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept {
	if (!ppObject) {
		return E_INVALIDARG;
	}

	*ppObject = FindInterface(riid);
	if (*ppObject) {
		static_cast<IUnknown*>(*ppObject)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG AbstractComObject::AddRef() noexcept {
	return InterlockedIncrement(&m_refCount);
}

ULONG AbstractComObject::Release() noexcept {
	const ULONG refCount = InterlockedDecrement(&m_refCount);
	if (!refCount) {
		delete this;
	}
	return refCount;
}

//
// Custom methods
//

_Ret_notnull_ void* AbstractComObject::QueryInterface(REFIID riid) {
	void* const pInterface = FindInterface(riid);
	if (pInterface) {
		static_cast<IUnknown*>(pInterface)->AddRef();
		return pInterface;
	}
	THROW(com_exception(E_NOINTERFACE), "IID={}", riid);
}

}  // namespace m3c
