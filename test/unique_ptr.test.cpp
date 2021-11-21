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

#include "m3c/unique_ptr.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <new>  // IWYU pragma: keep
#include <string>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

class Foo {
public:
	Foo() noexcept = default;
	explicit Foo(const int value) noexcept
	    : m_value(value) {
		// empty
	}
	virtual ~Foo() {
		Die();
	}

public:
	int& Arg() noexcept {
		return m_arg;
	}
	const int& Arg() const noexcept {
		return m_arg;
	}
	const int& Value() const noexcept {
		return m_value;
	}
	MOCK_METHOD(void, Die, ());

private:
	int m_arg = 5;
	const int m_value = 0;
};

#define MOCK_SETUP(name_)                                 \
	t::StrictMock<Foo>* name_ = new t::StrictMock<Foo>(); \
	bool name_##_deleted = 0;                             \
	EXPECT_CALL(*(name_), Die)                            \
	    .WillOnce(t::Assign(&name_##_deleted, true))

#define MOCK_EXPECT_DELETED(name_) \
	EXPECT_TRUE(name_##_deleted) << #name_ " not deleted"

#define MOCK_EXPECT_NOT_DELETED(name_) \
	EXPECT_FALSE(name_##_deleted) << #name_ " was deleted"

//
// unique_ptr()
//

TEST(unique_ptr_Test, ctor_Default_IsEmpty) {
	unique_ptr<Foo> ptr;

	EXPECT_EQ(nullptr, ptr);
}


//
// unique_ptr(unique_ptr&&)
//

TEST(unique_ptr_Test, ctorMove_WithEmpty_IsEmpty) {
	unique_ptr<Foo> oth;
	unique_ptr<Foo> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, ctorMove_WithValue_ValueIsMoved) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> oth(pFoo);
	unique_ptr<Foo> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(pFoo, ptr);
	EXPECT_NULL(oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}


//
// unique_ptr(nullptr_t)
//

TEST(unique_ptr_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	unique_ptr<Foo> ptr(nullptr);

	EXPECT_NULL(ptr);
}


//
// unique_ptr(T*)
//

TEST(unique_ptr_Test, ctorFromPointer_WithNullptr_IsEmpty) {
	Foo* p = nullptr;
	unique_ptr<Foo> ptr(p);

	EXPECT_NULL(ptr);
}

TEST(unique_ptr_Test, ctorFromPointer_WithValue_HasValue) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	EXPECT_EQ(pFoo, ptr);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}


//
// ~unique_ptr
//

TEST(unique_ptr_Test, dtor_Value_DeleteObject) {
	MOCK_SETUP(pFoo);

	{
		unique_ptr<Foo> ptr(pFoo);
	}
	MOCK_EXPECT_DELETED(pFoo);
}


//
// operator=(unique_ptr&&)
//

TEST(unique_ptr_Test, opMove_EmptyToEmpty_IsEmpty) {
	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth;

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, opMove_ValueToEmpty_ValueIsMoved) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth(pFoo);

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(pFoo, ptr);
	EXPECT_NULL(oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}

TEST(unique_ptr_Test, opMove_ValueToValue_ValueIsMoved) {
	MOCK_SETUP(pFoo);
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth(pOther);

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(pOther, ptr);  // value must have changed
	EXPECT_NULL(oth);
	MOCK_EXPECT_DELETED(pFoo);
	MOCK_EXPECT_NOT_DELETED(pOther);
}


//
// operator=(nullptr_t)
//

TEST(unique_ptr_Test, opAssign_NullptrToEmpty_IsEmpty) {
	unique_ptr<Foo> ptr;
	ptr = nullptr;

	EXPECT_NULL(ptr);
}

TEST(unique_ptr_Test, opAssign_NullptrToValue_ValueIsCleared) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	ptr = nullptr;

	EXPECT_NULL(ptr);
	MOCK_EXPECT_DELETED(pFoo);
}


//
// operator->
//

TEST(unique_ptr_Test, opMemberOf_Empty_ReturnNullptr) {
	unique_ptr<Foo> ptr;
	const Foo* const p = ptr.operator->();

	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, opMemberOf_Value_CallObject) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	ptr->Arg() = 7;

	EXPECT_EQ(7, pFoo->Arg());
}

TEST(unique_ptr_Test, opMemberOf_ValueAndConst_CallObject) {
	MOCK_SETUP(pFoo);

	const unique_ptr<Foo> ptr(pFoo);
	const int i = ptr->Arg();

	EXPECT_EQ(5, i);
}


//
// operator*
//

TEST(unique_ptr_Test, opInstance_Value_CallObject) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	Foo& foo = *ptr;
	foo.Arg() = 7;

	EXPECT_EQ(7, pFoo->Arg());
}

