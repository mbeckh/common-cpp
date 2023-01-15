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

#include "m3c/com_heap_ptr.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/LogListener.h>
#include <m4t/MallocSpy.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <objbase.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <string>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

class com_heap_ptr_Test : public t::Test {
protected:
	void SetUp() override {
		ASSERT_HRESULT_SUCCEEDED(CoRegisterMallocSpy(&m_spy));
	}

	void TearDown() override {
		EXPECT_EQ(0, m_spy.GetAllocatedCount());
		EXPECT_HRESULT_SUCCEEDED(CoRevokeMallocSpy());
	}

protected:
	m4t::MallocSpy m_spy;
	m4t::LogListener m_log = m4t::LogListenerMode::kStrictAll;
};


// Macros so that correct line is given in test report
#define EXPECT_COM_ALLOCATED(p_) EXPECT_TRUE(m_spy.IsAllocated(p_))
#define EXPECT_COM_DELETED(p_) EXPECT_TRUE(m_spy.IsDeleted(p_))
#define EXPECT_COM_ALLOCATED_COUNT(count_) EXPECT_EQ(count_, m_spy.GetAllocatedCount())
#define EXPECT_COM_DELETED_COUNT(count_) EXPECT_EQ(count_, m_spy.GetDeletedCount())


//
// com_heap_ptr()
//

