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

/// @file

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <propidl.h>
#include <unknwn.h>
#include <windows.h>

#include <regex>
#include <type_traits>


#ifdef M4T_GOOGLETEST_SHORT_NAMESPACE
namespace M4T_GOOGLETEST_SHORT_NAMESPACE = testing;
#else
namespace t = testing;
#endif

//
// Additional checks
//

/// @brief Generates a failure if @p arg_ is not `nullptr`.
/// @param arg_ The argument to check.
#define ASSERT_NULL(arg_) ASSERT_EQ(nullptr, arg_)

/// @brief Generates a failure if @p arg_ is `nullptr`.
/// @param arg_ The argument to check.
#define ASSERT_NOT_NULL(arg_) ASSERT_NE(nullptr, arg_)

/// @brief Generates a failure if @p arg_ is not `nullptr`.
/// @param arg_ The argument to check.
#define EXPECT_NULL(arg_) EXPECT_EQ(nullptr, arg_)

/// @brief Generates a failure if @p arg_ is `nullptr`.
/// @param arg_ The argument to check.
#define EXPECT_NOT_NULL(arg_) EXPECT_NE(nullptr, arg_)

// https://msdn.microsoft.com/en-us/library/aa270812(v=vs.60).aspx
// https://msdn.microsoft.com/de-de/library/974tc9t1.aspx
// https://en.wikipedia.org/wiki/Magic_number_(programming)
#ifdef _DEBUG
/// @brief Check if a memory address contains uninitialized data.
/// @note Available in debug builds only.
/// @param p_ The address to check.
#define EXPECT_UNINITIALIZED(p_) __pragma(warning(suppress : 6001)) EXPECT_EQ(0xCDCDCDCD, *(unsigned int*) p_)

/// @brief Check if a memory address contains deleted data.
/// @note Available in debug builds only.
/// @param p_ The address to check.
#define EXPECT_DELETED(p_) EXPECT_EQ(0xDDDDDDDD, *(unsigned int*) p_)
#else
#define EXPECT_UNINITIALIZED(p_)
#define EXPECT_DELETED(p_)
#endif


//
// Mock helpers
//

/// @brief Declare a COM mock object.
/// @param name_ The name of the mock object.
/// @param type_ The type of the mock object.
#define COM_MOCK_DECLARE(name_, type_) \
	type_ name_;                       \
	ULONG name_##RefCount_ = 1

