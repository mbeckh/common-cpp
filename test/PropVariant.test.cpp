/*
Copyright 2019-2020 Michael Beckh

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

#include "m3c/PropVariant.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/IStreamMock.h>
#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <oaidl.h>
#include <objidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propvarutil.h>
#include <unknwn.h>
#include <wtypes.h>

#include <exception>
#include <string>
#include <tuple>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

class VariantTypeToString_Test : public t::TestWithParam<std::tuple<VARTYPE, std::string, std::string>> {
protected:
	static VARTYPE GetVarType() {
		return std::get<0>(GetParam());
	}

	static std::string GetString() {
		return std::get<1>(GetParam());
	}
};

#pragma push_macro("VT")
#define VT(vt_) std::make_tuple(VARENUM::VT_##vt_, #vt_, #vt_)

INSTANTIATE_TEST_SUITE_P(VarType, VariantTypeToString_Test,
                         t::Values(
                             VT(EMPTY),
                             VT(NULL),
                             VT(I2),
                             VT(I4),
                             VT(R4),
                             VT(R8),
                             VT(CY),
                             VT(DATE),
                             VT(BSTR),
                             VT(DISPATCH),
                             VT(ERROR),
                             VT(BOOL),
                             VT(VARIANT),
                             VT(UNKNOWN),
                             VT(DECIMAL),
                             VT(I1),
                             VT(UI1),
                             VT(UI2),
                             VT(UI4),
                             VT(I8),
                             VT(UI8),
                             VT(INT),
                             VT(UINT),
                             VT(VOID),
                             VT(HRESULT),
                             VT(PTR),
                             VT(SAFEARRAY),
                             VT(CARRAY),
                             VT(USERDEFINED),
                             VT(LPSTR),
                             VT(LPWSTR),
                             VT(RECORD),
                             VT(INT_PTR),
                             VT(UINT_PTR),
                             VT(FILETIME),
                             VT(BLOB),
                             VT(STREAM),
                             VT(STORAGE),
                             VT(STREAMED_OBJECT),
                             VT(STORED_OBJECT),
                             VT(BLOB_OBJECT),
                             VT(CF),
                             VT(CLSID),
                             VT(VERSIONED_STREAM),
                             VT(BSTR_BLOB),
                             std::make_tuple(100, "ILLEGAL(0x64)", "Invalid"),
                             std::make_tuple(VARENUM::VT_ILLEGAL, "ILLEGAL(0xffff)", "ILLEGAL"),
                             std::make_tuple(VARENUM::VT_UI4 | VARENUM::VT_VECTOR, "UI4|VECTOR", "UI4_VECTOR"),
                             std::make_tuple(VARENUM::VT_I4 | VARENUM::VT_ARRAY, "I4|ARRAY", "I4_ARRAY"),
                             std::make_tuple(VARENUM::VT_LPSTR | VARENUM::VT_BYREF, "LPSTR|BYREF", "LPSTR_BYREF"),
                             std::make_tuple(VARENUM::VT_I1 | VARENUM::VT_RESERVED, "I1|RESERVED", "I1_RESERVED"),
                             std::make_tuple(VARENUM::VT_UI1 | 0xF000, "ILLEGAL(0xf011)", "UI1_Invalid"),
                             std::make_tuple(100 | VARENUM::VT_VECTOR, "ILLEGAL(0x1064)", "Invalid_VECTOR")),
                         [](const t::TestParamInfo<VariantTypeToString_Test::ParamType>& param) {
	                         // if prefix param.index is missing, source link is not shown in Visual Studio
	                         return fmt::format("{:02}_{}", param.index, std::get<2>(param.param));
                         });
#pragma pop_macro("VT")

class Variant_Test : public t::Test {
protected:
	void SetUp() override {
		COM_MOCK_SETUP(m_object, IStream);
	}

	void TearDown() override {
		COM_MOCK_VERIFY(m_object);
	}

protected:
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStreamMock>);
	t::MockFunction<void()> m_check;
};

class PropVariant_Test : public t::Test {
protected:
	void SetUp() override {
		COM_MOCK_SETUP(m_object, IStream);
	}

	void TearDown() override {
		COM_MOCK_VERIFY(m_object);
	}

protected:
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStreamMock>);
	t::MockFunction<void()> m_check;
};

using PropVariantCopy_Test = PropVariant_Test;


//
// VariantTypeToString
//

TEST_P(VariantTypeToString_Test, call) {
	const std::string str = VariantTypeToString(GetVarType());
	EXPECT_EQ(GetString(), str);
}

//
// Variant
//

TEST_F(Variant_Test, ctor_Ok_IsEmpty) {
	Variant pv;

	EXPECT_EQ(VT_EMPTY, pv.vt);
}

TEST_F(Variant_Test, ctor_Copy_IsCopy) {
	Variant value;
	ASSERT_HRESULT_SUCCEEDED(InitVariantFromString(L"Test", &value));

	Variant pv(value);

	EXPECT_EQ(VT_BSTR, value.vt);
	EXPECT_STREQ(L"Test", value.bstrVal);

	EXPECT_EQ(VT_BSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.bstrVal);
}

TEST_F(Variant_Test, ctor_Move_IsMoved) {
	Variant value;
	ASSERT_HRESULT_SUCCEEDED(InitVariantFromString(L"Test", &value));

	Variant pv(std::move(value));

	m4t::EnableMovedFromCheck(value);
	EXPECT_EQ(VT_EMPTY, value.vt);

	EXPECT_EQ(VT_BSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.bstrVal);
}

//
// ~Variant
//

TEST_F(Variant_Test, dtor_Value_Cleanup) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	{
		Variant pv;
		pv.vt = VT_UNKNOWN;
		pv.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
		m_check.Call();
	}
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
}


//
// PropVariant
//

TEST_F(PropVariant_Test, ctor_Ok_IsEmpty) {
	PropVariant pv;

	EXPECT_EQ(VT_EMPTY, pv.vt);
}

TEST_F(PropVariant_Test, ctor_Copy_IsCopy) {
	PropVariant value;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromString(L"Test", &value));

	PropVariant pv(value);

	EXPECT_EQ(VT_LPWSTR, value.vt);
	EXPECT_STREQ(L"Test", value.pwszVal);

	EXPECT_EQ(VT_LPWSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.pwszVal);
}

TEST_F(PropVariant_Test, ctor_Move_IsMoved) {
	PropVariant value;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromString(L"Test", &value));

	PropVariant pv(std::move(value));

	m4t::EnableMovedFromCheck(value);
	EXPECT_EQ(VT_EMPTY, value.vt);

	EXPECT_EQ(VT_LPWSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.pwszVal);
}

//
// ~PropVariant
//

TEST_F(PropVariant_Test, dtor_Value_Cleanup) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	{
		PropVariant pv;
		pv.vt = VT_UNKNOWN;
		pv.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
		m_check.Call();
	}
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
}


//
// Check some constraints for formatting
//

TEST_F(PropVariantCopy_Test, I2_ByRef_IsSame) {
	SHORT val = 20;

	PropVariant src;
	src.vt = VT_I2 | VT_BYREF;
	src.piVal = &val;

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	EXPECT_EQ(20, *dst.piVal);
	EXPECT_EQ(&val, src.piVal);
	EXPECT_EQ(&val, dst.piVal);
}

TEST_F(PropVariantCopy_Test, I2_VectorOfValue_IsDifferent) {
	SHORT buf[] = {20, 30};
	PropVariant src;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromInt16Vector(buf, sizeof(buf) / sizeof(*buf), &src));

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	ASSERT_EQ(2, dst.cai.cElems);
	EXPECT_EQ(20, dst.cai.pElems[0]);
	EXPECT_EQ(30, dst.cai.pElems[1]);

	EXPECT_NE(src.cai.pElems, dst.cai.pElems);
}

TEST_F(PropVariantCopy_Test, CLSID_Pointer_IsDifferent) {
	PropVariant src;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromCLSID(IID_IClassFactory, &src));
	ASSERT_EQ(VT_CLSID, src.vt);

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	EXPECT_EQ(IID_IClassFactory, *dst.puuid);
	EXPECT_NE(src.puuid, dst.puuid);
}

TEST_F(PropVariantCopy_Test, BSTR_Value_IsDifferent) {
	PropVariant src;
	src.vt = VT_BSTR;
	src.bstrVal = SysAllocString(L"Test");
	ASSERT_NOT_NULL(src.bstrVal);

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	EXPECT_STREQ(L"Test", dst.bstrVal);
	EXPECT_NE(src.bstrVal, dst.bstrVal);
}

TEST_F(PropVariantCopy_Test, BSTR_ByRef_IsSame) {
	BSTR bstr = SysAllocString(L"Test");
	ASSERT_NOT_NULL(bstr);

	PropVariant src;
	src.vt = VT_BSTR | VT_BYREF;
	src.pbstrVal = &bstr;

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	EXPECT_STREQ(L"Test", *dst.pbstrVal);
	EXPECT_EQ(src.pbstrVal, dst.pbstrVal);
}

TEST_F(PropVariantCopy_Test, LPWSTR_Pointer_IsDifferent) {
	PropVariant src;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromString(L"Test", &src));

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	EXPECT_STREQ(L"Test", dst.pwszVal);
	EXPECT_NE(src.pwszVal, dst.pwszVal);
}

TEST_F(PropVariantCopy_Test, LPWSTR_VectorOfPointer_IsDifferent) {
	PropVariant src;
	ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromStringAsVector(L"Test1;Test2", &src));

	PropVariant dst;
	ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

	ASSERT_EQ(2u, dst.calpwstr.cElems);
	EXPECT_STREQ(L"Test1", dst.calpwstr.pElems[0]);
	EXPECT_STREQ(L"Test2", dst.calpwstr.pElems[1]);

	EXPECT_NE(src.calpwstr.pElems, dst.calpwstr.pElems);
	EXPECT_NE(src.calpwstr.pElems[0], dst.calpwstr.pElems[0]);
	EXPECT_NE(src.calpwstr.pElems[1], dst.calpwstr.pElems[1]);
}

TEST_F(PropVariantCopy_Test, Unknown_Pointer_IsRefCounted) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_object, Release);

	{
		PropVariant src;
		src.vt = VT_UNKNOWN;
		src.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
		m_check.Call();

		PropVariant dst;
		ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

		COM_MOCK_EXPECT_REFCOUNT(3, m_object);
		m_check.Call();

		EXPECT_EQ(src.punkVal, dst.punkVal);
	}

	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
}

TEST_F(PropVariantCopy_Test, Unknown_PointerToPointer_IsNotRefCounted) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	IUnknown* pUnknown = &m_object;
	pUnknown->AddRef();
	{
		PropVariant src;
		src.vt = VT_UNKNOWN | VT_BYREF;
		src.ppunkVal = &pUnknown;

		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
		m_check.Call();

		PropVariant dst;
		ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
		m_check.Call();

		EXPECT_EQ(src.ppunkVal, dst.ppunkVal);
	}

	pUnknown->Release();

	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
}


//
// Variant Formatting
//

TEST_F(Variant_Test, format_IsEmpty_PrintEmpty) {
	Variant arg;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(EMPTY)", str);
}

TEST_F(Variant_Test, format_IsNull_PrintNull) {
	Variant arg;
	arg.vt = VT_NULL;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(NULL)", str);
}

TEST_F(Variant_Test, format_IsInt16_PrintI2) {
	Variant arg;
	InitVariantFromInt16(37, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(I2: 37)", str);
}

TEST_F(Variant_Test, format_IsVariantEmpty_PrintVariantEmpty) {
	Variant var;
	Variant arg;
	arg.vt = VT_VARIANT | VT_BYREF;
	arg.pvarVal = &var;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(VARIANT|BYREF->EMPTY)", str);
}

TEST_F(Variant_Test, format_IsVariantEmptyAsValue_PrintVariantEmptyValue) {
	Variant var;
	Variant arg;
	arg.vt = VT_VARIANT | VT_BYREF;
	arg.pvarVal = &var;

	const std::string str = fmt::format("{:v}", arg);

	EXPECT_EQ("", str);
}

TEST_F(Variant_Test, format_IsVariantInt16_PrintVariantI2) {
	Variant var;
	InitVariantFromInt16(37, &var);
	Variant arg;
	arg.vt = VT_VARIANT | VT_BYREF;
	arg.pvarVal = &var;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(VARIANT|BYREF->I2: 37)", str);
}

TEST_F(Variant_Test, format_IsVariantInt16AsValue_PrintVariantI2Value) {
	Variant var;
	InitVariantFromInt16(37, &var);
	Variant arg;
	arg.vt = VT_VARIANT | VT_BYREF;
	arg.pvarVal = &var;

	const std::string str = fmt::format("{:v}", arg);

	EXPECT_EQ("37", str);
}

//
// PropVariant Formatting
//

TEST_F(PropVariant_Test, format_IsEmpty_PrintEmpty) {
	PropVariant arg;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(EMPTY)", str);
}

TEST_F(PropVariant_Test, format_IsNull_PrintNull) {
	PropVariant arg;
	arg.vt = VT_NULL;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(NULL)", str);
}

TEST_F(PropVariant_Test, format_IsInt16_PrintI2) {
	PropVariant arg;
	InitPropVariantFromInt16(37, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(I2: 37)", str);
}

TEST_F(PropVariant_Test, format_IsInt16W_PrintI2) {
	PropVariant arg;
	InitPropVariantFromInt16(37, &arg);

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(L"(I2: 37)", str);
}

TEST_F(PropVariant_Test, format_IsInt32_PrintI4) {
	PropVariant arg;
	InitPropVariantFromInt32(37, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(I4: 37)", str);
}

TEST_F(PropVariant_Test, format_IsFloat_PrintR4) {
	PropVariant arg;
	arg.vt = VT_R4;
	arg.fltVal = 37;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(R4: 37)", str);
}

TEST_F(PropVariant_Test, format_IsDouble_PrintR8) {
	PropVariant arg;
	InitPropVariantFromDouble(37, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(R8: 37)", str);
}

TEST_F(PropVariant_Test, format_IsCurrency_PrintCYWithoutValue) {
	PropVariant arg;
	arg.vt = VT_CY;
	arg.cyVal.int64 = 37;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(CY)", str);
}

TEST_F(PropVariant_Test, format_IsCurrencyCentered_PrintCYCenteredWithoutValue) {
	PropVariant arg;
	arg.vt = VT_CY;
	arg.cyVal.int64 = 37;

	const std::string str = fmt::format("{:^6}", arg);

	EXPECT_EQ(" (CY) ", str);
}

TEST_F(PropVariant_Test, format_IsDate_PrintDate) {
	PropVariant arg;
	arg.vt = VT_DATE;
	arg.date = 3456.78;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(DATE: 1909/06/17:18:43:12.000)", str);
}

TEST_F(PropVariant_Test, format_IsBSTR_PrintBSTR) {
	PropVariant arg;
	arg.vt = VT_BSTR;
	arg.bstrVal = SysAllocString(L"Test");

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(BSTR: Test)", str);
}

TEST_F(PropVariant_Test, format_IsError_PrintError) {
	PropVariant arg;
	arg.vt = VT_ERROR;
	arg.scode = ERROR_ACCOUNT_EXPIRED;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(ERROR)", str);
}

TEST_F(PropVariant_Test, format_IsBooleanTrue_PrintMinus1) {
	PropVariant arg;
	InitPropVariantFromBoolean(TRUE, &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(BOOL: -1)", str);
}

TEST_F(PropVariant_Test, format_IsVariantI4_PrintVariantI4) {
	PropVariant pv;
	InitPropVariantFromInt32(37, &pv);

	PropVariant arg;
	arg.vt = VT_VARIANT | VT_BYREF;
	arg.pvarVal = &pv;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(VARIANT|BYREF->I4: 37)", str);
}

TEST_F(PropVariant_Test, format_IsString_PrintString) {
	// longer than the internal buffer in EncodeUtf8
	PropVariant arg;
	InitPropVariantFromString(std::wstring(512, L'x').c_str(), &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(LPWSTR: " + std::string(512, L'x') + ")", str);
}

TEST_F(PropVariant_Test, format_IsStringVector_PrintVector) {
	PropVariant arg;
	InitPropVariantFromStringAsVector(L"red;green;blue", &arg);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("(LPWSTR|VECTOR: red; green; blue)", str);
}

TEST_F(PropVariant_Test, format_IsStringVectorAsType_PrintType) {
	PropVariant arg;
	InitPropVariantFromStringAsVector(L"red;green;blue", &arg);

	const std::string str = fmt::format("{:t;}", arg);

	EXPECT_EQ("LPWSTR|VECTOR", str);
}

TEST_F(PropVariant_Test, format_IsStringVectorAsValue_PrintValie) {
	PropVariant arg;
	InitPropVariantFromStringAsVector(L"red;green;blue", &arg);

	const std::string str = fmt::format("{:v}", arg);

	EXPECT_EQ("red; green; blue", str);
}


//
// Logging
//

TEST_F(PropVariant_Test, LogString_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	PropVariant arg;

	EXPECT_CALL(log, Debug(t::_, "(EMPTY)\t"));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(PropVariant_Test, LogString_IsInt16_LogI2) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	PropVariant arg;
	InitPropVariantFromInt16(37, &arg);

	EXPECT_CALL(log, Debug(t::_, "(I2: 37)\t"));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(PropVariant_Test, LogEvent_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	PropVariant arg;

	EXPECT_CALL(log, Debug(t::_, "PropVariant=EMPTY, value=\t"));
	EXPECT_CALL(log, Event(evt::Test_LogPropVariant.Id, t::_, t::_, 3));
	EXPECT_CALL(log, EventArg(0, 6, m4t::PointerAs<char>(t::StrEq("EMPTY"))));
	EXPECT_CALL(log, EventArg(1, 2, m4t::PointerAs<wchar_t>(t::StrEq(L""))));
	EXPECT_CALL(log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPropVariant, arg, '\t');
}

TEST_F(PropVariant_Test, LogEvent_IsInt16_LogI2) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	PropVariant arg;
	InitPropVariantFromInt16(37, &arg);

	EXPECT_CALL(log, Debug(t::_, "PropVariant=I2, value=37\t"));
	EXPECT_CALL(log, Event(evt::Test_LogPropVariant.Id, t::_, t::_, 3));
	EXPECT_CALL(log, EventArg(0, 3, m4t::PointerAs<char>(t::StrEq("I2"))));
	EXPECT_CALL(log, EventArg(1, 6, m4t::PointerAs<wchar_t>(t::StrEq(L"37"))));
	EXPECT_CALL(log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPropVariant, arg, '\t');
}

TEST_F(PropVariant_Test, LogException_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	try {
		PropVariant arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "PropVariant=EMPTY, value=\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPropVariant.Id, t::_, t::_, 3)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, 6, m4t::PointerAs<char>(t::StrEq("EMPTY")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, 2, m4t::PointerAs<wchar_t>(t::StrEq(L"")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPropVariant << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST_F(PropVariant_Test, LogException_IsInt16_LogI2) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	try {
		PropVariant arg;
		InitPropVariantFromInt16(37, &arg);

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "PropVariant=I2, value=37\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPropVariant.Id, t::_, t::_, 3)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, 3, m4t::PointerAs<char>(t::StrEq("I2")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, 6, m4t::PointerAs<wchar_t>(t::StrEq(L"37")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPropVariant << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
