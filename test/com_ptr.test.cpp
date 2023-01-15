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

/// @file

#include "m3c/com_ptr.h"

#include "ComObjects.h"

#include "m3c/COM.h"
#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/IStreamMock.h>
#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <evntprov.h>
#include <objbase.h>
#include <objidl.h>
#include <unknwn.h>
#include <wtypes.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <string>
#include <system_error>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

class com_ptr_Test : public t::Test {
protected:
	void SetUp() override {
		COM_MOCK_SETUP(m_object, IStream, ISequentialStream);
		COM_MOCK_SETUP(m_other, IStream, ISequentialStream);

		// this test wants to set expectations for AddRef, Release and QueryInterface itself
		t::Mock::VerifyAndClearExpectations(&m_object);
		t::Mock::VerifyAndClearExpectations(&m_other);
	}

	void TearDown() override {
		COM_MOCK_VERIFY(m_object);
		COM_MOCK_VERIFY(m_other);
	}

protected:
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStreamMock>);
	COM_MOCK_DECLARE(m_other, t::StrictMock<m4t::IStreamMock>);
	t::MockFunction<void()> m_check;
	m4t::LogListener m_log = m4t::LogListenerMode::kStrictEvent;
};


//
// com_ptr()
//


TEST_F(com_ptr_Test, ctor_Default_IsEmpty) {
	const com_ptr<IStream> ptr;

	EXPECT_NULL(ptr);
}


//
// com_ptr(const com_ptr&)
//

TEST_F(com_ptr_Test, ctorCopy_WithEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	const com_ptr<IStream> ptr(oth);  // NOLINT(performance-unnecessary-copy-initialization): Check copy operation.

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopy_WithValue_ReferencedIsAdded) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release).Times(2);

	const com_ptr<IStream> oth(&m_object);
	const com_ptr<IStream> ptr(oth);  // NOLINT(performance-unnecessary-copy-initialization): Check copy operation.

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}


//
// com_ptr(const com_ptr<S>&)
//

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	const com_ptr<IPersistStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithSubInterfaceEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	const com_ptr<ISequentialStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithSubInterfaceValue_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	const com_ptr<ISequentialStream> ptr(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithBaseInterface_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IUnknown> oth(&m_object);
	const com_ptr<IStream> ptr(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithUnsupportedInterfaceEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	const com_ptr<IPersistStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithUnsupportedInterfaceValue_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);

	com_ptr<IStream> oth(&m_object);

	EXPECT_THAT([&oth]() { const com_ptr<IPersistStream> ptr(oth); },
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));

	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// com_ptr(com_ptr&&)
//

TEST_F(com_ptr_Test, ctorMove_WithEmpty_IsEmpty) {
	com_ptr<IStream> oth;
	const com_ptr<IStream> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorMove_WithValue_ReferencedIsMoved) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> oth(&m_object);
	const com_ptr<IStream> ptr(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// com_ptr(std::nullptr_t)
//

TEST_F(com_ptr_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	const com_ptr<IStream> ptr(nullptr);

	EXPECT_NULL(ptr);
}


//
// com_ptr(T*)
//

TEST_F(com_ptr_Test, ctorFromPointer_WithNullptr_IsEmpty) {
	IStream* const pStream = nullptr;
	const com_ptr<IStream> ptr(pStream);

	EXPECT_NULL(ptr);
}

TEST_F(com_ptr_Test, ctorFromPointer_WithValue_ReferencedIsAdded) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// ~com_ptr
//

TEST_F(com_ptr_Test, dtor_Default_ReferenceIsReleased) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	{
		const com_ptr<IStream> ptr(&m_object);
		COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	}
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}


//
// operator=(const com_ptr&)
//

