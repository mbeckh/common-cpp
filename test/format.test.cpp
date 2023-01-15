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

#include "m3c/format.h"

#include "m3c/PropVariant.h"
#include "m3c/com_ptr.h"

#include <m4t/IStreamMock.h>
#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

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


namespace m3c::test {
namespace {

namespace t = testing;

#undef WIN32_FUNCTIONS
#define WIN32_FUNCTIONS(fn_)                                                                                                        \
	fn_(2, RPC_STATUS, RPC_ENTRY, UuidToStringA,                                                                                    \
	    (const UUID* Uuid, RPC_CSTR* StringUuid),                                                                                   \
	    (Uuid, StringUuid),                                                                                                         \
	    nullptr);                                                                                                                   \
	fn_(2, BOOL, WINAPI, FileTimeToSystemTime,                                                                                      \
	    (const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime),                                                                    \
	    (lpFileTime, lpSystemTime),                                                                                                 \
	    nullptr);                                                                                                                   \
	fn_(2, BOOL, WINAPI, ConvertSidToStringSidA,                                                                                    \
	    (PSID Sid, LPSTR * StringSid),                                                                                              \
	    (Sid, StringSid),                                                                                                           \
	    nullptr);                                                                                                                   \
	fn_(7, DWORD, WINAPI, FormatMessageA,                                                                                           \
	    (DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, va_list * Arguments), \
	    (dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize, Arguments),                                                 \
	    nullptr);                                                                                                                   \
	fn_(1, HLOCAL, WINAPI, LocalFree,                                                                                               \
	    (HLOCAL hMem),                                                                                                              \
	    (hMem),                                                                                                                     \
	    nullptr);                                                                                                                   \
	fn_(2, HRESULT, STDAPICALLTYPE, PropVariantToStringAlloc,                                                                       \
	    (REFPROPVARIANT propvar, PWSTR * ppszOut),                                                                                  \
	    (propvar, ppszOut),                                                                                                         \
	    nullptr);                                                                                                                   \
	fn_(2, HRESULT, STDAPICALLTYPE, PSGetNameFromPropertyKey,                                                                       \
	    (REFPROPERTYKEY propkey, PWSTR * ppszCanonicalName),                                                                        \
	    (propkey, ppszCanonicalName),                                                                                               \
	    nullptr)


struct LongSID {
	SID sid;
	DWORD additionalSubAuthorities[1];
};

class format_Test : public t::Test {
protected:
	DTGM_API_MOCK(m_win32, WIN32_FUNCTIONS);
	t::MockFunction<void()> m_check;
	m4t::LogListener m_log = m4t::LogListenerMode::kStrictEvent;
};


//
// Encode
//

TEST_F(format_Test, Encode_EmptyS2S_Empty) {
	const std::string str = fmt::to_string(fmt_encode(""));

	EXPECT_EQ("", str);
}

TEST_F(format_Test, Encode_ValueS2S_Value) {
	const std::string str = fmt::to_string(fmt_encode("Test"));

	EXPECT_EQ("Test", str);
}

TEST_F(format_Test, Encode_EmptyS2W_Empty) {
	const std::wstring str = fmt::to_wstring(fmt_encode(""));

	EXPECT_EQ(L"", str);
}

TEST_F(format_Test, Encode_ValueS2W_Value) {
	const std::wstring str = fmt::to_wstring(fmt_encode("Test"));

	EXPECT_EQ(L"Test", str);
}

TEST_F(format_Test, Encode_EmptyW2S_Empty) {
	const std::string str = fmt::to_string(fmt_encode(L""));

	EXPECT_EQ("", str);
}

TEST_F(format_Test, Encode_ValueW2S_Value) {
	const std::string str = fmt::to_string(fmt_encode(L"Test"));

	EXPECT_EQ("Test", str);
}

TEST_F(format_Test, Encode_EmptyW2W_Empty) {
	const std::wstring str = fmt::to_wstring(fmt_encode(L""));

	EXPECT_EQ(L"", str);
}

TEST_F(format_Test, Encode_ValueW2W_Value) {
	const std::wstring str = fmt::to_wstring(fmt_encode(L"Test"));

	EXPECT_EQ(L"Test", str);
}


//
// GUID
//

TEST_F(format_Test, GUID_Default_Print) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	const std::string str = fmt::to_string(kGuid);