/// @brief Prepare a COM mock object for use.
/// @details Sets up basic calls for `AddRef`, `Release` and `QueryInterface`.
/// Provide all COM interfaces as additional argments.
/// @brief @param name_ The name of the mock object.
#define COM_MOCK_SETUP(name_, ...)                                            \
	m4t::SetupComMock<decltype(name_), __VA_ARGS__>(name_, name_##RefCount_); \
	do {                                                                      \
	} while (false)

/// @brief Verify that the reference count of a COM mock is 1.
/// @param name_ The name of the mock object.
#define COM_MOCK_VERIFY(name_)                                        \
	EXPECT_EQ(1ul, name_##RefCount_) << "Reference count of " #name_; \
	do {                                                              \
	} while (false)

/// @brief Verify that the reference count of a COM mock has a particular value.
/// @param count_ The expected reference count.
/// @param name_ The name of the mock object.
#define COM_MOCK_EXPECT_REFCOUNT(count_, name_) \
	EXPECT_EQ(count_, name_##RefCount_)


namespace m4t {

namespace t = testing;

namespace internal {
constexpr int kInvalid = 0;  ///< @brief A variable whose address serves as a marker for invalid pointer values.
}  // namespace internal

/// @brief A value to be used for marking pointer values as invalid.
constexpr const void* kInvalidPtr = &internal::kInvalid;

/// @brief Helper function to setup a mock object.
/// @tparam M The class of the mock object.
/// @tparam T The COM interfaces implmented by the mock object.
/// @param mock The mock object.
/// @param refCount The variable holding the mock reference count.
template <class M, class... T>
void SetupComMock(M& mock, ULONG& refCount) {
	// allow removing expectations without removing default behavior
	ON_CALL(mock, AddRef())
		.WillByDefault(m4t::AddRef(&refCount));
	ON_CALL(mock, Release())
		.WillByDefault(m4t::Release(&refCount));
	ON_CALL(mock, QueryInterface(t::_, t::_))
		.WillByDefault(m4t::QueryInterfaceFail());
	ON_CALL(mock, QueryInterface(t::AnyOf(__uuidof(IUnknown), __uuidof(T)...), t::_))
		.WillByDefault(m4t::QueryInterface(&mock));

	EXPECT_CALL(mock, AddRef()).Times(t::AtLeast(0));
	EXPECT_CALL(mock, Release()).Times(t::AtLeast(0));
	EXPECT_CALL(mock, QueryInterface(t::_, t::_)).Times(t::AtLeast(0));
	EXPECT_CALL(mock, QueryInterface(t::AnyOf(__uuidof(IUnknown), __uuidof(T)...), t::_)).Times(t::AtLeast(0));
}

/// @brief Create a matcher using `std::regex` instead of the regex library of google test.
/// @param pattern The regex pattern to use.
#pragma warning(suppress : 4100)
MATCHER_P(MatchesRegex, pattern, "") {
	return std::regex_match(arg, std::regex(pattern));
}

/// @brief Action for mocking `IUnknown::AddRef`.
/// @param pRefCount A pointer to the variable holding the COM reference count.
#pragma warning(suppress : 4100)
ACTION_P(AddRef, pRefCount) {
	static_assert(std::is_same_v<ULONG*, pRefCount_type>);
	return ++*pRefCount;
}

/// @brief Action for mocking `IUnknown::Release`.
/// @param pRefCount A pointer to the variable holding the COM reference count.
#pragma warning(suppress : 4100)
ACTION_P(Release, pRefCount) {
	static_assert(std::is_same_v<ULONG*, pRefCount_type>);
	return --*pRefCount;
}

/// @brief Action for mocking `IUnknown::QueryInterface`.
/// @param pObject The object to return as a result.
#pragma warning(suppress : 4100)
ACTION_P(QueryInterface, pObject) {
	static_assert(std::is_same_v<REFIID, arg0_type>);
	static_assert(std::is_same_v<void**, arg1_type>);
	static_assert(std::is_base_of_v<IUnknown, std::remove_pointer_t<pObject_type>>);

	*arg1 = pObject;
	static_cast<IUnknown*>(pObject)->AddRef();
	return S_OK;
}

/// @brief Action for mocking `IUnknown::QueryInterface` returning a failure.
#pragma warning(suppress : 4100)
ACTION(QueryInterfaceFail) {
	static_assert(std::is_same_v<REFIID, arg0_type>);
	static_assert(std::is_same_v<void**, arg1_type>);

	*arg1 = nullptr;
	return E_NOINTERFACE;
}

/// @brief Action for returning a COM object as an output argument.
/// @details Shortcut for `DoAll(SetArgPointee<idx>(&m_object), IgnoreResult(AddRef(&m_objectRefCount)))`.
/// Usage: `SetComObjectAndReturnOk<1>(&m_object)`. The output argument MUST NOT be null.
/// @tparam idx 0-based index of the argment.
/// @param pObject A pointer to a COM object.
ACTION_TEMPLATE(SetComObject, HAS_1_TEMPLATE_PARAMS(int, idx), AND_1_VALUE_PARAMS(pObject)) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<IUnknown, std::remove_pointer_t<std::remove_pointer_t<idx_type>>>);
	static_assert(std::is_base_of_v<IUnknown, std::remove_pointer_t<pObject_type>>);

	*(t::get<idx>(args)) = pObject;
	pObject->AddRef();
}

/// @brief Action for setting a pointer to a `PROPVARIANT` to a `VARIANT_BOOL` value.
/// @details Usage: `SetPropVariantToBool<1>(VARIANT_TRUE)`. The `PROPVARIANT` MUST NOT be null.
/// @tparam idx 0-based index of the argment.
/// @param variant A `VARIANT_BOOL`.
ACTION_TEMPLATE(SetPropVariantToBool, HAS_1_TEMPLATE_PARAMS(int, idx), AND_1_VALUE_PARAMS(variantBool)) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<PROPVARIANT, std::remove_pointer_t<std::remove_pointer_t<idx_type>>>);
	static_assert(std::is_same_v<VARIANT_BOOL, variantBool_type>);

	PROPVARIANT* ppv = t::get<idx>(args);
	ppv->boolVal = variantBool;
	ppv->vt = VT_BOOL;
}