TEST_F(com_ptr_Test, opCopy_EmptyToEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<IStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopy_ValueToEmpty_ReferencedIsAdded) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release).Times(2);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr;
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_EmptyToValue_ReferencedIsReleased) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	const com_ptr<IStream> oth;
	com_ptr<IStream> ptr(&m_object);
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_ValueToValue_ReferencedIsAdded) {
	const t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_other, AddRef).InSequence(s);
	calls += EXPECT_CALL(m_other, Release).InSequence(s);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_other);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_ValueToSameValue_NoChange) {
	const t::Sequence s;
	t::ExpectationSet calls;
	// allow with check for same value and without
	calls += EXPECT_CALL(m_object, AddRef).Times(t::Between(2, 3)).InSequence(s);
	calls += EXPECT_CALL(m_object, Release).Times(t::AtMost(1)).InSequence(s);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_object);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}


//
// operator=(const com_ptr<S>&)
//

TEST_F(com_ptr_Test, opCopyAndQuery_EmptyToEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<ISequentialStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToEmpty_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr;
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_EmptyToValue_ReferencedIsReleased) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	const com_ptr<IStream> oth;
	com_ptr<ISequentialStream> ptr(&m_object);
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToValue_ReferencedIsAdded) {
	const t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	calls += EXPECT_CALL(m_other, AddRef).InSequence(s);
	calls += EXPECT_CALL(m_other, Release).InSequence(s);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr(&m_other);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToSameValue_NoChange) {
	const t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(3).InSequence(s);
	calls += EXPECT_CALL(m_object, Release).InSequence(s);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr(&m_object);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedEmptyToEmpty_Empty) {
	const com_ptr<IStream> oth;
	com_ptr<IPersistStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedValueToEmpty_ThrowsException) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<IPersistStream> ptr;
	EXPECT_THAT(([&ptr, &oth]() { ptr = oth; }),
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedValueToValue_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef);
	calls += EXPECT_CALL(m_other, QueryInterface(IID_IPersistStream, t::_));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);
	EXPECT_CALL(m_other, Release).After(check);

	const com_ptr<IStream> oth(&m_other);
	com_ptr<IPersistStream> ptr(reinterpret_cast<IPersistStream*>(&m_object));
	EXPECT_THAT(([&ptr, &oth]() { ptr = oth; }),
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));

	EXPECT_EQ((void*) &m_object, ptr);
	EXPECT_EQ(&m_other, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// operator=(com_ptr&&)
//

TEST_F(com_ptr_Test, opMove_EmptyToEmpty_IsEmpty) {
	com_ptr<IStream> oth;
	com_ptr<IStream> ptr;
	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opMove_ValueToEmpty_ReferencedIsMoved) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr;
	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_EmptyToValue_IsEmpty) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	com_ptr<IStream> oth;
	com_ptr<IStream> ptr(&m_object);
	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_ValueToValue_ReferencedIsMoved) {
	const t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef).InSequence(s);
	calls += EXPECT_CALL(m_other, Release).InSequence(s);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_other);
	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_ValueToSameValue_NoChange) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_object);
	ptr = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// operator=(std::nullptr_t)
//

TEST_F(com_ptr_Test, opAssign_NullptrToEmpty_IsEmpty) {
	com_ptr<IStream> ptr;
	ptr = nullptr;

	EXPECT_NULL(ptr);
}

TEST_F(com_ptr_Test, opAssign_NullptrToValue_ReferencedIsReleased) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	com_ptr<IStream> ptr(&m_object);
	ptr = nullptr;

	EXPECT_NULL(ptr);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}


//
// operator->
//