	EXPECT_EQ("a5063846-0d67-4140-8562-af1aaf99a341", str);
}

TEST_F(format_Test, GUID_DefaultW_Print) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	const std::wstring str = fmt::to_wstring(kGuid);

	EXPECT_EQ(L"a5063846-0d67-4140-8562-af1aaf99a341", str);
}

TEST_F(format_Test, GUID_Centered_PrintCentered) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	const std::string str = fmt::format("{:^38}", kGuid);

	EXPECT_EQ(" a5063846-0d67-4140-8562-af1aaf99a341 ", str);
}

TEST_F(format_Test, GUID_Error_LogAndPrintFallback) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, UuidToStringA(t::Pointee(kGuid), t::_))
	    .Times(2)
	    .WillRepeatedly(t::Return(RPC_S_INVALID_ARG));
	EXPECT_CALL(m_log, Event(evt::FormatUuid_R.Id, DTGM_ARG3));

	const std::string str = fmt::to_string(kGuid);

	EXPECT_EQ("a5063846-0d67-4140-8562-af1aaf99a341", str);
}

TEST_F(format_Test, GUID_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr GUID kGuid = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};

	EXPECT_CALL(m_win32, UuidToStringA(t::Pointee(kGuid), t::_))
	    .Times(2)
	    .WillRepeatedly(t::Return(RPC_S_INVALID_ARG));
	EXPECT_CALL(m_log, Event(evt::FormatUuid_R.Id, DTGM_ARG3));

	const std::string str = fmt::format("{:^38}", kGuid);

	EXPECT_EQ(" a5063846-0d67-4140-8562-af1aaf99a341 ", str);
}


//
// FILETIME
//

TEST_F(format_Test, FILETIME_Default_Print) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	const std::string str = fmt::to_string(kFileTime);

	EXPECT_EQ("2010-12-27T12:22:06.950Z", str);
}

TEST_F(format_Test, FILETIME_DefaultW_Print) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	const std::wstring str = fmt::to_wstring(kFileTime);

	EXPECT_EQ(L"2010-12-27T12:22:06.950Z", str);
}

TEST_F(format_Test, FILETIME_Centered_PrintCentered) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	const std::string str = fmt::format("{:^26}", kFileTime);

	EXPECT_EQ(" 2010-12-27T12:22:06.950Z ", str);
}

TEST_F(format_Test, FILETIME_Error_LogAndPrintFallback) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, FileTimeToSystemTime(t::Pointee(kFileTime), t::_))
	    .Times(2)
	    .WillRepeatedly(m4t::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_log, Event(evt::FormatFileTime_E.Id, DTGM_ARG3));

	const std::string str = fmt::to_string(kFileTime);

	EXPECT_EQ("129379261269507321", str);
}

TEST_F(format_Test, FILETIME_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr FILETIME kFileTime = {.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};

	EXPECT_CALL(m_win32, FileTimeToSystemTime(t::Pointee(kFileTime), t::_))
	    .Times(2)
	    .WillRepeatedly(m4t::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_log, Event(evt::FormatFileTime_E.Id, DTGM_ARG3));

	const std::string str = fmt::format("{:^26}", kFileTime);

	EXPECT_EQ("    129379261269507321    ", str);
}


//
// SYSTEMTIME
//

TEST_F(format_Test, SYSTEMTIME_Default_Print) {
	constexpr SYSTEMTIME kSystemTime = {.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};

	const std::string str = fmt::to_string(kSystemTime);

	EXPECT_EQ("2021-07-31T14:31:26.379Z", str);
}