TEST(unique_ptr_Test, opInstance_ValueAndConst_CallObject) {
	MOCK_SETUP(pFoo);

	const unique_ptr<Foo> ptr(pFoo);
	const Foo& foo = *ptr;
	int i = foo.Arg();

	EXPECT_EQ(5, i);
}


//
// operator&
//

TEST(unique_ptr_Test, opAddressOf_Empty_ReturnNullptr) {
	unique_ptr<Foo> ptr;
	const Foo* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST(unique_ptr_Test, opAddressOf_Value_SetEmpty) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const Foo* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	MOCK_EXPECT_DELETED(pFoo);
}

TEST(unique_ptr_Test, opAddressOf_SetValue_HasValue) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr;
	Foo** pp = &ptr;
	ASSERT_NOT_NULL(pp);

	*pp = pFoo;

	EXPECT_EQ(pFoo, ptr);
}


//
// (bool)
//

TEST(unique_ptr_Test, opBool_Empty_ReturnFalse) {
	unique_ptr<Foo> ptr;

	bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST(unique_ptr_Test, opBool_Value_ReturnTrue) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	bool b = (bool) ptr;

	EXPECT_TRUE(b);
}


//
// get
//

TEST(unique_ptr_Test, get_Empty_ReturnNullptr) {
	unique_ptr<Foo> ptr;
	const Foo* const p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, get_EmptyAndConst_ReturnNullptr) {
	const unique_ptr<Foo> ptr;
	const Foo* p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, get_Value_ReturnPointer) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const Foo* const p = ptr.get();

	EXPECT_EQ(pFoo, p);
}

TEST(unique_ptr_Test, get_ValueAndConst_ReturnPointer) {
	MOCK_SETUP(pFoo);

	const unique_ptr<Foo> ptr(pFoo);
	const Foo* p = ptr.get();

	EXPECT_EQ(pFoo, p);
}


//
// reset
//

TEST(unique_ptr_Test, reset_EmptyWithDefault_IsEmpty) {
	unique_ptr<Foo> ptr;

	ptr.reset();

	EXPECT_NULL(ptr);
}

TEST(unique_ptr_Test, reset_EmptyWithNullptr_IsEmpty) {
	unique_ptr<Foo> ptr;

	ptr.reset(nullptr);

	EXPECT_NULL(ptr);
}

TEST(unique_ptr_Test, reset_ValueWithDefault_ReferenceIsReleased) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	ptr.reset();

	EXPECT_NULL(ptr);
	MOCK_EXPECT_DELETED(pFoo);
}

TEST(unique_ptr_Test, reset_ValueWithValue_ReferenceIsAdded) {
	MOCK_SETUP(pFoo);
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr(pFoo);

	ptr.reset(pOther);

	EXPECT_EQ(pOther, ptr);
	MOCK_EXPECT_DELETED(pFoo);
	MOCK_EXPECT_NOT_DELETED(pOther);
}

TEST(unique_ptr_Test, reset_ValueWithSameValue_NoChange) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	ptr.reset(pFoo);

	EXPECT_EQ(pFoo, ptr);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}


//
// release
//

TEST(unique_ptr_Test, release_Empty_ReturnNullptr) {
	unique_ptr<Foo> ptr;
	const Foo* const p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, release_Value_ReturnPointer) {
	MOCK_SETUP(pFoo);

	Foo* p = nullptr;
	{
		unique_ptr<Foo> ptr(pFoo);
		p = ptr.release();

		EXPECT_NULL(ptr);
		EXPECT_EQ(pFoo, p);
	}

	MOCK_EXPECT_NOT_DELETED(pFoo);
	delete p;
	MOCK_EXPECT_DELETED(pFoo);
}