TEST_F(com_ptr_Test, opMemberOf_Empty_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	const IStream* const p = ptr.operator->();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, opMemberOf_Value_CallFunction) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Commit(7)).WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const HRESULT hr = ptr->Commit(7);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(E_NOTIMPL, hr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// operator&
//

TEST_F(com_ptr_Test, opAddressOf_Empty_ReturnAddress) {
	com_ptr<IStream> ptr;
	const IStream* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST_F(com_ptr_Test, opAddressOf_Value_SetEmpty) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	com_ptr<IStream> ptr(&m_object);
	const IStream* const* const pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opAddressOf_SetValue_HasValue) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> ptr;
	IStream** const pp = &ptr;
	ASSERT_NOT_NULL(pp);

	m_object.AddRef();
	*pp = &m_object;

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// (bool)
//

TEST_F(com_ptr_Test, opBool_Empty_ReturnFalse) {
	const com_ptr<IStream> ptr;

	const bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST_F(com_ptr_Test, opBool_Value_ReturnTrue) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);

	const bool b = (bool) ptr;

	EXPECT_TRUE(b);
	m_check.Call();
}


//
// get
//

TEST_F(com_ptr_Test, get_Empty_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	const IStream* const p = ptr.get();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, get_EmptyAndConst_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	const IStream* const p = ptr.get();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, get_Value_ReturnPointer) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IStream* const p = ptr.get();

	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, get_ValueAndConst_ReturnPointer) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IStream* const p = ptr.get();

	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// get_owner()
//

TEST_F(com_ptr_Test, getOwner_Empty_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	const IStream* const p = ptr.get_owner();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, getOwner_Value_ReturnPointer) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release).Times(2);

	const com_ptr<IStream> ptr(&m_object);
	IStream* const p = ptr.get_owner();

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();

	p->Release();
}


//
// get_owner<Q>()
//

TEST_F(com_ptr_Test, getOwnerAndQuery_Empty_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	const IStream* const p = ptr.get_owner();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, getOwnerAndQuery_Value_ReturnPointer) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_))
	             .WillOnce(QueryInterface(&m_object));
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).Times(2).After(check);

	const com_ptr<IStream> ptr(&m_object);
	IPersistStream* const p = ptr.get_owner<IPersistStream>();

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, (void*) p);
	COM_MOCK_EXPECT_REFCOUNT(3, m_object);
	m_check.Call();

	p->Release();
}

TEST_F(com_ptr_Test, getOwnerAndQuery_ValueWithUnknownInterface_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_))
	             .WillOnce(m4t::QueryInterfaceFail());
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);

	const com_ptr<IStream> ptr(&m_object);

	EXPECT_THAT([&ptr]() { return ptr.get_owner<IPersistStream>(); },
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// reset
//

TEST_F(com_ptr_Test, reset_EmptyWithDefault_IsEmpty) {
	com_ptr<IStream> ptr;

	ptr.reset();

	EXPECT_NULL(ptr);
}

TEST_F(com_ptr_Test, reset_EmptyWithNullptr_IsEmpty) {
	com_ptr<IStream> ptr;

	ptr.reset(nullptr);

	EXPECT_NULL(ptr);
}

TEST_F(com_ptr_Test, reset_ValueWithDefault_ReferenceIsReleased) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);

	com_ptr<IStream> ptr(&m_object);

	ptr.reset();

	EXPECT_NULL(ptr);
	COM_MOCK_EXPECT_REFCOUNT(1, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, reset_ValueWithValue_ReferenceIsAdded) {
	const t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef).InSequence(s);
	calls += EXPECT_CALL(m_other, Release).InSequence(s);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);

	com_ptr<IStream> ptr(&m_other);

	ptr.reset(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, reset_ValueWithSameValue_NoChange) {
	const t::InSequence s;
	// allow with check for self reset and without
	EXPECT_CALL(m_object, AddRef).Times(t::Between(1, 2));
	EXPECT_CALL(m_object, Release).Times(t::AtMost(1));
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> ptr(&m_object);

	ptr.reset(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}


//
// release
//

TEST_F(com_ptr_Test, release_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;

	const IStream* const p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, release_Value_ReturnPointer) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> ptr(&m_object);

	IStream* const p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();

	p->Release();
}


//
// swap
//

