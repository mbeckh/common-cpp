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

#include "m3c/ClassFactory.h"

#include "m3c/COM.h"
#include "m3c/ComObject.h"
#include "m3c/exception.h"
#include "m3c/finally.h"
#include "m3c/types_log.h"  // IWYU pragma: keep

#include <llamalog/llamalog.h>

namespace m3c::internal {

//
// IClassFactory
//
HRESULT AbstractClassFactory::CreateInstance(_In_opt_ IUnknown* pOuter, REFIID riid, _COM_Outptr_ void** ppObject) noexcept {
	LOG_TRACE("pOuter={}, riid={}", pOuter, riid);

	try {
		if (!ppObject) {
			THROW(com_invalid_argument_exception("ppObject"));
		}

		if (pOuter) {
			THROW(com_invalid_argument_exception(CLASS_E_NOAGGREGATION), "pOuter={}", pOuter);
		}

		AbstractComObject* const pObject = CreateObject();
		auto f = finally([pObject]() noexcept {
			pObject->Release();
		});
		*ppObject = pObject->QueryInterface(riid);
		return S_OK;
	} catch (...) {
		if (ppObject) {
			*ppObject = nullptr;
		}
		return AS_HRESULT("IID={}: {}", riid);
	}
}

HRESULT AbstractClassFactory::LockServer(BOOL lock) noexcept {
	LOG_TRACE_RESULT(lock ? InterlockedIncrement(&COM::m_lockCount) : InterlockedDecrement(&COM::m_lockCount), "lock={1}, locks={0}");
	return S_OK;
}

}  // namespace m3c::internal