TEST_F(com_heap_ptr_Test, ctor_Default_IsEmpty) {
	com_heap_ptr<int> ptr;

	EXPECT_NULL(ptr);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// com_heap_ptr(com_heap_ptr&&)
//

TEST_F(com_heap_ptr_Test, ctorMove_WithEmpty_IsEmpty) {
	com_heap_ptr<int> oth;
	com_heap_ptr<int> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, ctorMove_WithValue_ValueIsMoved) {
	com_heap_ptr<int> oth(7);
	const int* const p = oth.get();
	com_heap_ptr<int> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(p, ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// com_heap_ptr(nullptr_t)
//

TEST_F(com_heap_ptr_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	com_heap_ptr<int> ptr(nullptr);

	EXPECT_NULL(ptr);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// com_heap_ptr(T*)
//

TEST_F(com_heap_ptr_Test, ctorFromPointer_WithNullptr_IsEmpty) {
	int* p = nullptr;
	com_heap_ptr<int> ptr(p);

	EXPECT_NULL(ptr);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, ctorFromPointer_WithValue_HasValue) {
	int* p = static_cast<int*>(CoTaskMemAlloc(8));
	com_heap_ptr<int> ptr(p);

	EXPECT_EQ(p, ptr);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// com_heap_ptr(size_t)
//

TEST_F(com_heap_ptr_Test, ctorFromSize_WithValue_CreateObject) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();

	EXPECT_NOT_NULL(ptr);
	EXPECT_NOT_NULL(p);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// ~com_heap_ptr
//

TEST_F(com_heap_ptr_Test, dtor_Value_DeleteObject) {
	int* p = static_cast<int*>(CoTaskMemAlloc(7));
	{
		com_heap_ptr<int> ptr(p);

		EXPECT_COM_ALLOCATED(p);
		EXPECT_COM_ALLOCATED_COUNT(1);
		EXPECT_COM_DELETED_COUNT(0);
	}
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(1);
}

//
// operator=(com_heap_ptr&&)
//

TEST_F(com_heap_ptr_Test, opMove_EmptyToEmpty_IsEmpty) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth;

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}


TEST_F(com_heap_ptr_Test, opMove_ValueToEmpty_ValueIsMoved) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth(7);
	const int* const p = oth.get();

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(p, ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, opMove_EmptyToValue_IsEmpty) {
	com_heap_ptr<int> ptr(7);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth;

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(1);
}

TEST_F(com_heap_ptr_Test, opMove_ValueToValue_ValueIsMoved) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth(7);
	const int* const o = oth.get();

	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(o, ptr);
	EXPECT_NE(p, ptr);  // value must have changed
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED(o);
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(1);
}


//
// operator=(nullptr_t)
//

TEST_F(com_heap_ptr_Test, opAssign_NullptrToEmpty_IsEmpty) {
	com_heap_ptr<int> ptr;

	ptr = nullptr;

	EXPECT_NULL(ptr);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, opAssign_NullptrToValue_ValueIsCleared) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();

	ptr = nullptr;

	EXPECT_NULL(ptr);
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(1);
}


//
// operator&
//

TEST_F(com_heap_ptr_Test, opAddressOf_Empty_ReturnNullptr) {
	com_heap_ptr<int> ptr;
	const int* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, opAddressOf_Value_SetEmpty) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();
	const int* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(1);
}

TEST_F(com_heap_ptr_Test, opAddressOf_SetValue_HasValue) {
	com_heap_ptr<int> ptr;
	int** pp = &ptr;
	ASSERT_NOT_NULL(pp);

	int* p = static_cast<int*>(CoTaskMemAlloc(4));
	*pp = p;

	EXPECT_EQ(p, ptr);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// (bool)
//

TEST_F(com_heap_ptr_Test, opBool_Empty_ReturnFalse) {
	com_heap_ptr<int> ptr;

	const bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST_F(com_heap_ptr_Test, opBool_Value_ReturnTrue) {
	com_heap_ptr<int> ptr(6);

	const bool b = (bool) ptr;

	EXPECT_TRUE(b);
}


//
// get
//

TEST_F(com_heap_ptr_Test, get_Empty_ReturnNullptr) {
	com_heap_ptr<int> ptr;
	const int* const p = ptr.get();

	EXPECT_NULL(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, get_EmptyAndConst_ReturnNullptr) {
	const com_heap_ptr<int> ptr;
	const int* const p = ptr.get();

	EXPECT_NULL(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, get_Value_ReturnPointer) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();

	EXPECT_NOT_NULL(p);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, get_ValueAndConst_ReturnPointer) {
	const com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();

	EXPECT_NOT_NULL(p);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// release
//

TEST_F(com_heap_ptr_Test, release_Empty_ReturnNullptr) {
	com_heap_ptr<int> ptr;
	const int* const p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, release_Value_ReturnPointer) {
	int* p = nullptr;
	{
		com_heap_ptr<int> ptr(6);
		const int* const addr = ptr.get();
		p = ptr.release();

		EXPECT_NULL(ptr);
		EXPECT_NOT_NULL(p);
		EXPECT_EQ(addr, p);
	}
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);

	CoTaskMemFree(p);
	EXPECT_COM_ALLOCATED_COUNT(0);
	EXPECT_COM_DELETED_COUNT(1);
}


//
// realloc
//

TEST_F(com_heap_ptr_Test, realloc_Empty_IsValue) {
	com_heap_ptr<int> ptr;
	ptr.realloc(5);

	EXPECT_NOT_NULL(ptr);
	EXPECT_COM_ALLOCATED(ptr.get());
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, realloc_EmptyWithSize0_IsValue) {
	com_heap_ptr<int> ptr;
	ptr.realloc(0);

	EXPECT_NOT_NULL(ptr);
	EXPECT_COM_ALLOCATED(ptr.get());
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, realloc_Value_IsValue) {
	com_heap_ptr<int> ptr(7);
	const int* const p = ptr.get();
	ptr.realloc(12);

	EXPECT_NOT_NULL(ptr);
	EXPECT_COM_ALLOCATED(ptr.get());
	EXPECT_COM_DELETED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(1);
}


//
// swap
//

TEST_F(com_heap_ptr_Test, swap_EmptyWithEmpty_AreEmpty) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_heap_ptr_Test, swap_ValueWithEmpty_EmptyAndValue) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(p, oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, swap_EmptyWithValue_ValueAndEmpty) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth(4);
	const int* const p = oth.get();

	ptr.swap(oth);

	EXPECT_EQ(p, ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, swap_ValueWithValue_ValueAndValue) {
	com_heap_ptr<int> ptr(7);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth(4);
	const int* const o = oth.get();

	ptr.swap(oth);

	EXPECT_EQ(o, ptr);
	EXPECT_EQ(p, oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED(o);
	EXPECT_COM_ALLOCATED_COUNT(2);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// hash
//

TEST_F(com_heap_ptr_Test, hash_Empty_ReturnHash) {
	com_heap_ptr<int> ptr;
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<int*>{}(ptr.get()), h);
}

TEST_F(com_heap_ptr_Test, hash_Value_ReturnHash) {
	com_heap_ptr<int> ptr(7);
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<int*>{}(ptr.get()), h);
}


//
// operator==(const com_heap_ptr&, const com_heap_ptr&)
// operator!=(const com_heap_ptr&, const com_heap_ptr&)
//

TEST_F(com_heap_ptr_Test, opEquals_EmptyAndEmpty_Equal) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth;

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);
}

TEST_F(com_heap_ptr_Test, opEquals_EmptyAndValue_NotEqual) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth(3);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);
}

