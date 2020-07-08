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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/m4t.h>

#include <cstddef>
#include <functional>
#include <new>
#include <utility>

namespace m3c::test {

// prevent conflict with global Foo
namespace {

class Foo {
public:
	Foo() = default;
	Foo(int value)
		: m_value(value) {
		// empty
	}
	virtual ~Foo() {
		Die();
	}

public:
	MOCK_METHOD0(Die, void());

public:
	int m_arg = 5;
	const int m_value = 0;
};

}  // namespace

#define MOCK_SETUP(name_)                                 \
	t::StrictMock<Foo>* name_ = new t::StrictMock<Foo>(); \
	bool name_##_deleted = 0;                             \
	EXPECT_CALL(*name_, Die())                            \
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

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, ctorMove_WithValue_ValueIsMoved) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> oth(pFoo);
	unique_ptr<Foo> ptr(std::move(oth));

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

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptr_Test, opMove_ValueToEmpty_ValueIsMoved) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr;
	unique_ptr<Foo> oth(pFoo);

	ptr = std::move(oth);

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
	Foo* p = ptr.operator->();

	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, opMemberOf_Value_CallObject) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	ptr->m_arg = 7;

	EXPECT_EQ(7, pFoo->m_arg);
}

TEST(unique_ptr_Test, opMemberOf_ValueAndConst_CallObject) {
	MOCK_SETUP(pFoo);

	const unique_ptr<Foo> ptr(pFoo);
	int i = ptr->m_arg;

	EXPECT_EQ(5, i);
}


//
// operator*
//

TEST(unique_ptr_Test, opInstance_Value_CallObject) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	Foo& foo = *ptr;
	foo.m_arg = 7;

	EXPECT_EQ(7, pFoo->m_arg);
}

TEST(unique_ptr_Test, opInstance_ValueAndConst_CallObject) {
	MOCK_SETUP(pFoo);

	const unique_ptr<Foo> ptr(pFoo);
	const Foo& foo = *ptr;
	int i = foo.m_arg;

	EXPECT_EQ(5, i);
}


//
// operator&
//

TEST(unique_ptr_Test, opAddressOf_Empty_ReturnNullptr) {
	unique_ptr<Foo> ptr;
	Foo** pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST(unique_ptr_Test, opAddressOf_Value_SetEmpty) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	Foo** pp = &ptr;

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
	Foo* p = ptr.get();

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
	Foo* p = ptr.get();

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
	Foo* p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST(unique_ptr_Test, release_Value_ReturnPointer) {
	MOCK_SETUP(pFoo);

	Foo* p;
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
	std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}

TEST(unique_ptr_Test, hash_Value_ReturnHash) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	std::size_t h = ptr.hash();

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

#pragma warning(suppress : 4834)
	oth.release();
}


//
// operator==(const com_heap_ptr<T>&, U* p)
// operator==(U* p, const com_heap_ptr<T>&)
// operator!=(const com_heap_ptr<T>&, U* p)
// operator!=(U* p, const com_heap_ptr<T>&)
//

