/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "m3c/types_fmt.h"  // IWYU pragma: keep

#include "m3c/PropVariant.h"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <objbase.h>
#include <propkey.h>
#include <propvarutil.h>
#include <rpc.h>
#include <wincodec.h>
#include <wtypes.h>

#include <string>


namespace m3c::test {

//
// UUID
//

TEST(types_fmtTest, UUID_Default_PrintValue) {
	std::string str;
	{
		const UUID arg = {0xa5063846, 0xd67, 0x4140, {0x85, 0x62, 0xaf, 0x1a, 0xaf, 0x99, 0xa3, 0x41}};
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "a5063846-0d67-4140-8562-af1aaf99a341");
}


//
// PROPVARIANT
//

TEST(types_fmtTest, PROPVARIANT_IsBooleanTrue_PrintMinus1) {
	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromBoolean(true, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(BOOL: -1)");
}

TEST(types_fmtTest, PROPVARIANT_IsInt16_PrintI2) {
	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromInt16(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I2: 37)");
}


TEST(types_fmtTest, PROPVARIANT_IsStringVector_PrintVector) {
	std::string str;
	{
		PROPVARIANT arg;
		InitPropVariantFromStringAsVector(L"red;green;blue", &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(LPWSTR|VECTOR: red; green; blue)");
}


//
// PropVariant
//

TEST(types_fmtTest, PropVariant_IsBooleanTrue_PrintMinus1) {
	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromBoolean(true, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(BOOL: -1)");
}

TEST(types_fmtTest, PropVariant_IsInt16_PrintI2) {
	std::string str;
	{
		PropVariant arg;
		InitPropVariantFromInt16(37, &arg);
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ(str, "(I2: 37)");
}

TEST(types_fmtTest, PropVariant_IsStringVector_PrintVector) {
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

TEST(types_fmtTest, IUnknown_Empty_PrintNull) {
	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ("0x0", str);
}

TEST(types_fmtTest, IUnknown_Value_PrintDefault) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}
	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST(types_fmtTest, comptrIUnknown_Empty_PrintNull) {
	std::string str;
	{
		com_ptr<IUnknown> ptr;
		str = fmt::format("{}", ptr);
	}

	EXPECT_EQ("0x0", str);
}

TEST(types_fmtTest, comptrIUnknown_Value_PrintDefault) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));

	std::string str;
	{
		com_ptr<IUnknown> ptr(&object);
		str = fmt::format("{}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}


//
// IStream
//

TEST(types_fmtTest, IStream_Empty_PrintNull) {
	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_EQ("0x0", str);
}

TEST(types_fmtTest, IStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", fmt_ptr(ptr.get()));
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}

TEST(types_fmtTest, comptrIStream_Empty_PrintNull) {
	std::string str;
	{
		com_ptr<IStream> ptr;
		str = fmt::format("{}", ptr);
	}

	EXPECT_EQ("0x0", str);
}

TEST(types_fmtTest, comptrIStream_Value_PrintName) {
	COM_MOCK_DECLARE(object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_SETUP(object, IStream, ISequentialStream);

	EXPECT_CALL(object, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Release()).Times(t::AtLeast(0));
	EXPECT_CALL(object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));

	std::string str;
	{
		com_ptr<IStream> ptr(&object);
		str = fmt::format("{}", ptr);
	}

	EXPECT_THAT(str, m4t::MatchesRegex("\\(test.dat, ref=1, this=0x[0-9a-f]+\\)"));
	COM_MOCK_VERIFY(object);
}


//
// PROPERTYKEY
//

TEST(types_fmtTest, PROPERTYKEY_IsValue_PrintName) {
	std::string str;
	{
		REFPROPERTYKEY arg = PKEY_Contact_BusinessAddress;
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("System.Contact.BusinessAddress", str);
}


//
// WICRect
//

TEST(types_fmtTest, WICRect_Default_PrintDimensions) {
	std::string str;
	{
		WICRect arg = {10, 20, 320, 160};
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("(@(10, 20) / 320 x 160)", str);
}

TEST(types_fmtTest, WICRect_AsHex_PrintDimensions) {
	std::string str;
	{
		WICRect arg = {10, 20, 320, 160};
		str = fmt::format("{:03x}", arg);
	}

	EXPECT_EQ("(@(00a, 014) / 140 x 0a0)", str);
}

}  // namespace m3c::test