TEST_F(com_ptr_Test, swap_EmptyWithEmpty_AreEmpty) {
	com_ptr<IStream> ptr;
	com_ptr<IStream> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, swap_ValueWithEmpty_EmptyAndValue) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, swap_EmptyWithValue_ValueAndEmpty) {
	const t::InSequence s;
	EXPECT_CALL(m_other, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_other, Release);

	com_ptr<IStream> ptr;
	com_ptr<IStream> oth(&m_other);

	ptr.swap(oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, swap_ValueWithValue_ValueAndValue) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);
	EXPECT_CALL(m_other, Release).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_other);

	ptr.swap(oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	COM_MOCK_EXPECT_REFCOUNT(2, m_other);
	m_check.Call();
}


//
// hash
//

TEST_F(com_ptr_Test, hash_Empty_ReturnHash) {
	const com_ptr<IStream> ptr;
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
}

TEST_F(com_ptr_Test, hash_Value_ReturnHash) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const std::size_t h = ptr.hash();

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
	m_check.Call();
}


//
// operator==(const com_ptr<T>&, const com_ptr<U>&)
// operator!=(const com_ptr<T>&, const com_ptr<U>&)
//

TEST_F(com_ptr_Test, opEquals_EmptyAndEmpty_Equal) {
	const com_ptr<IStream> ptr;
	const com_ptr<IStream> oth;

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);
}

TEST_F(com_ptr_Test, opEquals_EmptyAndValue_NotEqual) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr;
	const com_ptr<IStream> oth(&m_object);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndValue_NotEqual) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);
	EXPECT_CALL(m_other, Release).After(check);

	const com_ptr<IStream> ptr(&m_object);
	const com_ptr<IStream> oth(&m_other);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndSameValue_Equal) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release).Times(2);

	const com_ptr<IStream> ptr(&m_object);
	const com_ptr<IStream> oth(&m_object);

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndValueOfDifferentType_NotEqual) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);
	EXPECT_CALL(m_other, Release).After(check);

	const com_ptr<IStream> ptr(&m_object);
	const com_ptr<IUnknown> oth(&m_other);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndSameValueOfDifferentType_ReturnTrue) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef).Times(2);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release).Times(2);

	const com_ptr<IStream> ptr(&m_object);
	const com_ptr<IUnknown> oth(&m_object);

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);

	m_check.Call();
}


//
// operator==(const com_ptr<T>&, U* p)
// operator==(U* p, const com_ptr<T>&)
// operator!=(const com_ptr<T>&, U* p)
// operator!=(U* p, const com_ptr<T>&)
//

TEST_F(com_ptr_Test, opEqualsPointer_EmptyAndNullptr_Equal) {
	const com_ptr<IStream> ptr;
	const IStream* const p = nullptr;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}

TEST_F(com_ptr_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	const com_ptr<IStream> ptr;
	const IStream* const p = &m_other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IStream* const p = nullptr;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IStream* const p = &m_other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndSamePointer_Equal) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IStream* const p = &m_object;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndSamePointerOfDifferentType_Equal) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const IUnknown* const p = &m_object;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);

	m_check.Call();
}


//
// operator==(const com_ptr<T>&, std::nullptr_t)
// operator==(std::nullptr_t, const com_ptr<T>&)
// operator!=(const com_ptr<T>&, std::nullptr_t)
// operator!=(std::nullptr_t, const com_ptr<T>&)
//

TEST_F(com_ptr_Test, opEqualsNullptr_Empty_Equal) {
	const com_ptr<IStream> ptr;

	EXPECT_TRUE(ptr == nullptr);
	EXPECT_TRUE(nullptr == ptr);

	EXPECT_FALSE(ptr != nullptr);
	EXPECT_FALSE(nullptr != ptr);
}

TEST_F(com_ptr_Test, opEqualsNullptr_Value_NotEqual) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);

	EXPECT_FALSE(ptr == nullptr);
	EXPECT_FALSE(nullptr == ptr);

	EXPECT_TRUE(ptr != nullptr);
	EXPECT_TRUE(nullptr != ptr);

	m_check.Call();
}