TEST_F(format_Test, SYSTEMTIME_DefaultW_Print) {
	constexpr SYSTEMTIME kSystemTime = {.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};

	const std::wstring str = fmt::to_wstring(kSystemTime);

	EXPECT_EQ(L"2021-07-31T14:31:26.379Z", str);
}

TEST_F(format_Test, SYSTEMTIME_Centered_PrintCentered) {
	constexpr SYSTEMTIME kSystemTime = {.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};

	const std::string str = fmt::format("{:^26}", kSystemTime);

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

	const SID& arg = kSid.sid;
	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("S-1-5-32-547", str);
}

TEST_F(format_Test, SID_DefaultW_Print) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	const SID& arg = kSid.sid;
	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"S-1-5-32-547", str);
}

TEST_F(format_Test, SID_Centered_PrintCentered) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	const SID& arg = kSid.sid;
	const std::string str = fmt::format("{:^14}", arg);

	EXPECT_EQ(" S-1-5-32-547 ", str);
}

TEST_F(format_Test, SID_Error_LogAndPrintFallback) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, ConvertSidToStringSidA)
	    .Times(2)
	    .WillRepeatedly(m4t::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_log, Event(evt::FormatSid_E.Id, DTGM_ARG3));

	const SID& arg = kSid.sid;
	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("S-1-5-32-547", str);
}

TEST_F(format_Test, SID_ErrorCentered_LogAndPrintFallbackCentered) {
	constexpr LongSID kSid{.sid = {.Revision = SID_REVISION,
	                               .SubAuthorityCount = 2,
	                               .IdentifierAuthority = SECURITY_NT_AUTHORITY,
	                               .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
	                       .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

	EXPECT_CALL(m_win32, ConvertSidToStringSidA)
	    .Times(2)
	    .WillRepeatedly(m4t::SetLastErrorAndReturn(ERROR_DIRECTORY, FALSE));
	EXPECT_CALL(m_log, Event(evt::FormatSid_E.Id, DTGM_ARG3));

	const SID& arg = kSid.sid;
	const std::string str = fmt::format("{:^14}", arg);

	EXPECT_EQ(" S-1-5-32-547 ", str);
}


//
// Error codes
//


// ERROR_...

TEST_F(format_Test, win32error_Default_Print) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(win32_error(ERROR_ABANDON_HIBERFILE));
	});

	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_DefaultW_Print) {
	const std::wstring str = m4t::WithLocale("en-US", [] {
		return fmt::to_wstring(win32_error(ERROR_ABANDON_HIBERFILE));
	});

	EXPECT_EQ(L"A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_Centered_PrintCentered) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^78}", win32_error(ERROR_ABANDON_HIBERFILE));
	});

	EXPECT_EQ(" A valid hibernation file has been invalidated and should be abandoned. (787) ", str);
}


// Common error handling

TEST_F(format_Test, win32error_Error_LogAndPrintFallback) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_log, Event(evt::FormatMessageId_E.Id, DTGM_ARG3));

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("<Error> (787)", str);
}

TEST_F(format_Test, win32error_ErrorCentered_LogAndPrintFallbackCentered) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_log, Event(evt::FormatMessageId_E.Id, DTGM_ARG3));

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^78}", win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("                                <Error> (787)                                 ", str);
}

TEST_F(format_Test, win32error_DynamicBuffer_Print) {
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0))
	    .WillOnce(t::DoDefault());
	EXPECT_CALL(m_win32, LocalFree(t::NotNull()));

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_DynamicBufferFreeError_LogAndPrint) {
	HLOCAL hBuffer = nullptr;

	t::InSequence s;
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0));
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(t::Invoke([&hBuffer, this](DWORD flags, LPCVOID source, DWORD messageId, DWORD languageId, LPSTR buffer, DWORD size, va_list* args) {
		    const DWORD result = m_win32.DTGM_Real_FormatMessageA(flags, source, messageId, languageId, buffer, size, args);
		    if (hBuffer) {
			    throw std::logic_error("LocalFree");
		    }
		    hBuffer = *reinterpret_cast<LPSTR*>(buffer);
		    return result;
	    }));
	EXPECT_CALL(m_win32, LocalFree(t::ResultOf(
	                         [&hBuffer](const HLOCAL hMem) noexcept {
		                         return hMem == hBuffer;
	                         },
	                         t::IsTrue())))
	    .WillOnce(t::DoAll(m4t::SetLastError(ERROR_IMPLEMENTATION_LIMIT), t::Assign(&hBuffer, nullptr), t::ReturnArg<0>()));
	EXPECT_CALL(m_log, Event(evt::MemoryLeak_E.Id, DTGM_ARG3));

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{}", win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("A valid hibernation file has been invalidated and should be abandoned. (787)", str);
}