//
// swap
//

TEST(unique_ptr_Test, swap_EmptyWithEmpty_AreEmpty) {
	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, swap_ValueWithEmpty_EmptyAndValue) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(pFoo, oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}

TEST(unique_ptr_Test, swap_EmptyWithValue_ValueAndEmpty) {
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth(pOther);

	ptr.swap(oth);

	EXPECT_EQ(pOther, ptr);
	EXPECT_NULL(oth);
	MOCK_EXPECT_NOT_DELETED(pOther);
}

TEST(unique_ptr_Test, swap_ValueWithValue_ValueAndValue) {
	MOCK_SETUP(pFoo);
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth(pOther);

	ptr.swap(oth);

	EXPECT_EQ(pOther, ptr);
	EXPECT_EQ(pFoo, oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
	MOCK_EXPECT_NOT_DELETED(pOther);
}


//
// hash
//

TEST(unique_ptr_Test, hash_Empty_ReturnHash) {
	unique_ptr<Foo> ptr;
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}

TEST(unique_ptr_Test, hash_Value_ReturnHash) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}


//
// operator==(const com_heap_ptr&, const com_heap_ptr&)
// operator!=(const com_heap_ptr&, const com_heap_ptr&)
//

TEST(unique_ptr_Test, opEquals_EmptyAndEmpty_Equal) {
	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth;

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);
}

TEST(unique_ptr_Test, opEquals_EmptyAndValue_NotEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth(pFoo);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);
}

TEST(unique_ptr_Test, opEquals_ValueAndValue_NotEqual) {
	MOCK_SETUP(pFoo);
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth(pOther);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);
}

TEST(unique_ptr_Test, opEquals_ValueAndSameValue_Equal) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth(ptr.get());

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);

	// MUST release to prevent double free
	EXPECT_EQ(ptr.get(), oth.release());
}


//
// operator==(const unique_ptr<T>&, U* p)
// operator==(U* p, const unique_ptr<T>&)
// operator!=(const unique_ptr<T>&, U* p)
// operator!=(U* p, const unique_ptr<T>&)
//

TEST(unique_ptr_Test, opEqualsPointer_EmptyAndNullptr_Equal) {
	unique_ptr<Foo> ptr;
	const Foo* const p = nullptr;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	unique_ptr<Foo> ptr;
	const Foo other;
	const Foo* const p = &other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const Foo* const p = nullptr;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const Foo other;
	const Foo* const p = &other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_ValueAndSamePointer_Equal) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	EXPECT_TRUE(ptr == pFoo);
	EXPECT_TRUE(pFoo == ptr);

	EXPECT_FALSE(ptr != pFoo);
	EXPECT_FALSE(pFoo != ptr);
}


//
// operator==(const unique_ptr<T>&, nullptr_t)
// operator==(nullptr_t, const unique_ptr<T>&)
// operator!=(const unique_ptr<T>&, nullptr_t)
// operator!=(nullptr_t, const unique_ptr<T>&)
//

TEST(unique_ptr_Test, opEqualsNullptr_Empty_Equal) {
	unique_ptr<Foo> ptr;

	EXPECT_TRUE(ptr == nullptr);
	EXPECT_TRUE(nullptr == ptr);

	EXPECT_FALSE(ptr != nullptr);
	EXPECT_FALSE(nullptr != ptr);
}

TEST(unique_ptr_Test, opEqualsNullptr_Value_NoEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);

	EXPECT_FALSE(ptr == nullptr);
	EXPECT_FALSE(nullptr == ptr);

	EXPECT_TRUE(ptr != nullptr);
	EXPECT_TRUE(nullptr != ptr);
}


//
// make_unique
//

TEST(unique_ptr_Test, makeUnique_Default_ObjectCreated) {
	unique_ptr<Foo> ptr = make_unique<Foo>();

	EXPECT_NOT_NULL(ptr);
}

