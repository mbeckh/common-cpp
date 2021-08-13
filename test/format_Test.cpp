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

#include "m3c/format.h"  // IWYU pragma: keep

#include "testevents.h"

#include "m3c/PropVariant.h"
#include "m3c/com_ptr.h"

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <windows.h>
#include <detours_gmock.h>
#include <objbase.h>
#include <objidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#include <rpc.h>
#include <sddl.h>
#include <unknwn.h>
#include <wincodec.h>
#include <wtypes.h>

#include <span>
#include <string>

static bool operator==(const FILETIME& lhs, const FILETIME& rhs) noexcept {
	return lhs.dwLowDateTime == rhs.dwLowDateTime && lhs.dwHighDateTime == rhs.dwHighDateTime;
}

#define WIN32_FUNCTIONS(fn_)                                                                                                                                                                         \
	fn_(8, ULONG, __stdcall, EventWriteEx,                                                                                                                                                           \
	    (REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor, ULONG64 Filter, ULONG Flags, LPCGUID ActivityId, LPCGUID RelatedActivityId, ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData), \
	    (RegHandle, EventDescriptor, Filter, Flags, ActivityId, RelatedActivityId, UserDataCount, UserData),                                                                                         \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, RPC_STATUS, RPC_ENTRY, UuidToStringA,                                                                                                                                                     \
	    (const UUID* Uuid, RPC_CSTR* StringUuid),                                                                                                                                                    \
	    (Uuid, StringUuid),                                                                                                                                                                          \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, BOOL, WINAPI, FileTimeToSystemTime,                                                                                                                                                       \
	    (const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime),                                                                                                                                     \
	    (lpFileTime, lpSystemTime),                                                                                                                                                                  \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, BOOL, WINAPI, ConvertSidToStringSidA,                                                                                                                                                     \
	    (PSID Sid, LPSTR * StringSid),                                                                                                                                                               \
	    (Sid, StringSid),                                                                                                                                                                            \
	    nullptr);                                                                                                                                                                                    \
	fn_(7, DWORD, WINAPI, FormatMessageA,                                                                                                                                                            \
	    (DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list * Arguments),                                                                  \
	    (dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments),                                                                                                                  \
	    nullptr);                                                                                                                                                                                    \
	fn_(1, HLOCAL, WINAPI, LocalFree,                                                                                                                                                                \
	    (HLOCAL hMem),                                                                                                                                                                               \
	    (hMem),                                                                                                                                                                                      \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, HRESULT, STDAPICALLTYPE, PropVariantToStringAlloc,                                                                                                                                        \
	    (REFPROPVARIANT propvar, PWSTR * ppszOut),                                                                                                                                                   \
	    (propvar, ppszOut),                                                                                                                                                                          \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, HRESULT, STDAPICALLTYPE, PSGetNameFromPropertyKey,                                                                                                                                        \
	    (REFPROPERTYKEY propkey, PWSTR * ppszCanonicalName),                                                                                                                                         \
	    (propkey, ppszCanonicalName),                                                                                                                                                                \
	    nullptr)

DTGM_DECLARE_API_MOCK(Win32, WIN32_FUNCTIONS);

namespace m3c::test {

namespace dtgm = detours_gmock;

struct LongSID {
	SID sid;
	DWORD additionalSubAuthorities[1];
};

class format_Test : public t::Test {
protected:
	void TearDown() override {
		DTGM_DETACH_API_MOCK(Win32);
	}

protected:
	DTGM_DEFINE_API_MOCK(Win32, m_win32);
};

//
// GUID
//

TEST_F(format_Test, GUID_Default_Print) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const GUID arg = kGuid;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("a5063846-0d67-4140-8562-af1aaf99a341", str);
}

TEST_F(format_Test, GUID_Centered_PrintCentered) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const GUID arg = kGuid;
		str = fmt::format("{:^38}", arg);
	}

	EXPECT_EQ(" a5063846-0d67-4140-8562-af1aaf99a341 ", str);
}

TEST_F(format_Test, GUID_Error_LogAndPrintFallback) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, UuidToStringA(t::Pointee(kGuid), t::_))
		.Times(2)
		.WillRepeatedly(t::Return(RPC_S_INVALID_ARG));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::UuidToStringX.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const GUID arg = kGuid;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("a5063846-0d67-4140-8562-af1aaf99a341", str);
}

TEST_F(format_Test, GUID_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, UuidToStringA(t::Pointee(kGuid), t::_))
		.Times(2)
		.WillRepeatedly(t::Return(RPC_S_INVALID_ARG));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::UuidToStringX.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const GUID arg = kGuid;
		str = fmt::format("{:^38}", arg);
	}

	EXPECT_EQ(" a5063846-0d67-4140-8562-af1aaf99a341 ", str);
}