TEST_F(com_heap_ptr_Test, opEquals_ValueAndValue_NotEqual) {
	com_heap_ptr<int> ptr(3);
	com_heap_ptr<int> oth(7);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);
}

TEST_F(com_heap_ptr_Test, opEquals_ValueAndSameValue_Equal) {
	com_heap_ptr<int> ptr(3);
	com_heap_ptr<int> oth(ptr.get());

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);

	// MUST release to prevent double free
	EXPECT_EQ(ptr.get(), oth.release());
}


//
// operator==(const com_heap_ptr<T>&, U* p)
// operator==(U* p, const com_heap_ptr<T>&)
// operator!=(const com_heap_ptr<T>&, U* p)
// operator!=(U* p, const com_heap_ptr<T>&)
//

TEST_F(com_heap_ptr_Test, opEqualsPointer_EmptyAndNullptr_Equal) {
	com_heap_ptr<int> ptr;
	const int* const p = nullptr;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}

TEST_F(com_heap_ptr_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	com_heap_ptr<int> ptr;
	const int i = 7;
	const int* const p = &i;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST_F(com_heap_ptr_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	com_heap_ptr<int> ptr(3);
	const int* const p = nullptr;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST_F(com_heap_ptr_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	com_heap_ptr<int> ptr(3);
	const int i = 3;
	const int* const p = &i;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST_F(com_heap_ptr_Test, opEqualsPointer_ValueAndSamePointer_Equal) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}


//
// operator==(const com_heap_ptr<T>&, nullptr_t)
// operator==(nullptr_t, const com_heap_ptr<T>&)
// operator!=(const com_heap_ptr<T>&, nullptr_t)
// operator!=(nullptr_t, const com_heap_ptr<T>&)
//

TEST_F(com_heap_ptr_Test, opEqualsNullptr_Empty_Equal) {
	com_heap_ptr<int> ptr;

	EXPECT_TRUE(ptr == nullptr);
	EXPECT_TRUE(nullptr == ptr);

	EXPECT_FALSE(ptr != nullptr);
	EXPECT_FALSE(nullptr != ptr);
}

TEST_F(com_heap_ptr_Test, opEqualsNullptr_Value_NoEqual) {
	com_heap_ptr<int> ptr(3);

	EXPECT_FALSE(ptr == nullptr);
	EXPECT_FALSE(nullptr == ptr);

	EXPECT_TRUE(ptr != nullptr);
	EXPECT_TRUE(nullptr != ptr);
}


//
// std::swap
//

TEST_F(com_heap_ptr_Test, stdSwap_EmptyWithEmpty_AreEmpty) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth;

	std::swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_heap_ptr_Test, stdSwap_ValueWithEmpty_EmptyAndValue) {
	com_heap_ptr<int> ptr(3);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth;

	std::swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(p, oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, stdSwap_EmptyWithValue_ValueAndEmpty) {
	com_heap_ptr<int> ptr;
	com_heap_ptr<int> oth(4);
	const int* const p = oth.get();

	std::swap(ptr, oth);

	EXPECT_EQ(p, ptr);
	EXPECT_NULL(oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED_COUNT(1);
	EXPECT_COM_DELETED_COUNT(0);
}

TEST_F(com_heap_ptr_Test, stdSwap_ValueWithValue_ValueAndValue) {
	com_heap_ptr<int> ptr(7);
	const int* const p = ptr.get();
	com_heap_ptr<int> oth(4);
	const int* const o = oth.get();

	ptr.swap(oth);

	EXPECT_EQ(o, ptr);
	EXPECT_EQ(p, oth);
	EXPECT_COM_ALLOCATED(p);
	EXPECT_COM_ALLOCATED(o);
	EXPECT_COM_ALLOCATED_COUNT(2);
	EXPECT_COM_DELETED_COUNT(0);
}


//
// std::hash
//

TEST_F(com_heap_ptr_Test, stdHash_Empty_ReturnHash) {
	com_heap_ptr<int> ptr;
	const std::size_t h = std::hash<com_heap_ptr<int>>{}(ptr);

	EXPECT_EQ(std::hash<int*>{}(ptr.get()), h);
}

TEST_F(com_heap_ptr_Test, stdHash_Value_ReturnHash) {
	com_heap_ptr<int> ptr(7);
	std::size_t h = std::hash<com_heap_ptr<int>>{}(ptr);

	EXPECT_EQ(std::hash<int*>{}(ptr.get()), h);
}


//
// Formatting
//

TEST_F(com_heap_ptr_Test, format_Empty_PrintNullptr) {
	com_heap_ptr<int> arg;

	EXPECT_EQ("0x0", fmt::to_string(arg));
}

TEST_F(com_heap_ptr_Test, format_EmptyCentered_PrintNullptrCentered) {
	com_heap_ptr<int> arg;

	EXPECT_EQ(" 0x0 ", fmt::format("{:^5}", arg));
}

TEST_F(com_heap_ptr_Test, format_Value_PrintPointer) {
	com_heap_ptr<int> arg(7);
	const void* const expectPtr = arg.get();

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ(fmt::to_string(expectPtr), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
}

TEST_F(com_heap_ptr_Test, format_ValueW_PrintPointer) {
	com_heap_ptr<int> arg(7);
	const void* const expectPtr = arg.get();

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(fmt::to_wstring(expectPtr), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
}

TEST_F(com_heap_ptr_Test, format_ValueCentered_PrintPointerCentered) {
	com_heap_ptr<int> arg(7);
	const void* const expectPtr = arg.get();

	const std::string str = fmt::format("{:^20}", arg);

	EXPECT_EQ(20, str.length());
	EXPECT_EQ(fmt::format("{:^20}", expectPtr), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+0x[0-9a-f]+\\s+"));
}


//
// Logging
//

TEST_F(com_heap_ptr_Test, LogString_Empty_LogNullptr) {
	com_heap_ptr<int> arg;

	EXPECT_CALL(m_log, Debug(t::_, "0x0\t"));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_heap_ptr_Test, LogStrimg_Value_LogPointer) {
	com_heap_ptr<int> arg(7);

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt::ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_heap_ptr_Test, LogEvent_Empty_LogNullptr) {
	com_heap_ptr<int> arg;

	EXPECT_CALL(m_log, Debug(t::_, "ptr=0x0\t"));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_heap_ptr_Test, LogEvent_Value_LogPointer) {
	com_heap_ptr<int> arg;

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr={}\t", fmt::ptr(arg.get()))));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get())));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_heap_ptr_Test, LogException_Empty_LogNullptr) {
	try {
		com_heap_ptr<int> arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, "ptr=0x0\t")).InSequence(seqString);
		EXPECT_CALL(m_log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr))).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(m_log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST_F(com_heap_ptr_Test, LogException_Value_LogPointer) {
	try {
		com_heap_ptr<int> arg(7);

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr={}\t", fmt::ptr(arg.get())))).InSequence(seqString);
		EXPECT_CALL(m_log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get()))).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(m_log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
