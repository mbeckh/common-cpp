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

#include "m3c/ClassFactory.h"

#include "m3c_events.h"

#include "m3c/COM.h"
#include "m3c/ComObject.h"
#include "m3c/Log.h"
#include "m3c/exception.h"
#include "m3c/finally.h"

#include <windows.h>
#include <unknwn.h>

namespace m3c::internal {

//
// IClassFactory
//

HRESULT AbstractClassFactory::CreateInstance(_In_opt_ IUnknown* const pOuter, REFIID riid, _COM_Outptr_ void** const ppObject) noexcept {
	Log::Trace("pOuter={}, riid={}", pOuter, riid);

	try {
		if (!ppObject) {
			[[unlikely]];
			throw com_invalid_argument_error("object") + evt::Default;
		}

		if (pOuter && !IsEqualIID(riid, IID_IUnknown)) {
			[[unlikely]];
			// aggregation requires query for IUnknown during creation
			throw com_invalid_argument_error(E_NOINTERFACE, "outer") + evt::Default;
		}

		AbstractComObject* const pObject = CreateObject();
		const auto release = finally([pObject]() noexcept {
			pObject->ReleaseNonDelegated();
		});
		*ppObject = pObject->QueryInterface(riid);
		if (pOuter) {
			// delegate public IUnknown to outer
			pObject->m_pUnknown = pOuter;
		}
		return S_OK;
	} catch (...) {
		if (ppObject) {
			[[likely]];
			*ppObject = nullptr;
		}
		return Log::ExceptionToHResult(Priority::kError, evt::IClassFactory_CreateInstance_H, riid);
	}
}

HRESULT AbstractClassFactory::LockServer(const BOOL lock) noexcept {
	const ULONG count = lock ? InterlockedIncrement(&COM::s_lockCount) : InterlockedDecrement(&COM::s_lockCount);
	Log::Trace("lock={}, locks={}", lock, count);
	return S_OK;
}

}  // namespace m3c::internal