//
// FILETIME
//

TEST_F(format_Test, FILETIME_Default_Print) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const FILETIME arg = kFileTime;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("2010-12-27T12:22:06.950Z", str);
}

TEST_F(format_Test, FILETIME_Centered_PrintCentered) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const FILETIME arg = kFileTime;
		str = fmt::format("{:^26}", arg);
	}

	EXPECT_EQ(" 2010-12-27T12:22:06.950Z ", str);
}

TEST_F(format_Test, FILETIME_Error_LogAndPrintFallback) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, FileTimeToSystemTime(t::Pointee(kFileTime), t::_))
		.Times(2)
		.WillRepeatedly(dtgm::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::FileTimeToSystemTime.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const FILETIME arg = kFileTime;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("129379261269507321", str);
}

TEST_F(format_Test, FILETIME_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, FileTimeToSystemTime(t::Pointee(kFileTime), t::_))
		.Times(2)
		.WillRepeatedly(dtgm::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::FileTimeToSystemTime.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const FILETIME arg = kFileTime;
		str = fmt::format("{:^26}", arg);
	}

	EXPECT_EQ("    129379261269507321    ", str);
}


//
// SYSTEMTIME
//

TEST_F(format_Test, SYSTEMTIME_Default_Print) {
	constexpr SYSTEMTIME kSystemTime = {.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const SYSTEMTIME arg = kSystemTime;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("2021-07-31T14:31:26.379Z", str);
}

TEST_F(format_Test, SYSTEMTIME_Centered_PrintCentered) {
	constexpr SYSTEMTIME kSystemTime = {.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const SYSTEMTIME arg = kSystemTime;
		str = fmt::format("{:^26}", arg);
	}

	EXPECT_EQ(" 2021-07-31T14:31:26.379Z ", str);
}


//
// SID
//

TEST_F(format_Test, SID_Default_Print) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const SID& arg = kSid.sid;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("S-1-5-32-547", str);
}

TEST_F(format_Test, SID_Centered_PrintCentered) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		const SID& arg = kSid.sid;
		str = fmt::format("{:^14}", arg);
	}

	EXPECT_EQ(" S-1-5-32-547 ", str);
}

TEST_F(format_Test, SID_Error_LogAndPrintFallback) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, ConvertSidToStringSidA(DTGM_ARG2))
		.Times(2)
		.WillRepeatedly(dtgm::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::ConvertSidToStringSidX.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const SID& arg = kSid.sid;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("S-1-5-32-547", str);
}

TEST_F(format_Test, SID_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, ConvertSidToStringSidA(DTGM_ARG2))
		.Times(2)
		.WillRepeatedly(dtgm::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::ConvertSidToStringSidX.Id), DTGM_ARG6))
		.Times(1);

	std::string str;
	{
		const SID& arg = kSid.sid;
		str = fmt::format("{:^14}", arg);
	}

	EXPECT_EQ(" S-1-5-32-547 ", str);
}

//
// Error codes
//


// ERROR_...

TEST_F(format_Test, win32error_Default_Print) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});

	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_Centered_PrintCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^78}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});

	EXPECT_EQ(" A valid hibernation file has been invalidated and should be abandoned. (787) ", str);
}


// Common error handling

TEST_F(format_Test, win32error_Error_LogAndPrintFallback) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::FormatMessageX.Id), DTGM_ARG6))
		.Times(1);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("<Error> (787)", str);
}

TEST_F(format_Test, win32error_ErrorCentered_LogAndPrintFallbackCentered) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::FormatMessageX.Id), DTGM_ARG6))
		.Times(1);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^78}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("                                <Error> (787)                                 ", str);
}

TEST_F(format_Test, win32error_DynamicBuffer_Print) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_win32, LocalFree(t::NotNull())).Times(1);
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_DynamicBufferFreeError_LogAndPrint) {
	HLOCAL hBuffer = nullptr;

	t::Sequence seq;
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.InSequence(seq)
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0));
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.InSequence(seq)
		.WillOnce(t::Invoke([&hBuffer](DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list* Arguments) {
			const DWORD result = (DTGM_REAL(Win32, FormatMessageA))(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments);
			if (hBuffer) {
				throw std::logic_error("LocalFree");
			}
			hBuffer = *reinterpret_cast<LPSTR*>(lpBuffer);
			return result;
		}));
	EXPECT_CALL(m_win32, LocalFree(t::ResultOf(
							 [&hBuffer](const HLOCAL hMem) noexcept {
								 return hMem == hBuffer;
							 },
							 t::IsTrue())))
		.InSequence(seq)
		.WillOnce(t::DoAll(dtgm::SetLastError(ERROR_IMPLEMENTATION_LIMIT), t::Assign(&hBuffer, nullptr), t::ReturnArg<0>()));

	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::LocalFree.Id), DTGM_ARG6)).Times(1);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_DynamicBufferError_LogAndPrintFallback) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0))
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_win32, LocalFree(t::IsNull())).Times(1);
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::FormatMessageX.Id), DTGM_ARG6))
		.Times(1);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("<Error> (787)", str);
}