TEST_F(format_Test, win32error_DynamicBufferError_LogAndPrintFallback) {
	t::InSequence s;
	EXPECT_CALL(m_win32, FormatMessageA(m4t::BitsSet(FORMAT_MESSAGE_FROM_SYSTEM), t::_, ERROR_ABANDON_HIBERFILE, DTGM_ARG4))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INSUFFICIENT_BUFFER, 0))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_BADKEY, 0));
	EXPECT_CALL(m_win32, LocalFree(t::IsNull()));
	EXPECT_CALL(m_log, Event(evt::FormatMessageId_E.Id, DTGM_ARG3));

	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(win32_error(ERROR_ABANDON_HIBERFILE));
	});
	EXPECT_EQ("<Error> (787)", str);
}


// RPC_STATUS

TEST_F(format_Test, rpcstatus_Default_Print) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(rpc_status(RPC_S_OUT_OF_MEMORY));
	});

	EXPECT_EQ("Not enough memory resources are available to complete this operation. (14)", str);
}

TEST_F(format_Test, rpcstatus_DefaultW_Print) {
	const std::wstring str = m4t::WithLocale("en-US", [] {
		return fmt::to_wstring(rpc_status(RPC_S_OUT_OF_MEMORY));
	});

	EXPECT_EQ(L"Not enough memory resources are available to complete this operation. (14)", str);
}

TEST_F(format_Test, rpcstatus_Centered_PrintCentered) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^76}", rpc_status(RPC_S_OUT_OF_MEMORY));
	});

	EXPECT_EQ(" Not enough memory resources are available to complete this operation. (14) ", str);
}


// HRESULT

TEST_F(format_Test, hresult_Default_Print) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::to_string(hresult(E_ABORT));
	});

	EXPECT_EQ("Operation aborted (0x80004004)", str);
}

TEST_F(format_Test, hresult_DefaultW_Print) {
	const std::wstring str = m4t::WithLocale("en-US", [] {
		return fmt::to_wstring(hresult(E_ABORT));
	});

	EXPECT_EQ(L"Operation aborted (0x80004004)", str);
}
TEST_F(format_Test, hresult_Centered_PrintCentered) {
	const std::string str = m4t::WithLocale("en-US", [] {
		return fmt::format("{:^32}", hresult(E_ABORT));
	});

	EXPECT_EQ(" Operation aborted (0x80004004) ", str);
}


//
// VARIANT
//

TEST_F(format_Test, VARIANT_IsBooleanTrue_PrintMinus1) {
	VARIANT arg;
	InitVariantFromBoolean(TRUE, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(BOOL: -1)", str);
}

TEST_F(format_Test, VARIANT_IsBooleanTrueW_PrintMinus1) {
	VARIANT arg;
	InitVariantFromBoolean(TRUE, &arg);

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"(BOOL: -1)", str);
}


//
// PROPVARIANT
//

TEST_F(format_Test, PROPVARIANT_IsBooleanTrue_PrintMinus1) {
	PROPVARIANT arg;
	InitPropVariantFromBoolean(TRUE, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(BOOL: -1)", str);
}

TEST_F(format_Test, PROPVARIANT_IsBooleanTrueW_PrintMinus1) {
	PROPVARIANT arg;
	InitPropVariantFromBoolean(TRUE, &arg);

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"(BOOL: -1)", str);
}