TEST(unique_ptr_Test, makeUnique_WithArg_ObjectCreated) {
	unique_ptr<Foo> ptr = make_unique<Foo>(1);

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(1, ptr->Value());
}


//
// std::swap
//

TEST(unique_ptr_Test, stdSwap_EmptyAndEmpty_AreEmpty) {
	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth;

	swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, stdSwap_ValueAndEmpty_EmptyAndValue) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth;

	swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(pFoo, oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
}

TEST(unique_ptr_Test, stdSwap_EmptyAndValue_ValueAndEmpty) {
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth(pOther);

	swap(ptr, oth);

	EXPECT_EQ(pOther, ptr);
	EXPECT_NULL(oth);
	MOCK_EXPECT_NOT_DELETED(pOther);
}

TEST(unique_ptr_Test, stdSwap_ValueAndValue_ValueAndValue) {
	MOCK_SETUP(pFoo);
	MOCK_SETUP(pOther);

	unique_ptr<Foo> ptr(pFoo);
	unique_ptr<Foo> oth(pOther);

	swap(ptr, oth);

	EXPECT_EQ(pOther, ptr);
	EXPECT_EQ(pFoo, oth);
	MOCK_EXPECT_NOT_DELETED(pFoo);
	MOCK_EXPECT_NOT_DELETED(pOther);
}


//
// std::hash
//

TEST(unique_ptr_Test, stdHash_Empty_ReturnHash) {
	unique_ptr<Foo> ptr;
	const std::size_t h = std::hash<unique_ptr<Foo>>{}(ptr);

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}

TEST(unique_ptr_Test, stdHash_Value_ReturnHash) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	const std::size_t h = std::hash<unique_ptr<Foo>>{}(ptr);

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}


//
// unique_ptr()
//

TEST(unique_ptrArray_Test, ctor_Default_IsEmpty) {
	unique_ptr<int[]> ptr;

	EXPECT_EQ(nullptr, ptr);
}


//
// unique_ptr(unique_ptr&&)
//

TEST(unique_ptrArray_Test, ctorMove_WithEmpty_IsEmpty) {
	unique_ptr<int[]> oth;
	unique_ptr<int[]> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptrArray_Test, ctorMove_WithValue_ValueIsMoved) {
	int* const arr = new int[3];
	{
		unique_ptr<int[]> oth(arr);
		unique_ptr<int[]> ptr(std::move(oth));

		m4t::EnableMovedFromCheck(oth);
		EXPECT_EQ(arr, ptr);
		EXPECT_NULL(oth);
	}
	EXPECT_DELETED(arr);
}


//
// unique_ptr(nullptr_t)
//

TEST(unique_ptrArray_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	unique_ptr<int[]> ptr(nullptr);

	EXPECT_NULL(ptr);
}


//
// unique_ptr(T*)
//

TEST(unique_ptrArray_Test, ctorFromPointer_WithNullptr_IsEmpty) {
	int* const arr = nullptr;
	unique_ptr<int[]> ptr(arr);

	EXPECT_NULL(ptr);
}

TEST(unique_ptrArray_Test, ctorFromPointer_WithValue_HasValue) {
	int* const arr = new int[3];
	{
		unique_ptr<int[]> ptr(arr);

		EXPECT_EQ(arr, ptr);
	}
	EXPECT_DELETED(arr);
}


//
// ~unique_ptr<T[]>
//

TEST(unique_ptrArray_Test, dtor_Value_DeleteObject) {
	int* const arr = new int[3];
	EXPECT_UNINITIALIZED(arr);

	{
		unique_ptr<int[]> ptr(arr);
	}
	EXPECT_DELETED(arr);
}


//
// operator=(unique_ptr<T[]>&&)
//