//
// std::swap
//

TEST_F(com_ptr_Test, stdSwap_EmptyWithEmpty_AreEmpty) {
	com_ptr<IStream> ptr;
	com_ptr<IStream> oth;

	swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, stdSwap_ValueWithEmpty_EmptyAndValue) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth;

	swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, stdSwap_EmptyWithValue_ValueAndEmpty) {
	const t::InSequence s;
	EXPECT_CALL(m_other, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_other, Release);

	com_ptr<IStream> ptr;
	com_ptr<IStream> oth(&m_other);

	swap(ptr, oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, stdSwap_ValueWithValue_ValueAndValue) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef);
	calls += EXPECT_CALL(m_other, AddRef);
	const t::Expectation check = EXPECT_CALL(m_check, Call).After(calls);
	EXPECT_CALL(m_object, Release).After(check);
	EXPECT_CALL(m_other, Release).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_other);

	swap(ptr, oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2, m_object);
	COM_MOCK_EXPECT_REFCOUNT(2, m_other);
	m_check.Call();
}


//
// hash
//

TEST_F(com_ptr_Test, stdHash_Empty_ReturnHash) {
	const com_ptr<IStream> ptr;
	const std::size_t h = std::hash<com_ptr<IStream>>{}(ptr);

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
}

TEST_F(com_ptr_Test, stdHash_Value_ReturnHash) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_check, Call);
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> ptr(&m_object);
	const std::size_t h = std::hash<com_ptr<IStream>>{}(ptr);

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
	m_check.Call();
}


//
// make_com
//

TEST_F(com_ptr_Test, makeCom_Interface_ReturnObject) {
	const ULONG objectCount = COM::GetObjectCount();

	const com_ptr<IFoo> ptr = make_com<IFoo, Foo>();

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(0, ptr->GetValue());
	EXPECT_EQ(objectCount + 1, COM::GetObjectCount());
}

TEST_F(com_ptr_Test, makeCom_InterfaceWithArgs_ReturnObject) {
	const ULONG objectCount = COM::GetObjectCount();

	const com_ptr<IFoo> ptr = make_com<IFoo, Foo>(7);

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(7, ptr->GetValue());
	EXPECT_EQ(objectCount + 1, COM::GetObjectCount());
}


//
// Formatting
//

TEST_F(com_ptr_Test, format_IUnknownEmpty_PrintNullptr) {
	const com_ptr<IUnknown> arg;
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_string(arg));
}

TEST_F(com_ptr_Test, format_IUnknownEmptyCentered_PrintNullptrCentered) {
	com_ptr<IUnknown> arg;
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
}

TEST_F(com_ptr_Test, format_IUnknownValue_PrintPointer) {
	const com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_string(arg));
}

TEST_F(com_ptr_Test, format_IUnknownValueW_PrintPointer) {
	const com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);
	const std::wstring expect = fmt::to_wstring(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_wstring(arg));
}

TEST_F(com_ptr_Test, format_IUnknownValueCentered_PrintPointerCentered) {
	com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
}

TEST_F(com_ptr_Test, format_ObjectEmpty_PrintNullptr) {
	const com_ptr<IFoo> arg;
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_string(arg));
}

TEST_F(com_ptr_Test, format_ObjectEmptyCentered_PrintNullptrCentered) {
	com_ptr<IFoo> arg;
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
}

TEST_F(com_ptr_Test, format_ObjectValue_PrintPointer) {
	const com_ptr<IFoo> arg = make_com<IFoo, Foo>(7);
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_string(arg));
}

TEST_F(com_ptr_Test, format_ObjectValueCentered_PrintPointerCentered) {
	com_ptr<IFoo> arg = make_com<IFoo, Foo>(7);
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
}

TEST_F(com_ptr_Test, format_IStreamEmpty_PrintNullptr) {
	const com_ptr<IStream> arg;
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::to_string(arg));
}

