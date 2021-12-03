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

#include "m3c/LogData.h"
#include <m3c/source_location.h>

#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <evntprov.h>

#include <cstdint>
#include <exception>
#include <string>
#include <system_error>

namespace m3c::test {
namespace {

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


//
// throw
//
TEST(throw_Test, Default) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + evt::Default;
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const char*>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, nullptr),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, Event_ArgCountIs0) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + evt::Test_Event;
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event.Id)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, Event_ArgCountIs1) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + evt::Test_Event_String << "mymessage";
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String.Id)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, Event_ArgCountIs2) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + evt::Test_Event_String_H << "mymessage" << E_NOTIMPL;
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String_H.Id)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, String_ArgCountIs0) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + "TestEvent";
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const char*>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, t::StrEq("TestEvent")),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, String_ArgCountIs1) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + "TestEvent {}"
		                << "mymessage";
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const char*>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, t::StrEq("TestEvent {}")),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, String_ArgCountIs2) {
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + "TestEvent {} 0x{:X}"
		                << "mymessage" << E_NOTIMPL;
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const char*>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, t::StrEq("TestEvent {} 0x{:X}")),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(throw_Test, Event_ArgCountIs2WithCustom) {
	class LogArg {
	public:
		void operator>>(_Inout_ LogData& logData) const {
			logData << E_NOTIMPL;
		}
	};
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([]() {
		            throw std::exception("myexception") + "TestEvent {} 0x{:X}"
		                << "mymessage" << LogArg();
	            }),
	            (t::Throws<internal::ExceptionDetail<std::exception, const char*>>(
	                t::AllOf(
	                    t::Property(&std::exception::what, t::StrEq("myexception")),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, t::StrEq("TestEvent {} 0x{:X}")),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}


//
// M3C_COM_HR
//

TEST(M3C_COM_HR_Test, Event_Ok_NoThrow) {
	HRESULT hr = S_OK;
	EXPECT_NO_THROW(M3C_COM_HR(hr, evt::Test_Event_String_H, "mymessage"));  // NOLINT(cppcoreguidelines-avoid-goto): Used internally by EXPECT_NO_THROW.
}

TEST(M3C_COM_HR_Test, Event_Error_ThrowWithCode) {
	HRESULT hr = E_ABORT;
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([hr]() {
		            M3C_COM_HR(hr, evt::Test_Event_String_H, "mymessage");
	            }),
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, E_ABORT)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String_H.Id)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

TEST(M3C_COM_HR_Test, String_Ok_NoThrow) {
	HRESULT hr = S_OK;
	EXPECT_NO_THROW(M3C_COM_HR(hr, "TestEvent {}", "mymessage"));  // NOLINT(cppcoreguidelines-avoid-goto): Used internally by EXPECT_NO_THROW.
}

TEST(M3C_COM_HR_Test, String_Error_ThrowWithCode) {
	HRESULT hr = E_ABORT;
	constexpr std::uint_least32_t kLine = std::source_location::current().line() + 1;  // macro in next line
	EXPECT_THAT(([hr]() {
		            M3C_COM_HR(hr, "TestEvent {}", "mymessage");
	            }),
	            (t::Throws<internal::ExceptionDetail<com_error, const char*>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, E_ABORT)),
	                    t::Property(&internal::BaseException<const char*>::GetLogMessage, t::StrEq("TestEvent {}")),
	                    t::Property(&internal::BaseException<const char*>::GetSourceLocation, t::Property(&std::source_location::line, kLine))))));
}

}  // namespace
}  // namespace m3c::test