TEST(unique_ptrArray_Test, opMove_EmptyToEmpty_IsEmpty) {
	unique_ptr<int[]> ptr;
	unique_ptr<int[]> oth;

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptrArray_Test, opMove_ValueToEmpty_ValueIsMoved) {
	int* const arr = new int[3];

	{
		unique_ptr<int[]> ptr;
		unique_ptr<int[]> oth(arr);

		ptr = std::move(oth);

		m4t::EnableMovedFromCheck(oth);
		EXPECT_EQ(arr, ptr);
		EXPECT_NULL(oth);
	}
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, opMove_ValueToValue_ValueIsMoved) {
	int* const arr = new int[3];
	int* const other = new int[3];

	{
		unique_ptr<int[]> ptr(arr);
		unique_ptr<int[]> oth(other);

		ptr = std::move(oth);

		m4t::EnableMovedFromCheck(oth);
		EXPECT_EQ(other, ptr);  // value must have changed
		EXPECT_NULL(oth);
		EXPECT_DELETED(arr);
	}
	EXPECT_DELETED(other);
}


//
// operator=(nullptr_t)
//

TEST(unique_ptrArray_Test, opAssign_NullptrToEmpty_IsEmpty) {
	unique_ptr<int[]> ptr;
	ptr = nullptr;

	EXPECT_NULL(ptr);
}

TEST(unique_ptrArray_Test, opAssign_NullptrToValue_ValueIsCleared) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	ptr = nullptr;

	EXPECT_NULL(ptr);
	EXPECT_DELETED(arr);
}


//
// operator&
//

TEST(unique_ptrArray_Test, opAddressOf_Empty_ReturnNullptr) {
	unique_ptr<int[]> ptr;
	const int* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST(unique_ptrArray_Test, opAddressOf_Value_SetEmpty) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	const int* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, opAddressOf_SetValue_HasValue) {
	int* arr = new int[3];

	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): arr will be assigned to ptr via pp.
	{
		unique_ptr<int[]> ptr;
		int** pp = &ptr;
		ASSERT_NOT_NULL(pp);

		*pp = arr;

		EXPECT_EQ(arr, ptr);
	}
	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
	EXPECT_DELETED(arr);
}


//
// operator[]
//

TEST(unique_ptrArray_Test, opIndex_ValueAndConst_ReturnValue) {
	int* arr = new int[3]{1, 2, 3};
	const unique_ptr<int[]> ptr(arr);
	const int& v0 = ptr[0];
	const int& v1 = ptr[1];
	const int& v2 = ptr[2];

	EXPECT_EQ(v0, arr[0]);
	EXPECT_EQ(v1, arr[1]);
	EXPECT_EQ(v2, arr[2]);
}

TEST(unique_ptrArray_Test, opIndex_Value_ReturnValue) {
	int* arr = new int[3]{1, 2, 3};
	unique_ptr<int[]> ptr(arr);
	const int& v0 = ptr[0];
	const int& v1 = ptr[1];
	const int& v2 = ptr[2];

	EXPECT_EQ(v0, arr[0]);
	EXPECT_EQ(v1, arr[1]);
	EXPECT_EQ(v2, arr[2]);
}


//
// (bool)
//

TEST(unique_ptrArray_Test, opBool_Empty_ReturnFalse) {
	unique_ptr<int[]> ptr;

	const bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST(unique_ptrArray_Test, opBool_Value_ReturnTrue) {
	unique_ptr<int[]> ptr(new int[3]);

	const bool b = (bool) ptr;

	EXPECT_TRUE(b);
}


//
// get
//

TEST(unique_ptrArray_Test, get_Empty_ReturnNullptr) {
	unique_ptr<int[]> ptr;
	const int* const p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, get_EmptyAndConst_ReturnNullptr) {
	const unique_ptr<int> ptr;
	const int* p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, get_Value_ReturnPointer) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	const int* const p = ptr.get();

	EXPECT_NOT_NULL(p);
}

TEST(unique_ptrArray_Test, get_ValueAndConst_ReturnPointer) {
	int* arr = new int[3];

	const unique_ptr<int[]> ptr(arr);
	const int* p = ptr.get();

	EXPECT_NOT_NULL(p);
}


//
// reset
//

TEST(unique_ptrArray_Test, reset_EmptyWithDefault_IsEmpty) {
	unique_ptr<int[]> ptr;

	ptr.reset();

	EXPECT_NULL(ptr);
}

TEST(unique_ptrArray_Test, reset_EmptyWithNullptr_IsEmpty) {
	unique_ptr<int[]> ptr;

	ptr.reset(nullptr);

	EXPECT_NULL(ptr);
}