TEST_F(format_Test, PROPVARIANT_IsInt16_PrintI2) {
	PROPVARIANT arg;
	InitPropVariantFromInt16(37, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(I2: 37)", str);
}


TEST_F(format_Test, PROPVARIANT_IsStringVector_PrintVector) {
	PROPVARIANT arg;
	InitPropVariantFromStringAsVector(L"red;green;blue", &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(LPWSTR|VECTOR: red; green; blue)", str);
}

TEST_F(format_Test, PROPVARIANT_IsUInt32Centered_PrintUI4Centered) {
	PROPVARIANT arg;
	InitPropVariantFromUInt32(897, &arg);

	const std::string str = fmt::format("{:^12}", arg);

	EXPECT_EQ(" (UI4: 897) ", str);
}

TEST_F(format_Test, PROPVARIANT_IsEmptyAsType_PrintType) {
	PROPVARIANT arg;
	PropVariantInit(&arg);

	const std::string str = fmt::format("{:t}", arg);

	EXPECT_EQ("EMPTY", str);
}

TEST_F(format_Test, PROPVARIANT_IsCurrencyAsType_PrintType) {
	PROPVARIANT arg;
	arg.vt = VT_CY;
	arg.cyVal.Hi = 42;

	const std::string str = fmt::format("{:t}", arg);

	EXPECT_EQ("CY", str);
}

TEST_F(format_Test, PROPVARIANT_IsUInt32AsType_PrintType) {
	PROPVARIANT arg;
	InitPropVariantFromUInt32(897, &arg);

	const std::string str = fmt::format("{:t;}", arg);

	EXPECT_EQ("UI4", str);
}

TEST_F(format_Test, PROPVARIANT_IsUInt32AsTypeCentered_PrintTypeCentered) {
	PROPVARIANT arg;
	InitPropVariantFromUInt32(897, &arg);

	const std::string str = fmt::format("{:t;^5}", arg);

	EXPECT_EQ(" UI4 ", str);
}

TEST_F(format_Test, PROPVARIANT_IsEmptyAsValue_PrintValue) {
	PROPVARIANT arg;
	PropVariantInit(&arg);

	const std::string str = fmt::format("{:v}", arg);

	EXPECT_EQ("", str);
}

TEST_F(format_Test, PROPVARIANT_IsCurrencyAsValue_PrintValue) {
	PROPVARIANT arg;
	arg.vt = VT_CY;
	arg.cyVal.Hi = 42;

	const std::string str = fmt::format("{:v}", arg);

	EXPECT_EQ("<?>", str);
}

TEST_F(format_Test, PROPVARIANT_IsUInt32AsValue_PrintValue) {
	PROPVARIANT arg;
	InitPropVariantFromUInt32(897, &arg);

	const std::string str = fmt::format("{:v;}", arg);

	EXPECT_EQ("897", str);
}

TEST_F(format_Test, PROPVARIANT_IsUInt32AsValueCentered_PrintValueCentered) {
	PROPVARIANT arg;
	InitPropVariantFromUInt32(897, &arg);

	const std::string str = fmt::format("{:v;^5}", arg);

	EXPECT_EQ(" 897 ", str);
}

TEST_F(format_Test, PROPVARIANT_IsI2AndError_LogAndPrintFallback) {
	PROPVARIANT arg;
	InitPropVariantFromInt16(37, &arg);

	t::InSequence s;
	EXPECT_CALL(m_win32, PropVariantToStringAlloc)
	    .WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_log, Event(evt::FormatVariant_H.Id, DTGM_ARG3));

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(I2)", str);
}


//
// IUnknown
//

TEST_F(format_Test, IUnknown_Empty_PrintNull) {
	com_ptr<IUnknown> ptr;

	const std::string str = fmt::to_string(fmt_ptr(ptr.get()));

	EXPECT_EQ("(ptr=0x0, ref=0)", str);
}

TEST_F(format_Test, IUnknown_EmptyCentered_PrintNullCentered) {
	com_ptr<IUnknown> ptr;

	const std::string str = fmt::format("{:^40}", fmt_ptr(ptr.get()));

	EXPECT_EQ(40, str.size());
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(ptr=0x0, ref=0\\)\\s+"));
}

TEST_F(format_Test, IUnknown_EmptyAsPointer_PrintPointer) {
	com_ptr<IUnknown> ptr;

	const std::string str = fmt::format("{:p;}", fmt_ptr(ptr.get()));

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, IUnknown_EmptyAsRef_PrintRef) {
	com_ptr<IUnknown> ptr;

	const std::string str = fmt::format("{:r;}", fmt_ptr(ptr.get()));

	EXPECT_EQ("0", str);
}


TEST_F(format_Test, IUnknown_Value_Print) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::to_string(fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(fmt::format("(ptr={}, ref=2)", fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\(ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueW_Print) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::to_wstring(fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(fmt::format(L"(ptr={}, ref=2)", fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"\\(ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueCentered_PrintCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format("{:^40}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(40, str.size());
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(ptr=0x[0-9a-f]+, ref=2\\)\\s+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueAsPointer_PrintPointer) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format("{:p;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(fmt::to_string(fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueAsPointerW_PrintPointer) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format(L"{:p;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(fmt::to_wstring(fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueAsRef_PrintRef) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format("{:r;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ("2", str);
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueAsRefW_PrintRef) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format(L"{:r;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ(L"2", str);
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IUnknown_ValueAsRefAsHex_PrintRefHex) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		m_check.Call();
		str = fmt::format("{:r;#04x}", fmt_ptr(ptr.get()));
		m_check.Call();
	}
	EXPECT_EQ("0x02", str);
	COM_MOCK_VERIFY(object);
}


//
// IStream
//

TEST_F(format_Test, IStream_Empty_PrintNull) {
	com_ptr<IStream> ptr;

	const std::string str = fmt::to_string(fmt_ptr(ptr.get()));

	EXPECT_EQ("(<Empty>, ptr=0x0, ref=0)", str);
}

TEST_F(format_Test, IStream_EmptyCentered_PrintNullCentered) {
	com_ptr<IStream> ptr;

	const std::string str = fmt::format("{:^40}", fmt_ptr(ptr.get()));

	EXPECT_EQ(40, str.size());
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(<Empty>, ptr=0x0, ref=0\\)\\s+"));
}

TEST_F(format_Test, IStream_EmptyAsName_PrintName) {
	com_ptr<IStream> ptr;

	const std::string str = fmt::format("{:n;}", fmt_ptr(ptr.get()));

	EXPECT_EQ("<Empty>", str);
}

TEST_F(format_Test, IStream_EmptyAsPointer_PrintPointer) {
	com_ptr<IStream> ptr;

	const std::string str = fmt::format("{:p;}", fmt_ptr(ptr.get()));

	EXPECT_EQ("0x0", str);
}

TEST_F(format_Test, IStream_EmptyAsRef_PrintRef) {
	com_ptr<IStream> ptr;

	const std::string str = fmt::format("{:r;}", fmt_ptr(ptr.get()));

	EXPECT_EQ("0", str);
}

TEST_F(format_Test, IStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::to_string(fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(fmt::format("(test.dat, ptr={}, ref=2)", fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueW_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::to_wstring(fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(fmt::format(L"(test.dat, ptr={}, ref=2)", fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"\\(test.dat, ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueWithEmptyName_PrintDefaultName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(nullptr));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::to_string(fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(<IStream>, ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueCentered_PrintNameCentered) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format("{:^60}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(60, str.size());
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+\\(test.dat, ptr=0x[0-9a-f]+, ref=2\\)\\s+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_Error_LogAndPrintFallback) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT))
	    .WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_log, Event(evt::FormatIStream_H.Id, DTGM_ARG3));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::to_string(fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(<Error>, ptr=0x[0-9a-f]+, ref=2\\)"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsName_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format("{:n;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ("test.dat", str);
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsNameW_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format(L"{:n;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(L"test.dat", str);
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsPointer_PrintPointer) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format("{:p;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(fmt::to_string(fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsPointerW_PrintPointer) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format(L"{:p;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(fmt::to_wstring(fmt::ptr(&object)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsRef_PrintRef) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format("{:r;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ("2", str);
	COM_MOCK_VERIFY(object);
}

TEST_F(format_Test, IStream_ValueAsRefW_PrintRef) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	t::InSequence s;
	EXPECT_CALL(object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, AddRef).Times(t::AtMost(1));
	EXPECT_CALL(object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(object, Release);

	std::wstring str;
	{
		com_ptr<IStream> ptr(&object);
		m_check.Call();
		str = fmt::format(L"{:r;}", fmt_ptr(ptr.get()));
		m_check.Call();
	}

	EXPECT_EQ(L"2", str);
	COM_MOCK_VERIFY(object);
}


//
// PROPERTYKEY
//

TEST_F(format_Test, PROPERTYKEY_IsValue_PrintName) {
	REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("System.Contact.BusinessAddress", str);
}

TEST_F(format_Test, PROPERTYKEY_IsValueW_PrintName) {
	REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"System.Contact.BusinessAddress", str);
}

TEST_F(format_Test, PROPERTYKEY_Error_PrintFallback) {
	REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;

	t::InSequence s;
	EXPECT_CALL(m_win32, PSGetNameFromPropertyKey(PKEY_Contact_BusinessAddress, t::_))
	    .WillOnce(t::Return(E_ACCESSDENIED));
	EXPECT_CALL(m_log, Event(evt::FormatPropertyKey_H.Id, DTGM_ARG3));

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("730fb6dd-cf7c-426b-a03f-bd166cc9ee24", str);
}

TEST_F(format_Test, PROPERTYKEY_IsValueCentered_PrintNameCentered) {
	REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;

	const std::string str = fmt::format("{:^32}", arg);

	EXPECT_EQ(" System.Contact.BusinessAddress ", str);
}

TEST_F(format_Test, PROPERTYKEY_ErrorCentered_PrintFallbackCentered) {
	REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;

	t::InSequence s;
	EXPECT_CALL(m_win32, PSGetNameFromPropertyKey(t::Ref(PKEY_Contact_BusinessAddress), t::_))
	    .WillOnce(t::Return(E_ACCESSDENIED));
	EXPECT_CALL(m_log, Event(evt::FormatPropertyKey_H.Id, DTGM_ARG3));

	const std::string str = fmt::format("{:^38}", arg);

	EXPECT_EQ(" 730fb6dd-cf7c-426b-a03f-bd166cc9ee24 ", str);
}


//
// WICRect
//

TEST_F(format_Test, WICRect_Default_PrintDimensions) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(@(10, 20) / 320 x 160)", str);
}

TEST_F(format_Test, WICRect_DefaultW_PrintDimensions) {
	const WICRect arg = {10, 20, 320, 160};

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"(@(10, 20) / 320 x 160)", str);
}

TEST_F(format_Test, WICRect_AsHex_PrintDimensions) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::format("{:03x}", arg);

	EXPECT_EQ("(@(00a, 014) / 140 x 0a0)", str);
}

TEST_F(format_Test, WICRect_X_PrintX) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::format("{:x}", arg);

	EXPECT_EQ("10", str);
}

TEST_F(format_Test, WICRect_Y_PrintY) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::format("{:y}", arg);

	EXPECT_EQ("20", str);
}

TEST_F(format_Test, WICRect_Width_PrintWidth) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::format("{:w}", arg);

	EXPECT_EQ("320", str);
}

TEST_F(format_Test, WICRect_Height_PrintHeight) {
	const WICRect arg = {10, 20, 320, 160};

	const std::string str = fmt::format("{:h}", arg);

	EXPECT_EQ("160", str);
}

}  // namespace
}  // namespace m3c::test