TEST(unique_ptr_Test, opEqualsPointer_EmptyAndNullptr_Equal) {
	unique_ptr<Foo> ptr;
	Foo* p = nullptr;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	unique_ptr<Foo> ptr;
	Foo other;
	Foo* p = &other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	Foo* p = nullptr;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST(unique_ptr_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	Foo other;
	Foo* p = &other;

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
// operator==(const com_heap_ptr<T>&, nullptr_t)
// operator==(nullptr_t, const com_heap_ptr<T>&)
// operator!=(const com_heap_ptr<T>&, nullptr_t)
// operator!=(nullptr_t, const com_heap_ptr<T>&)
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
	EXPECT_EQ(1, ptr->m_value);
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
	std::size_t h = std::hash<unique_ptr<Foo>>{}(ptr);

	EXPECT_EQ(std::hash<Foo*>{}(ptr.get()), h);
}

TEST(unique_ptr_Test, stdHash_Value_ReturnHash) {
	MOCK_SETUP(pFoo);

	unique_ptr<Foo> ptr(pFoo);
	std::size_t h = std::hash<unique_ptr<Foo>>{}(ptr);

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

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptrArray_Test, ctorMove_WithValue_ValueIsMoved) {
	int* arr = new int[3];
	{
		unique_ptr<int[]> oth(arr);
		unique_ptr<int[]> ptr(std::move(oth));

		EXPECT_EQ(arr, ptr);
		EXPECT_NULL(oth);
		m4t::memory_start_tracking(arr);
	}
	m4t::memory_stop_tracking();
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
	int* arr = nullptr;
	unique_ptr<int[]> ptr(arr);

	EXPECT_NULL(ptr);
}

TEST(unique_ptrArray_Test, ctorFromPointer_WithValue_HasValue) {
	int* arr = new int[3];
	{
		unique_ptr<int[]> ptr(arr);

		EXPECT_EQ(arr, ptr);
		m4t::memory_start_tracking(arr);
	}
	m4t::memory_stop_tracking();
	EXPECT_DELETED(arr);
}


//
// ~unique_ptr<T[]>
//

TEST(unique_ptrArray_Test, dtor_Value_DeleteObject) {
	int* arr = new int[3];
	EXPECT_UNINITIALIZED(arr);

	{
		unique_ptr<int[]> ptr(arr);
		m4t::memory_start_tracking(arr);
	}
	m4t::memory_stop_tracking();
	EXPECT_DELETED(arr);
}


//
// operator=(unique_ptr<T[]>&&)
//

TEST(unique_ptrArray_Test, opMove_EmptyToEmpty_IsEmpty) {
	unique_ptr<int[]> ptr;
	unique_ptr<int[]> oth;

	ptr = std::move(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST(unique_ptrArray_Test, opMove_ValueToEmpty_ValueIsMoved) {
	int* arr = new int[3];

	{
		unique_ptr<int[]> ptr;
		unique_ptr<int[]> oth(arr);

		ptr = std::move(oth);

		EXPECT_EQ(arr, ptr);
		EXPECT_NULL(oth);
		m4t::memory_start_tracking(arr);
	}
	m4t::memory_stop_tracking();
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, opMove_ValueToValue_ValueIsMoved) {
	int* arr = new int[3];
	int* other = new int[3];

	{
		unique_ptr<int[]> ptr(arr);
		unique_ptr<int[]> oth(other);

		m4t::memory_start_tracking(arr);
		ptr = std::move(oth);

		EXPECT_EQ(other, ptr);  // value must have changed
		EXPECT_NULL(oth);
		EXPECT_DELETED(arr);
		m4t::memory_start_tracking(other);
	}
	m4t::memory_stop_tracking();
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
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	m4t::memory_start_tracking(arr);
	ptr = nullptr;
	m4t::memory_stop_tracking();

	EXPECT_NULL(ptr);
	EXPECT_DELETED(arr);
}


//
// operator&
//

TEST(unique_ptrArray_Test, opAddressOf_Empty_ReturnNullptr) {
	unique_ptr<int[]> ptr;
	int** pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST(unique_ptrArray_Test, opAddressOf_Value_SetEmpty) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	m4t::memory_start_tracking(arr);
	int** pp = &ptr;
	m4t::memory_stop_tracking();

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, opAddressOf_SetValue_HasValue) {
	int* arr = new int[3];

	{
		unique_ptr<int[]> ptr;
		int** pp = &ptr;
		ASSERT_NOT_NULL(pp);

		*pp = arr;

		EXPECT_EQ(arr, ptr);
		m4t::memory_start_tracking(arr);
	}
	m4t::memory_stop_tracking();
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
	int& v0 = ptr[0];
	int& v1 = ptr[1];
	int& v2 = ptr[2];

	EXPECT_EQ(v0, arr[0]);
	EXPECT_EQ(v1, arr[1]);
	EXPECT_EQ(v2, arr[2]);
}


//
// (bool)
//

TEST(unique_ptrArray_Test, opBool_Empty_ReturnFalse) {
	unique_ptr<int[]> ptr;

	bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST(unique_ptrArray_Test, opBool_Value_ReturnTrue) {
	unique_ptr<int[]> ptr(new int[3]);

	bool b = (bool) ptr;

	EXPECT_TRUE(b);
}


//
// get
//

TEST(unique_ptrArray_Test, get_Empty_ReturnNullptr) {
	unique_ptr<int[]> ptr;
	int* p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, get_EmptyAndConst_ReturnNullptr) {
	const unique_ptr<int> ptr;
	const int* p = ptr.get();

	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, get_Value_ReturnPointer) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	int* p = ptr.get();

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

	m4t::memory_start_tracking(arr);
	ptr.reset();
	m4t::memory_stop_tracking();

	EXPECT_NULL(ptr);
	EXPECT_DELETED(arr);
}

TEST(unique_ptrArray_Test, reset_ValueWithValue_ReferenceIsAdded) {
	int* arr = new int[3];
	int* other = new int[4];

	unique_ptr<int[]> ptr(arr);

	m4t::memory_start_tracking(arr);
	ptr.reset(other);
	m4t::memory_stop_tracking();

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
	int* p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST(unique_ptrArray_Test, release_Value_ReturnPointer) {
	int* arr = new int[3]{1, 2, 3};
	int* p;
	{
		unique_ptr<int[]> ptr(arr);
		p = ptr.release();

		EXPECT_NULL(ptr);
		EXPECT_EQ(arr, p);
	}
	EXPECT_THAT(1, arr[0]);
	EXPECT_THAT(2, arr[1]);
	EXPECT_THAT(3, arr[2]);

	delete p;
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
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	unique_ptr<int[]> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(arr, oth);
	EXPECT_UNINITIALIZED(arr);
}

TEST(unique_ptrArray_Test, swap_EmptyWithValue_ValueAndEmpty) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr;
	unique_ptr<int[]> oth(arr);

	ptr.swap(oth);

	EXPECT_EQ(arr, ptr);
	EXPECT_NULL(oth);
	EXPECT_UNINITIALIZED(arr);
}

TEST(unique_ptrArray_Test, swap_ValueWithValue_ValueAndValue) {
	int* arr = new int[3];
	int* arrOther = new int[3];

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
	std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<void*>{}(ptr.get()), h);
}

TEST(unique_ptrArray_Test, hash_Value_ReturnHash) {
	int* arr = new int[3];

	unique_ptr<int[]> ptr(arr);
	std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<void*>{}(ptr.get()), h);
}


//
// make_unique
//

TEST(unique_ptrArray_Test, makeUnique_WithSize_ArrayCreated) {
	unique_ptr<int[]> ptr = make_unique<int[]>(3);

	EXPECT_NOT_NULL(ptr);
	EXPECT_UNINITIALIZED(ptr.get());
}

}  // namespace m3c::test