// RPC_STATUS

TEST_F(format_Test, rpcstatus_Default_Print) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_rpc_status(RPC_S_OUT_OF_MEMORY));
	});

	EXPECT_EQ("Not enough memory resources are available to complete this operation. (14)", str);
}

TEST_F(format_Test, rpcstatus_Centered_PrintCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^76}", make_rpc_status(RPC_S_OUT_OF_MEMORY));
	});

	EXPECT_EQ(" Not enough memory resources are available to complete this operation. (14) ", str);
}


// HRESULT

TEST_F(format_Test, hresult_Default_Print) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", make_hresult(E_ABORT));
	});

	EXPECT_EQ("Operation aborted (0x80004004)", str);
}

TEST_F(format_Test, hresult_Centered_PrintCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^32}", make_hresult(E_ABORT));
	});

	EXPECT_EQ(" Operation aborted (0x80004004) ", str);
}


//
// PROPVARIANT
//

TEST_F(format_Test, PROPVARIANT_IsBooleanTrue_PrintMinus1) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromBoolean(true, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(BOOL: -1)");
}

TEST_F(format_Test, PROPVARIANT_IsInt16_PrintI2) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromInt16(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I2: 37)");
}


TEST_F(format_Test, PROPVARIANT_IsStringVector_PrintVector) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromStringAsVector(L"red;green;blue", &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(LPWSTR|VECTOR: red; green; blue)");
}

TEST_F(format_Test, PROPVARIANT_IsUInt32Centered_PrintUI4Centered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromUInt32(897, &arg);
		str = fmt::format("{:^12}", arg);
	}

	EXPECT_EQ(str, " (UI4: 897) ");
}

TEST_F(format_Test, PROPVARIANT_IsI2AndError_LogAndPrintFallback) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, PropVariantToStringAlloc(DTGM_ARG2))
		.InSequence(seq)
		.WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::PropVariantToStringAlloc.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::formatter_format.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromInt16(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I2)");
}


//
// PropVariant
//

TEST_F(format_Test, PropVariant_IsEmpty_PrintEmpty) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(EMPTY)");
}

TEST_F(format_Test, PropVariant_IsNull_PrintNull) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_NULL;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(NULL)");
}

TEST_F(format_Test, PropVariant_IsInt16_PrintI2) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromInt16(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I2: 37)");
}

TEST_F(format_Test, PropVariant_IsInt32_PrintI4) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromInt32(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I4: 37)");
}

TEST_F(format_Test, PropVariant_IsFloat_PrintR4) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_R4;
		arg.fltVal = 37;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(R4: 37)");
}

TEST_F(format_Test, PropVariant_IsDouble_PrintR8) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromDouble(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(R8: 37)");
}

TEST_F(format_Test, PropVariant_IsCurrency_PrintCYWithoutValue) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_CY;
		arg.cyVal.int64 = 37;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(CY)");
}

TEST_F(format_Test, PropVariant_IsCurrencyCentered_PrintCYCenteredWithoutValue) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_CY;
		arg.cyVal.int64 = 37;
		str = fmt::format("{:^6}", arg);
	}

	EXPECT_EQ(str, " (CY) ");
}

TEST_F(format_Test, PropVariant_IsDate_PrintDate) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_DATE;
		arg.date = 3456.78;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(DATE: 1909/06/17:18:43:12.000)");
}

TEST_F(format_Test, PropVariant_IsBSTR_PrintBSTR) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_BSTR;
		arg.bstrVal = SysAllocString(L"Test");
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(BSTR: Test)");
}

TEST_F(format_Test, PropVariant_IsError_PrintError) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		arg.vt = VT_ERROR;
		arg.scode = ERROR_ACCOUNT_EXPIRED;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(ERROR)");
}

TEST_F(format_Test, PropVariant_IsBooleanTrue_PrintMinus1) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromBoolean(true, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(BOOL: -1)");
}

TEST_F(format_Test, PropVariant_IsVariant_PrintVariant) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PROPVARIANT variant;
		InitPropVariantFromInt32(37, &variant);

		PropVariant arg;
		arg.vt = VT_VARIANT | VT_BYREF;
		arg.pvarVal = &variant;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(VARIANT|BYREF: 37)");
}

