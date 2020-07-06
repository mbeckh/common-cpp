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

#pragma once

#include <m3c/com_heap_ptr.h>

#include <gmock/gmock.h>

#include <objidl.h>
#include <windows.h>

#include <cstring>
#include <cwchar>
#include <type_traits>

namespace m4t {

/// @brief Mock class for `IStream`.
class IStream_Mock : public IStream {
public:
	IStream_Mock();
	virtual ~IStream_Mock();

public:  // IUnknown
	MOCK_METHOD2_WITH_CALLTYPE(__stdcall, QueryInterface, HRESULT(REFIID riid, void** ppvObject));
	MOCK_METHOD0_WITH_CALLTYPE(__stdcall, AddRef, ULONG());
	MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Release, ULONG());

public:  // ISequentialStream
	MOCK_METHOD3_WITH_CALLTYPE(__stdcall, Read, HRESULT(void* pv, ULONG cb, ULONG* pcbRead));
	MOCK_METHOD3_WITH_CALLTYPE(__stdcall, Write, HRESULT(const void* pv, ULONG cb, ULONG* pcbWritten));

public:  // IStream
	MOCK_METHOD3_WITH_CALLTYPE(__stdcall, Seek, HRESULT(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition));
	MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetSize, HRESULT(ULARGE_INTEGER libNewSize));
	MOCK_METHOD4_WITH_CALLTYPE(__stdcall, CopyTo, HRESULT(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten));
	MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Commit, HRESULT(DWORD grfCommitFlags));
	MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Revert, HRESULT());
	MOCK_METHOD3_WITH_CALLTYPE(__stdcall, LockRegion, HRESULT(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType));
	MOCK_METHOD3_WITH_CALLTYPE(__stdcall, UnlockRegion, HRESULT(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType));
	MOCK_METHOD2_WITH_CALLTYPE(__stdcall, Stat, HRESULT(STATSTG* pstatstg, DWORD grfStatFlag));
	MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Clone, HRESULT(IStream** ppstm));
};

/// @brief Default action for `IStream::Stat`.
#pragma warning(suppress : 4100)
ACTION_P(IStream_Stat, wszName) {
	static_assert(std::is_same_v<STATSTG*, arg0_type>);
	static_assert(std::is_convertible_v<wszName_type, const wchar_t*>);

	const wchar_t* wsz = wszName;
	const size_t cch = wcslen(wsz) + 1;
	m3c::com_heap_ptr<wchar_t> pName(cch);
	std::memcpy(pName.get(), wsz, cch * sizeof(wchar_t));
	arg0->pwcsName = pName.release();

	return S_OK;
}

}  // namespace m4t
