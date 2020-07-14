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

#include "m3c/types_log.h"

#include "m3c/PropVariant.h"
#include "m3c/com_ptr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <llamalog/LogLine.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <objbase.h>
#include <objidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <rpc.h>
#include <unknwn.h>
#include <wtypes.h>

#include <string>


namespace m3c::test {

namespace {

lg::LogLine GetLogLine(const char* const pattern = "{}") {
	return lg::LogLine(lg::Priority::kDebug, "file.cpp", 99, "myfunction()", pattern);
}

}  // namespace


//
// UUID
//

TEST(types_log_Test, UUID_Default_PrintMessage) {
	lg::LogLine logLine = GetLogLine();
	{
		const UUID arg = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "a5063846-0d67-4140-8562-af1aaf99a341");
}


//
// PROPVARIANT
//

TEST(types_log_Test, PROPVARIANT_Default_PrintMessage) {
	lg::LogLine logLine = GetLogLine();
	{
		PROPVARIANT arg;
		InitPropVariantFromStringAsVector(L"red;green;blue", &arg);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "(LPWSTR|VECTOR: red; green; blue)");
}


//
// PropVariant
//

TEST(types_log_Test, PropVariant_IntPtr_PrintValue) {
	lg::LogLine logLine = GetLogLine();
	{
		int i = 20;
		PropVariant arg;
		arg.vt = VT_I4 | VT_BYREF;
		arg.pintVal = &i;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(I4|BYREF: 20)", str);
}

TEST(types_log_Test, PropVariant_CLSID_PrintString) {
	lg::LogLine logLine = GetLogLine();
	{
		PropVariant arg;
		InitPropVariantFromCLSID(IID_IClassFactory, &arg);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(CLSID: {00000001-0000-0000-C000-000000000046})", str);
}
TEST(types_log_Test, PropVariant_BSTR_PrintString) {
	lg::LogLine logLine = GetLogLine();
	{
		PropVariant arg;
		arg.vt = VT_BSTR;
		arg.bstrVal = SysAllocString(L"Test");
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(BSTR: Test)", str);
}

TEST(types_log_Test, PropVariant_BSTRPtr_PrintString) {
	lg::LogLine logLine = GetLogLine();
	{
		BSTR bstr = SysAllocString(L"Test");

		PropVariant arg;
		arg.vt = VT_BSTR | VT_BYREF;
		arg.pbstrVal = &bstr;
		logLine << arg;

		SysFreeString(bstr);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(BSTR|BYREF: Test)", str);
}

TEST(types_log_Test, PropVariant_LPWSTR_PrintString) {
	lg::LogLine logLine = GetLogLine();
	{
		PropVariant arg;
		InitPropVariantFromString(L"Test", &arg);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(LPWSTR: Test)", str);
}

TEST(types_log_Test, PropVariant_StringArray_PrintMessage) {
	lg::LogLine logLine = GetLogLine();
	{
		PropVariant arg;
		InitPropVariantFromStringAsVector(L"red;green;blue", &arg);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "(LPWSTR|VECTOR: red; green; blue)");
}


//
// IUnknown
//

TEST(types_log_Test, IUnknown_Empty_PrintNull) {
	lg::LogLine logLine = GetLogLine();
	{
		com_ptr<IUnknown> ptr;
		logLine << ptr.get();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}

TEST(types_log_Test, IUnknown_Value_PrintDefault) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));


	std::string str;
	{
		lg::LogLine logLine = GetLogLine();
		{
			com_ptr<IUnknown> ptr(&object);
			logLine << ptr.get();
		}
		str = logLine.GetLogMessage();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST(types_log_Test, comptrIUnknown_Empty_PrintNull) {
	lg::LogLine logLine = GetLogLine();
	{
		com_ptr<IUnknown> ptr;
		logLine << ptr;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}

TEST(types_log_Test, comptrIUnknown_Value_PrintDefault) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));

	std::string str;
	{
		lg::LogLine logLine = GetLogLine();
		{
			com_ptr<IUnknown> ptr(&object);
			logLine << ptr;
		}
		str = logLine.GetLogMessage();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}


//
// IStream
//

TEST(types_log_Test, IStream_Empty_PrintNull) {
	lg::LogLine logLine = GetLogLine();
	{
		com_ptr<IStream> ptr;
		logLine << ptr.get();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}

TEST(types_log_Test, IStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);
	t::MockFunction<void()> checkFunction;

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));
	t::Expectation check = EXPECT_CALL(checkFunction, Call());
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).After(check).WillOnce(m4t::IStream_Stat(L"test.dat"));
	std::string str;
	{
		lg::LogLine logLine = GetLogLine();
		{
			com_ptr<IStream> ptr(&object);
			logLine << ptr.get();
		}
		checkFunction.Call();
		str = logLine.GetLogMessage();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST(types_log_Test, comptrIStream_Empty_PrintNull) {
	lg::LogLine logLine = GetLogLine();
	{
		com_ptr<IStream> ptr;
		logLine << ptr;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}

TEST(types_log_Test, comptrIStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);
	t::MockFunction<void()> checkFunction;

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));
	t::Expectation check = EXPECT_CALL(checkFunction, Call());
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).After(check).WillOnce(m4t::IStream_Stat(L"test.dat"));

	std::string str;
	{
		lg::LogLine logLine = GetLogLine();
		{
			com_ptr<IStream> ptr(&object);
			logLine << ptr;
		}
		checkFunction.Call();
		str = logLine.GetLogMessage();
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}


//
// PROPERTYKEY
//

TEST(types_log_Test, PROPERTYKEY_IsValue_PrintName) {
	lg::LogLine logLine = GetLogLine();
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("System.Contact.BusinessAddress", str);
}

}  // namespace m3c::test