TEST(unique_ptrArray_Test, reset_ValueWithDefault_ReferenceIsReleased) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);

	ptr.reset();

	EXPECT_NULL(ptr);
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, reset_ValueWithValue_ReferenceIsAdded) {
	int* arr = new int[3];
	int* other = new int[4];

	unique_ptr<int[]> ptr(arr);

	ptr.reset(other);

	EXPECT_EQ(other, ptr);
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, reset_ValueWithSameValue_NoChange) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);

	ptr.reset(arr);

	EXPECT_EQ(arr, ptr);
}


//
// release
//

TEST(unique_ptrArray_Test, release_Empty_ReturnNullptr) {
	unique_ptr<int[]> ptr;
	const int* const p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, release_Value_ReturnPointer) {
	int* const arr = new int[3]{1, 2, 3};
	int* p = nullptr;
	{
		unique_ptr<int[]> ptr(arr);
		p = ptr.release();

		EXPECT_NULL(ptr);
		EXPECT_EQ(arr, p);
	}
	EXPECT_THAT(1, arr[0]);
	EXPECT_THAT(2, arr[1]);
	EXPECT_THAT(3, arr[2]);

	delete[] p;
}


//
// swap
//

TEST(unique_ptrArray_Test, swap_EmptyWithEmpty_AreEmpty) {
	unique_ptr<int[]> ptr;
	unique_ptr<int[]> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptrArray_Test, swap_ValueWithEmpty_EmptyAndValue) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	unique_ptr<int[]> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(arr, oth);
	EXPECT_UNINITIALIZED(arr);
}

TEST(unique_ptrArray_Test, swap_EmptyWithValue_ValueAndEmpty) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr;
	unique_ptr<int[]> oth(arr);

	ptr.swap(oth);

	EXPECT_EQ(arr, ptr);
	EXPECT_NULL(oth);
	EXPECT_UNINITIALIZED(arr);
}

TEST(unique_ptrArray_Test, swap_ValueWithValue_ValueAndValue) {
	int* const arr = new int[3];
	int* const arrOther = new int[3];

	unique_ptr<int[]> ptr(arr);
	unique_ptr<int[]> oth(arrOther);

	ptr.swap(oth);

	EXPECT_EQ(arrOther, ptr);
	EXPECT_EQ(arr, oth);
	EXPECT_UNINITIALIZED(arr);
	EXPECT_UNINITIALIZED(arrOther);
}


//
// hash
//

TEST(unique_ptrArray_Test, hash_Empty_ReturnHash) {
	unique_ptr<int[]> ptr;
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<void*>{}(ptr.get()), h);
}

TEST(unique_ptrArray_Test, hash_Value_ReturnHash) {
	int* const arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<void*>{}(ptr.get()), h);
}


//
// make_unique_for_overwrite
//

TEST(unique_ptrArray_Test, makeUniqueForOverwrite_WithSize_ArrayCreated) {
	unique_ptr<int[]> ptr = make_unique_for_overwrite<int[]>(3);

	EXPECT_NOT_NULL(ptr);
	EXPECT_UNINITIALIZED(ptr.get());
}


//
// Formatting
//

TEST(unique_ptr_Test, format_Empty_PrintInvalidHandle) {
	unique_ptr<Foo> arg;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("0x0", str);
}

TEST(unique_ptr_Test, format_EmptyCentered_PrintInvalidHandleCentered) {
	unique_ptr<Foo> arg;

	const std::string str = FMT_FORMAT("{:^5}", arg);

	EXPECT_EQ(" 0x0 ", str);
}