TEST_F(format_Test, PropVariant_IsString_PrintString) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	// longer than the internal buffer in EncodeUtf8
	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromString(std::wstring(512, L'x').c_str(), &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(LPWSTR: " + std::string(512, L'x') + ")");
}

TEST_F(format_Test, PropVariant_IsStringVector_PrintVector) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromStringAsVector(L"red;green;blue", &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(LPWSTR|VECTOR: red; green; blue)");
}


//
// IUnknown
//

TEST_F(format_Test, IUnknown_Empty_PrintNull) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, IUnknown_EmptyCentered_PrintNullCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{:^5}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ(" 0x0 ", str);
}

TEST_F(format_Test, IUnknown_Value_Print) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}
	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueCentered_PrintCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{:^40}", fmt_ptr(ptr.get()));
	}
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(ref=2, this=0x[0-9a-f]+\\)\\s+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, comptrIUnknown_Empty_PrintNull) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{}", ptr);
	}

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, comptrIUnknown_EmptyCentered_PrintNullCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{:^5}", ptr);
	}

	EXPECT_EQ(" 0x0 ", str);
}

TEST_F(format_Test, comptrIUnknown_Value_Print) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, comptrIUnknown_ValueCentered_PrintCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{:^40}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(ref=2, this=0x[0-9a-f]+\\)\\s+"));
	COM_MOCK_VERIFY(object);
}


//
// IStream
//

TEST_F(format_Test, IStream_Empty_PrintNull) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, IStream_EmptyCentered_PrintNullCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{:^5}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ(" 0x0 ", str);
}

TEST_F(format_Test, IStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueWithEmptyName_PrintDefaultName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(nullptr));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(\\(IStream\\), ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueCentered_PrintNameCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{:^60}", fmt_ptr(ptr.get()));
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(test.dat, ref=2, this=0x[0-9a-f]+\\)\\s+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_Error_LogAndPrintFallback) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::Sequence seq;
	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT))
		.InSequence(seq)
		.WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::IStream_Stat.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::formatter_format.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(<Error>, ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, comptrIStream_Empty_PrintNull) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{}", ptr);
	}

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, comptrIStream_EmptyCentered_PrintNullCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{:^5}", ptr);
	}

	EXPECT_EQ(" 0x0 ", str);
}

TEST_F(format_Test, comptrIStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=2, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, comptrIStream_ValueCentered_PrintNameCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Release()).Times(t::Between(1, 2));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{:^60}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(test.dat, ref=2, this=0x[0-9a-f]+\\)\\s+"));
	COM_MOCK_VERIFY(object);
}

//
// PROPERTYKEY
//

TEST_F(format_Test, PROPERTYKEY_IsValue_PrintName) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("System.Contact.BusinessAddress", str);
}

TEST_F(format_Test, PROPERTYKEY_Error_PrintFallback) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, PSGetNameFromPropertyKey(PKEY_Contact_BusinessAddress, t::_))
		.InSequence(seq)
		.WillOnce(t::Return(E_ACCESSDENIED));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::PSGetNameFromPropertyKey.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	std::string str;
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("730fb6dd-cf7c-426b-a03f-bd166cc9ee24", str);
}

TEST_F(format_Test, PROPERTYKEY_IsValueCentered_PrintNameCentered) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		str = fmt::format("{:^32}", arg);
	}

	EXPECT_EQ(" System.Contact.BusinessAddress ", str);
}

TEST_F(format_Test, PROPERTYKEY_ErrorCentered_PrintFallbackCentered) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, PSGetNameFromPropertyKey(t::Ref(PKEY_Contact_BusinessAddress), t::_))
		.InSequence(seq)
		.WillOnce(t::Return(E_ACCESSDENIED));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::PSGetNameFromPropertyKey.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	std::string str;
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		str = fmt::format("{:^38}", arg);
	}

	EXPECT_EQ(" 730fb6dd-cf7c-426b-a03f-bd166cc9ee24 ", str);
}

//
// WICRect
//

TEST_F(format_Test, WICRect_Default_PrintDimensions) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		WICRect arg = {10, 20, 320, 160};
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("(@(10, 20) / 320 x 160)", str);
}

TEST_F(format_Test, WICRect_AsHex_PrintDimensions) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	std::string str;
	{
		WICRect arg = {10, 20, 320, 160};
		str = fmt::format("{:03x}", arg);
	}

	EXPECT_EQ("(@(00a, 014) / 140 x 0a0)", str);
}

}  // namespace m3c::test