/// @brief Action for setting a pointer to a `PROPVARIANT` to a `BSTR` value.
/// @details Usage: `SetPropVariantToBool<1>("value")`. The `PROPVARIANT` MUST NOT be null.
/// @tparam idx 0-based index of the argment.
/// @param wsz A wide character string.
ACTION_TEMPLATE(SetPropVariantToBSTR, HAS_1_TEMPLATE_PARAMS(int, idx), AND_1_VALUE_PARAMS(wsz)) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<PROPVARIANT, std::remove_pointer_t<std::remove_pointer_t<idx_type>>>);

	PROPVARIANT* ppv = t::get<idx>(args);
	PROPVARIANT pv;
	M3C_HRESULT_TO_EXCEPTION(InitPropVariantFromString(wsz, &pv), "InitPropVariantFromString");
	M3C_HRESULT_TO_EXCEPTION(PropVariantChangeType(ppv, pv, 0, VARENUM::VT_BSTR), "PropVariantChangeType");
}

/// @brief Action for setting a pointer to a `PROPVARIANT` to `VT_EMPTY`.
/// @details Usage: `SetPropVariantToEmpty<1>()`. The `PROPVARIANT` MUST NOT be null.
/// @tparam idx 0-based index of the argment.
ACTION_TEMPLATE(SetPropVariantToEmpty, HAS_1_TEMPLATE_PARAMS(int, idx), AND_0_VALUE_PARAMS()) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<PROPVARIANT, std::remove_pointer_t<idx_type>>);

	PROPVARIANT* ppv = t::get<idx>(args);
	M3C_HRESULT_TO_EXCEPTION(PropVariantClear(ppv), "PropVariantClear");
}

/// @brief Action for setting pointer to a `PROPVARIANT` to an `IStream` value.
/// @details Usage: `SetPropVariantToBool<1>(pStream)`.  The `PROPVARIANT` MUST NOT be null.
/// @tparam idx 0-based index of the argment.
/// @param variant An object of type `IStream`.
ACTION_TEMPLATE(SetPropVariantToStream, HAS_1_TEMPLATE_PARAMS(int, idx), AND_1_VALUE_PARAMS(pStream)) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<PROPVARIANT, std::remove_pointer_t<idx_type>>);

	PROPVARIANT* ppv = t::get<idx>(args);
	pStream->AddRef();
	ppv->pStream = pStream;
	ppv->vt = VT_STREAM;
}

/// @brief Action for setting pointer to a `PROPVARIANT` to an `IStream` value.
/// @details Usage: `SetPropVariantToBool<1>(pStream)`.  The `PROPVARIANT` MUST NOT be null.
/// @tparam idx 0-based index of the argment.
/// @param variant An object of type `IStream`.
ACTION_TEMPLATE(SetPropVariantToUInt32, HAS_1_TEMPLATE_PARAMS(int, idx), AND_1_VALUE_PARAMS(value)) {
	typedef typename t::tuple_element<idx, args_type>::type idx_type;
	static_assert(std::is_base_of_v<PROPVARIANT, std::remove_pointer_t<idx_type>>);

	PROPVARIANT* ppv = t::get<idx>(args);
	ppv->ulVal = value;
	ppv->vt = VARENUM::VT_UI4;
}

}  // namespace m4t
