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

#include "m3c/exception.h"

#include "testevents.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/m4t.h>
//#include <llamalog/llamalog.h>

#include <windows.h>

#include <exception>
#include <string>
#include <system_error>


namespace m3c::test {

namespace t = testing;

//
// windows_error
//

TEST(windows_error_Test, ctor_FromCode_HasValues) {
	const windows_error error(ERROR_INSUFFICIENT_BUFFER);
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("The data area passed to a system call is too small.", str);
	EXPECT_EQ(ERROR_INSUFFICIENT_BUFFER, error.code().value());
}

TEST(windows_error_Test, ctor_FromCodeAndMessage_HasValues) {
	const windows_error error(ERROR_INSUFFICIENT_BUFFER, "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The data area passed to a system call is too small.", str);
	EXPECT_EQ(ERROR_INSUFFICIENT_BUFFER, error.code().value());
}


//
// rpc_error
//

TEST(rpc_error_Test, ctor_FromCode_HasValues) {
	const rpc_error error(RPC_S_STRING_TOO_LONG);
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("The string is too long.", str);
	EXPECT_EQ(RPC_S_STRING_TOO_LONG, error.code().value());
}

TEST(rpc_error_Test, ctor_FromCodeAndMessage_HasValues) {
	const rpc_error error(RPC_S_STRING_TOO_LONG, "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The string is too long.", str);
	EXPECT_EQ(RPC_S_STRING_TOO_LONG, error.code().value());
}


//
// com_exception
//

TEST(com_error_Test, ctor_FromHRESULT_HasValues) {
	const com_error error(E_INVALIDARG);
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("The parameter is incorrect.", str);
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_error_Test, ctor_FromHRESULTAndMessage_HasValues) {
	const com_error error(E_INVALIDARG, "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The parameter is incorrect.", str);
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_error_Test, ctor_FromWIN32AndMessage_HasValues) {
	const com_error error(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The data area passed to a system call is too small.", str);
	EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), error.code().value());
}

TEST(com_error_Test, ctor_FromRPCAndMessage_HasValues) {
	const com_error error(HRESULT_FROM_WIN32(RPC_S_STRING_TOO_LONG), "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The string is too long.", str);
	EXPECT_EQ(HRESULT_FROM_WIN32(RPC_S_STRING_TOO_LONG), error.code().value());
}


//
// com_invalid_argument_exception
//

TEST(com_invalid_argument_error_Test, ctor_Default_IsInvalidArgumentCode) {
	const com_invalid_argument_error error;
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("The parameter is incorrect.", str);
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_invalid_argument_error_Test, ctor_FromMessage_IsInvalidArgumentCode) {
	const com_invalid_argument_error error("ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: The parameter is incorrect.", str);
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_invalid_argument_error_Test, ctor_FromHRESULTAndMessage_HasValues) {
	const com_invalid_argument_error error(E_NOTIMPL, "ExceptionMessage");
	const std::string str = m4t::WithLocale("en-US", [&error] { return error.what(); });

	EXPECT_EQ("ExceptionMessage: Not implemented", str);
	EXPECT_EQ(E_NOTIMPL, error.code().value());
}

TEST(M3C_COM_HR_Test, call_Ok_NoThrow) {
	HRESULT hr = S_OK;
	EXPECT_NO_THROW(M3C_COM_HR(hr, evt::TestEvent2, "mymessage"));
}

TEST(M3C_COM_HR_Test, call_Error_ThrowWithCode) {
	HRESULT hr = E_ABORT;
	constexpr std::uint_least32_t kLine = source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([hr]() {
					M3C_COM_HR(hr, evt::TestEvent2, "mymessage");
				}),
	            t::Throws<internal::ExceptionDetail<com_error>>(
					t::AllOf(
						t::Property(&system_error::code, t::Property(&std::error_code::value, E_ABORT)),
						t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::TestEvent2.Id)),
						t::Property(&internal::BaseException::GetSourceLocation, t::Property(&source_location::line, kLine)))));
}

//
// Tests for ExceptionToHRESULT are part of Log_Test.cpp.
//

}  // namespace m3c::test
