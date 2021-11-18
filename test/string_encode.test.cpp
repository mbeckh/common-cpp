/*
Copyright 2021 Michael Beckh

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

#include "m3c/string_encode.h"

#include "m3c/exception.h"

#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <detours_gmock.h>
#include <evntprov.h>

#include <string>
#include <system_error>

namespace m3c::test {
namespace {

namespace t = testing;

#undef WIN32_FUNCTIONS
#define WIN32_FUNCTIONS(fn_)                                                                                                                                         \
	fn_(6, int, WINAPI, MultiByteToWideChar,                                                                                                                         \
	    (UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar),                                                \
	    (CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar),                                                                                \
	    nullptr);                                                                                                                                                    \
	fn_(8, int, WINAPI, WideCharToMultiByte,                                                                                                                         \
	    (UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar), \
	    (CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar),                                              \
	    nullptr)

class string_encode_Test : public t::Test {
protected:
	static constexpr int kFixedBufferSize = 256;
	DTGM_API_MOCK(m_win32, WIN32_FUNCTIONS);
};


//
// UTF-8
//

TEST_F(string_encode_Test, EncodeUtf8_Empty_ReturnEmpty) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, t::_, t::_, t::_, nullptr, nullptr))
	    .Times(0);

	const std::string str = EncodeUtf8(L"");
	EXPECT_EQ("", str);
}

TEST_F(string_encode_Test, EncodeUtf8_EmptyAndLength_ReturnEmpty) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, t::_, t::_, t::_, nullptr, nullptr))
	    .Times(0);

	const std::string str = EncodeUtf8(L"", 0);
	EXPECT_EQ("", str);
}

TEST_F(string_encode_Test, EncodeUtf8_HighChar_ReturnEncoded) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, 5, t::_, kFixedBufferSize, nullptr, nullptr));

	const std::string str = EncodeUtf8(L"Te\u00F6st");
	EXPECT_EQ("Te\xC3\xB6st", str);
}

TEST_F(string_encode_Test, EncodeUtf8_HighCharAndLength_ReturnEncoded) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, 3, t::_, kFixedBufferSize, nullptr, nullptr));

	const std::string str = EncodeUtf8(L"Te\u00F6st", 3);
	EXPECT_EQ("Te\xC3\xB6", str);
}

TEST_F(string_encode_Test, EncodeUtf8_FitsInternalBuffer_ReturnEncoded) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize, nullptr, nullptr));

	const std::string str = EncodeUtf8(L"Test");
	EXPECT_EQ("Test", str);
}

TEST_F(string_encode_Test, EncodeUtf8_FitsInternalBufferAndLength_ReturnEncoded) {
	constexpr int kLength = kFixedBufferSize + 87;

	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize, nullptr, nullptr));

	const std::string str = EncodeUtf8(std::wstring(kLength, L'x').c_str(), 4);
	EXPECT_EQ("xxxx", str);
}

TEST_F(string_encode_Test, EncodeUtf8_RequiresDynamicBuffer_ReturnEncoded) {
	constexpr int kLength = kFixedBufferSize + 87;

	t::Sequence seq;
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kLength, t::_, 0, nullptr, nullptr))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kLength, t::_, kLength, nullptr, nullptr))
	    .InSequence(seq);

	const std::string str = EncodeUtf8(std::wstring(kLength, L'x').c_str());
	EXPECT_EQ(std::string(kLength, 'x'), str);
}

TEST_F(string_encode_Test, EncodeUtf8_RequiresDynamicBufferAfterEncoding_ReturnEncoded) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kFixedBufferSize, t::_, kFixedBufferSize, nullptr, nullptr))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kFixedBufferSize, t::_, 0, nullptr, nullptr))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kFixedBufferSize, t::_, kFixedBufferSize + 1, nullptr, nullptr))
	    .InSequence(seq);

	const std::string str = EncodeUtf8(std::wstring(kFixedBufferSize - 1, L'x').append(1, L'\u00FC').c_str());
	EXPECT_EQ(std::string(kFixedBufferSize - 1, 'x') + "\u00C3\u00BC", str);
}

TEST_F(string_encode_Test, EncodeUtf8_FitsInternalBufferAndError_ThrowException) {
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize, nullptr, nullptr))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf8(L"Test"); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}

TEST_F(string_encode_Test, EncodeUtf8_RequiresDynamicBufferAndErrorGettingSize_ThrowException) {
	constexpr int kLength = kFixedBufferSize + 87;

	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kLength, t::_, 0, nullptr, nullptr))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf8(std::wstring(kLength, L'x').c_str()); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}

TEST_F(string_encode_Test, EncodeUtf8_RequiresDynamicBufferAndErrorConverting_ThrowException) {
	constexpr int kLength = kFixedBufferSize + 87;

	t::Sequence seq;
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kLength, t::_, 0, nullptr, nullptr))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, WideCharToMultiByte(CP_UTF8, t::_, t::_, kLength, t::_, kLength, nullptr, nullptr))
	    .InSequence(seq)
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf8(std::wstring(kLength, L'x').c_str()); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}


//
// UTF-16
//

TEST_F(string_encode_Test, EncodeUtf16_Empty_ReturnEmpty) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, t::_, t::_, t::_))
	    .Times(0);

	const std::wstring str = EncodeUtf16("");
	EXPECT_EQ(L"", str);
}

TEST_F(string_encode_Test, EncodeUtf16_EmptyAndLength_ReturnEmpty) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, t::_, t::_, t::_))
	    .Times(0);

	const std::wstring str = EncodeUtf16("", 0);
	EXPECT_EQ(L"", str);
}

TEST_F(string_encode_Test, EncodeUtf16_HighChar_ReturnEncoded) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, 6, t::_, kFixedBufferSize));

	const std::wstring str = EncodeUtf16("Te\xC3\xB6st");
	EXPECT_EQ(L"Te\u00F6st", str);
}

TEST_F(string_encode_Test, EncodeUtf16_HighCharAndLength_ReturnEncoded) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize));

	const std::wstring str = EncodeUtf16("Te\xC3\xB6st", 4);
	EXPECT_EQ(L"Te\u00F6", str);
}

TEST_F(string_encode_Test, EncodeUtf16_FitsInternalBuffer_ReturnEncoded) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize));

	const std::wstring str = EncodeUtf16("Test");
	EXPECT_EQ(L"Test", str);
}

TEST_F(string_encode_Test, EncodeUtf16_FitsInternalBufferAndLength_ReturnEncoded) {
	constexpr int kLength = kFixedBufferSize + 87;

	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize));

	const std::wstring str = EncodeUtf16(std::string(kLength, 'x').c_str(), 4);
	EXPECT_EQ(L"xxxx", str);
}

TEST_F(string_encode_Test, EncodeUtf16_RequiresDynamicBuffer_ReturnEncoded) {
	constexpr int kLength = kFixedBufferSize + 87;

	t::Sequence seq;
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, kLength, t::_, 0))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, kLength, t::_, kLength))
	    .InSequence(seq);

	const std::wstring str = EncodeUtf16(std::string(kLength, 'x').c_str());
	EXPECT_EQ(std::wstring(kLength, 'x'), str);
}

TEST_F(string_encode_Test, EncodeUtf16_FitsInternalBufferAndError_ThrowException) {
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, 4, t::_, kFixedBufferSize))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf16("Test"); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}

TEST_F(string_encode_Test, EncodeUtf16_RequiresDynamicBufferAndErrorGettingSize_ThrowException) {
	constexpr int kLength = kFixedBufferSize + 87;

	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, kLength, t::_, 0))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf16(std::string(kLength, 'x').c_str()); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}

TEST_F(string_encode_Test, EncodeUtf16_RequiresDynamicBufferAndErrorConverting_ThrowException) {
	constexpr int kLength = kFixedBufferSize + 87;

	t::Sequence seq;
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, kLength, t::_, 0))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, MultiByteToWideChar(CP_UTF8, t::_, t::_, kLength, t::_, kLength))
	    .InSequence(seq)
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, 0));

	EXPECT_THAT([]() { return EncodeUtf16(std::string(kLength, 'x').c_str()); },
	            t::Throws<internal::ExceptionDetail<windows_error>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_NOT_SUPPORTED)),
	                    t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Encode_E.Id)))));
}

}  // namespace
}  // namespace m3c::test
