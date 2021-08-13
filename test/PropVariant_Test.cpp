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

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <windows.h>
#include <objidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propvarutil.h>
#include <unknwn.h>
#include <wtypes.h>

#include <utility>


namespace m3c::test {

class VariantTypeToString_Test : public t::TestWithParam<std::tuple<VARTYPE, std::string, std::string>> {
protected:
	static VARTYPE GetVarType() {
		return std::get<0>(GetParam());
	}

	static std::string GetString() {
		return std::get<1>(GetParam());
	}
};

#pragma push_macro("VT_")
#define VT_(vt_) std::make_tuple(VARENUM::VT_##vt_, #vt_, #vt_)

INSTANTIATE_TEST_SUITE_P(VarType, VariantTypeToString_Test,
                         t::Values(
							 VT_(EMPTY),
							 VT_(NULL),
							 VT_(I2),
							 VT_(I4),
							 VT_(R4),
							 VT_(R8),
							 VT_(CY),
							 VT_(DATE),
							 VT_(BSTR),
							 VT_(DISPATCH),
							 VT_(ERROR),
							 VT_(BOOL),
							 VT_(VARIANT),
							 VT_(UNKNOWN),
							 VT_(DECIMAL),
							 VT_(I1),
							 VT_(UI1),
							 VT_(UI2),
							 VT_(UI4),
							 VT_(I8),
							 VT_(UI8),
							 VT_(INT),
							 VT_(UINT),
							 VT_(VOID),
							 VT_(HRESULT),
							 VT_(PTR),
							 VT_(SAFEARRAY),
							 VT_(CARRAY),
							 VT_(USERDEFINED),
							 VT_(LPSTR),
							 VT_(LPWSTR),
							 VT_(RECORD),
							 VT_(INT_PTR),
							 VT_(UINT_PTR),
							 VT_(FILETIME),
							 VT_(BLOB),
							 VT_(STREAM),
							 VT_(STORAGE),
							 VT_(STREAMED_OBJECT),
							 VT_(STORED_OBJECT),
							 VT_(BLOB_OBJECT),
							 VT_(CF),
							 VT_(CLSID),
							 VT_(VERSIONED_STREAM),
							 VT_(BSTR_BLOB),
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
#pragma pop_macro("VT_")

class Variant_Test : public t::Test {
protected:
	void SetUp() override {
		COM_MOCK_SETUP(m_object, IStream);
	}

	void TearDown() override {
		COM_MOCK_VERIFY(m_object);
	}

protected:
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStream_Mock>);
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
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStream_Mock>);
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

	EXPECT_EQ(VT_EMPTY, value.vt);

	EXPECT_EQ(VT_BSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.bstrVal);
}

//
// ~Variant
//

TEST_F(Variant_Test, dtor_Value_Cleanup) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	{
		Variant pv;
		pv.vt = VT_UNKNOWN;
		pv.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();
	}
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
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

	EXPECT_EQ(VT_EMPTY, value.vt);

	EXPECT_EQ(VT_LPWSTR, pv.vt);
	EXPECT_STREQ(L"Test", pv.pwszVal);
}

//
// ~PropVariant
//

TEST_F(PropVariant_Test, dtor_Value_Cleanup) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	{
		PropVariant pv;
		pv.vt = VT_UNKNOWN;
		pv.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();
	}
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
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

	ASSERT_EQ(2u, dst.cai.cElems);
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
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_object, Release());

	{
		PropVariant src;
		src.vt = VT_UNKNOWN;
		src.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();

		PropVariant dst;
		ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

		COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
		m_check.Call();

		EXPECT_EQ(src.punkVal, dst.punkVal);
	}

	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
}

TEST_F(PropVariantCopy_Test, Unknown_PointerToPointer_IsNotRefCounted) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	IUnknown* pUnknown = &m_object;
	pUnknown->AddRef();
	{
		PropVariant src;
		src.vt = VT_UNKNOWN | VT_BYREF;
		src.ppunkVal = &pUnknown;

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();

		PropVariant dst;
		ASSERT_HRESULT_SUCCEEDED(PropVariantCopy(&dst, &src));

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();

		EXPECT_EQ(src.ppunkVal, dst.ppunkVal);
	}

	pUnknown->Release();

	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
}

}  // namespace m3c::test