TEST_F(com_ptr_Test, format_IStreamEmptyCentered_PrintNullptrCentered) {
	com_ptr<IStream> arg;
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
}

TEST_F(com_ptr_Test, format_IStreamValue_PrintPointer) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);  // 1
	EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);  // 2
	EXPECT_CALL(m_object, Release);

	const com_ptr<IStream> arg(&m_object);
	const std::string expect = fmt::to_string(fmt_ptr(arg.get()));
	m_check.Call();  // 1

	EXPECT_EQ(expect, fmt::to_string(arg));
	m_check.Call();  // 2
}

TEST_F(com_ptr_Test, format_IStreamValueCentered_PrintPointerCentered) {
	const t::InSequence s;
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);  // 1
	EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
	EXPECT_CALL(m_object, AddRef);
	EXPECT_CALL(m_object, Release);
	EXPECT_CALL(m_check, Call);  // 2
	EXPECT_CALL(m_object, Release);

	com_ptr<IStream> arg(&m_object);
	const std::string expect = fmt::format("{:^20}", fmt_ptr(arg.get()));
	m_check.Call();  // 1

	EXPECT_EQ(expect, fmt::format("{:^20}", arg));
	m_check.Call();  // 2
}


//
// Logging
//

TEST_F(com_ptr_Test, LogString_IUnknownEmpty_LogNullptr) {
	const com_ptr<IUnknown> arg;

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_ptr_Test, LogString_IUnknownValue_LogPointer) {
	const com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_ptr_Test, LogString_ObjectEmpty_LogNullptr) {
	const com_ptr<IFoo> arg;

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_ptr_Test, LogString_ObjectValue_LogPointer) {
	const com_ptr<IFoo> arg = make_com<IFoo, Foo>(7);

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_ptr_Test, LogString_IStreamEmpty_LogNullptr) {
	const com_ptr<IStream> arg;

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(com_ptr_Test, LogString_IStreamValue_LogPointer) {
	{
		const t::InSequence s;
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_check, Call);  // 1
		EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_object, Release);
		EXPECT_CALL(m_check, Call);  // 2
		EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_object, Release);
		EXPECT_CALL(m_check, Call);  // 3
		EXPECT_CALL(m_object, Release);
	}

	const com_ptr<IStream> arg(&m_object);
	m_check.Call();  // 1

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("{}\t", fmt_ptr(arg.get()))));

	m_check.Call();  // 2

	Log::Info("{}{}", arg, '\t');
	m_check.Call();  // 3
}

TEST_F(com_ptr_Test, LogEvent_IUnknownEmpty_LogNullptr) {
	const com_ptr<IUnknown> arg;

	EXPECT_CALL(m_log, Debug(t::_, "ptr=(ptr=0x0, ref=0)\t"));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_ptr_Test, LogEvent_IUnknownValue_LogPointer) {
	const com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr=(ptr={}, ref=1)\t", fmt::ptr(arg.get()))));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get())));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_ptr_Test, LogEvent_ObjectEmpty_LogNullptr) {
	const com_ptr<IFoo> arg;

	EXPECT_CALL(m_log, Debug(t::_, "ptr=(ptr=0x0, ref=0)\t"));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_ptr_Test, LogEvent_ObjectValue_LogPointer) {
	const com_ptr<IFoo> arg = make_com<IFoo, Foo>(7);

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr=(ptr={}, ref=1)\t", fmt::ptr(arg.get()))));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get())));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(com_ptr_Test, LogEvent_IStreamEmpty_LogNullptr) {
	const com_ptr<IStream> arg;

	EXPECT_CALL(m_log, Debug(t::_, "IStream=(ptr=0x0, ref=0), name=<Empty>\t"));
	EXPECT_CALL(m_log, Event(evt::Test_LogIStream.Id, t::_, t::_, 3));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(nullptr)));
	EXPECT_CALL(m_log, EventArg(1, 16, m4t::PointerAs<wchar_t>(t::StrEq(L"<Empty>"))));
	EXPECT_CALL(m_log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogIStream, arg, '\t');
}