TEST(unique_ptr_Test, format_Value_PrintHandle) {
	MOCK_SETUP(pFoo);
	unique_ptr<Foo> arg(pFoo);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ(fmt::to_string(fmt::ptr(pFoo)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
}

TEST(unique_ptr_Test, format_ValueW_PrintHandle) {
	MOCK_SETUP(pFoo);
	unique_ptr<Foo> arg(pFoo);

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(fmt::to_wstring(fmt::ptr(pFoo)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
}

TEST(unique_ptr_Test, format_ValueCentered_PrintHandleCentered) {
	MOCK_SETUP(pFoo);
	unique_ptr<Foo> arg(pFoo);

	const std::string str = FMT_FORMAT("{:^20}", arg);

	EXPECT_EQ(20, str.length());
	EXPECT_EQ(FMT_FORMAT("{:^20}", fmt::ptr(pFoo)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+0x[0-9a-f]+\\s+"));
}

TEST(unique_ptrArray_Test, format_Empty_PrintInvalidHandle) {
	unique_ptr<int[]> arg;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ("0x0", str);
}

TEST(unique_ptrArray_Test, format_EmptyCentered_PrintInvalidHandleCentered) {
	unique_ptr<int[]> arg;

	const std::string str = FMT_FORMAT("{:^5}", arg);

	EXPECT_EQ(" 0x0 ", str);
}

TEST(unique_ptrArray_Test, format_Value_PrintHandle) {
	int* const arr = new int[3];
	unique_ptr<int[]> arg(arr);

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ(fmt::to_string(fmt::ptr(arr)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
}

TEST(unique_ptrArray_Test, format_ValueW_PrintHandle) {
	int* const arr = new int[3];
	unique_ptr<int[]> arg(arr);

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(fmt::to_wstring(fmt::ptr(arr)), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
}

TEST(unique_ptrArray_Test, format_ValueCentered_PrintHandleCentered) {
	int* const arr = new int[3];
	unique_ptr<int[]> arg(arr);

	const std::string str = FMT_FORMAT("{:^20}", arg);

	EXPECT_EQ(20, str.length());
	EXPECT_EQ(FMT_FORMAT("{:^20}", fmt::ptr(arr)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+0x[0-9a-f]+\\s+"));
}


//
// Logging
//

TEST(unique_ptr_Test, LogString_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	unique_ptr<Foo> arg;

	EXPECT_CALL(log, Debug(t::_, "0x0\t"));

	Log::Info("{}{}", arg, '\t');
}

TEST(unique_ptr_Test, LogString_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	MOCK_SETUP(pFoo);
	unique_ptr<Foo> arg(pFoo);

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("{}\t", fmt::ptr(pFoo))));

	Log::Info("{}{}", arg, '\t');
}

TEST(unique_ptr_Test, LogEvent_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	unique_ptr<Foo> arg;

	EXPECT_CALL(log, Debug(t::_, "ptr=0x0\t"));
	EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST(unique_ptr_Test, LogEvent_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	MOCK_SETUP(pFoo);
	unique_ptr<Foo> arg(pFoo);

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(pFoo))));
	EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(pFoo)));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST(unique_ptr_Test, LogException_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	try {
		unique_ptr<Foo> arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "ptr=0x0\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST(unique_ptr_Test, LogException_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	MOCK_SETUP(pFoo);

	try {
		unique_ptr<Foo> arg(pFoo);

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(pFoo)))).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(pFoo))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST(unique_ptrArray_Test, LogString_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	unique_ptr<int[]> arg;

	EXPECT_CALL(log, Debug(t::_, "0x0\t"));

	Log::Info("{}{}", arg, '\t');
}

TEST(unique_ptrArray_Test, LogString_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	int* const arr = new int[3];
	unique_ptr<int[]> arg(arr);

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("{}\t", fmt::ptr(arr))));

	Log::Info("{}{}", arg, '\t');
}

TEST(unique_ptrArray_Test, LogEvent_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	unique_ptr<int[]> arg;

	EXPECT_CALL(log, Debug(t::_, "ptr=0x0\t"));
	EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST(unique_ptrArray_Test, LogEvent_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	int* const arr = new int[3];
	unique_ptr<int[]> arg(arr);

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(arr))));
	EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arr)));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST(unique_ptrArray_Test, LogException_Empty_LogInvalidHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	try {
		unique_ptr<int[]> arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "ptr=0x0\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST(unique_ptrArray_Test, LogException_Value_LogHandle) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	int* const arr = new int[3];

	try {
		unique_ptr<int[]> arg(arr);

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(arr)))).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arr))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