TEST_F(com_ptr_Test, LogEvent_IStreamValue_LogPointer) {
	{
		const t::InSequence s;
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_check, Call);  // 1
		EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_object, Release);
		EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT)).WillOnce(m4t::IStream_Stat(L"test.dat"));
		EXPECT_CALL(m_check, Call);  // 2
		EXPECT_CALL(m_object, Release);
	}

	const com_ptr<IStream> arg(&m_object);
	m_check.Call();  // 1

	EXPECT_CALL(m_log, Debug(t::_, fmt::format("IStream=(ptr={}, ref=2), name=test.dat\t", fmt::ptr(arg.get()))));
	EXPECT_CALL(m_log, Event(evt::Test_LogIStream.Id, t::_, t::_, 3));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get())));
	EXPECT_CALL(m_log, EventArg(1, 18, m4t::PointerAs<wchar_t>(t::StrEq(L"test.dat"))));
	EXPECT_CALL(m_log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogIStream, arg, '\t');

	m_check.Call();  // 2
}

TEST_F(com_ptr_Test, LogException_IUnknownValue_LogPointer) {
	try {
		const com_ptr<IUnknown> arg = make_com<IUnknown, Foo>(7);

		const t::Sequence seqString;
		const t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr=(ptr={}, ref=1)\t", fmt::ptr(arg.get())))).InSequence(seqString);
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

TEST_F(com_ptr_Test, LogException_ObjectValue_LogPointer) {
	try {
		const com_ptr<IFoo> arg = make_com<IFoo, Foo>(7);

		const t::Sequence seqString;
		const t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, fmt::format("ptr=(ptr={}, ref=1)\t", fmt::ptr(arg.get())))).InSequence(seqString);
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

TEST_F(com_ptr_Test, LogException_IStreamValue_LogPointer) {
	{
		const t::InSequence s;
		EXPECT_CALL(m_object, AddRef);
		EXPECT_CALL(m_check, Call);      // 1
		EXPECT_CALL(m_object, AddRef);   // copy into exception
		EXPECT_CALL(m_object, AddRef);   // copy exception
		EXPECT_CALL(m_object, Release);  // release local copy
		EXPECT_CALL(m_object, Release);  // release local exception
		EXPECT_CALL(m_check, Call);      // 2
		EXPECT_CALL(m_object, Stat(t::_, STATFLAG_DEFAULT))
		    .Times(2)
		    .WillRepeatedly(m4t::IStream_Stat(L"test.dat"));
		EXPECT_CALL(m_object, AddRef);   // Log output
		EXPECT_CALL(m_object, Release);  // Log output
		EXPECT_CALL(m_check, Call);      // 3
		EXPECT_CALL(m_object, Release);  // end of catch block
		EXPECT_CALL(m_check, Call);      // 4
	}

	try {
		const com_ptr<IStream> arg(&m_object);

		const t::Sequence seqString;
		const t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, fmt::format("IStream=(ptr={}, ref=2), name=test.dat\t", fmt::ptr(arg.get())))).InSequence(seqString);
		EXPECT_CALL(m_log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(m_log, Event(evt::Test_LogIStream.Id, t::_, t::_, 3)).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(arg.get()))).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(1, 18, m4t::PointerAs<wchar_t>(t::StrEq(L"test.dat"))));
		EXPECT_CALL(m_log, EventArg(2, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(m_log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		m_check.Call();  // 1
		throw std::exception() + evt::Test_LogIStream << arg << '\t';
	} catch (...) {
		m_check.Call();  // 2
		Log::InfoException(evt::Test_LogException);
		m_check.Call();  // 3
	}
	m_check.Call();  // 4
}

}  // namespace
}  // namespace m3c::test
