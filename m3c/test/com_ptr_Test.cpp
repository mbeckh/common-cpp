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

#include "m3c/com_ptr.h"

#include "Foo.h"

#include "m3c/COM.h"
#include "m3c/ComObject.h"
#include "m3c/exception.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <llamalog/llamalog.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <objbase.h>
#include <objidl.h>
#include <unknwn.h>
#include <windows.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>


namespace m3c::test {

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
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStream_Mock>);
	COM_MOCK_DECLARE(m_other, t::StrictMock<m4t::IStream_Mock>);
	t::MockFunction<void()> m_check;
};


//
// com_ptr()
//

TEST_F(com_ptr_Test, ctor_Default_IsEmpty) {
	com_ptr<IStream> ptr;

	EXPECT_NULL(ptr);
}


//
// com_ptr(const com_ptr&)
//

TEST_F(com_ptr_Test, ctorCopy_WithEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<IStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopy_WithValue_ReferencedIsAdded) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release()).Times(2);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}


//
// com_ptr(const com_ptr<S>&)
//


TEST_F(com_ptr_Test, ctorCopyAndQuery_WithEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<IPersistStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithSubInterfaceEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<ISequentialStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithSubInterfaceValue_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	const com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithBaseInterface_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	const com_ptr<IUnknown> oth(&m_object);
	com_ptr<IStream> ptr(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithUnsupportedInterfaceEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<IPersistStream> ptr(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorCopyAndQuery_WithUnsupportedInterfaceValue_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);

	com_ptr<IStream> oth(&m_object);

	EXPECT_THROW(com_ptr<IPersistStream> ptr(oth), com_exception);

	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

//
// com_ptr(com_ptr&&)
//

TEST_F(com_ptr_Test, ctorMove_WithEmpty_IsEmpty) {
	const com_ptr<IStream> oth;
	com_ptr<IStream> ptr(std::move(oth));

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, ctorMove_WithValue_ReferencedIsMoved) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(std::move(oth));

	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// com_ptr(std::nullptr_t)
//

TEST_F(com_ptr_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	com_ptr<IStream> ptr(nullptr);

	EXPECT_NULL(ptr);
}


//
// com_ptr(T*)
//

TEST_F(com_ptr_Test, ctorFromPointer_WithNullptr_IsEmpty) {
	IStream* pStream = nullptr;
	com_ptr<IStream> ptr(pStream);

	EXPECT_NULL(ptr);
}

TEST_F(com_ptr_Test, ctorFromPointer_WithValue_ReferencedIsAdded) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// ~com_ptr
//

TEST_F(com_ptr_Test, dtor_Default_ReferenceIsReleased) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	{
		const com_ptr<IStream> ptr(&m_object);
		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	}
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}


//
// operator=(const com_ptr&)
//

TEST_F(com_ptr_Test, opCopy_EmptyToEmpty_IsEmpty) {
	com_ptr<IStream> oth;
	com_ptr<IStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopy_ValueToEmpty_ReferencedIsAdded) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release()).Times(2);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr;
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_EmptyToValue_ReferencedIsReleased) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> oth;
	com_ptr<IStream> ptr(&m_object);
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_ValueToValue_ReferencedIsAdded) {
	t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_other, AddRef()).InSequence(s);
	calls += EXPECT_CALL(m_other, Release()).InSequence(s);
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_other);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopy_ValueToSameValue_NoChange) {
	t::Sequence s;
	t::ExpectationSet calls;
	// allow with check for same value and without
	calls += EXPECT_CALL(m_object, AddRef()).Times(t::Between(2, 3)).InSequence(s);
	calls += EXPECT_CALL(m_object, Release()).Times(t::Between(0, 1)).InSequence(s);
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_object);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}


//
// operator=(const com_ptr<S>&)
//

TEST_F(com_ptr_Test, opCopyAndQuery_EmptyToEmpty_IsEmpty) {
	com_ptr<IStream> oth;
	com_ptr<ISequentialStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToEmpty_ReferencedIsAdded) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr;
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_EmptyToValue_ReferencedIsReleased) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> oth;
	com_ptr<ISequentialStream> ptr(&m_object);
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToValue_ReferencedIsAdded) {
	t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	calls += EXPECT_CALL(m_other, AddRef()).InSequence(s);
	calls += EXPECT_CALL(m_other, Release()).InSequence(s);
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr(&m_other);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_ValueToSameValue_NoChange) {
	t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(3).InSequence(s);
	calls += EXPECT_CALL(m_object, Release()).InSequence(s);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_ISequentialStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<ISequentialStream> ptr(&m_object);
	ptr = oth;

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedEmptyToEmpty_Empty) {
	com_ptr<IStream> oth;
	com_ptr<IPersistStream> ptr;
	ptr = oth;

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedValueToEmpty_ThrowsException) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_));
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> oth(&m_object);
	com_ptr<IPersistStream> ptr;
	EXPECT_THROW(ptr = oth, com_exception);

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opCopyAndQuery_UnsupportedValueToValue_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef());
	calls += EXPECT_CALL(m_other, QueryInterface(IID_IPersistStream, t::_));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);
	EXPECT_CALL(m_other, Release()).After(check);

	com_ptr<IStream> oth(&m_other);
	com_ptr<IPersistStream> ptr((IPersistStream*) &m_object);
	EXPECT_THROW(ptr = oth, com_exception);

	EXPECT_EQ((void*) &m_object, ptr);
	EXPECT_EQ(&m_other, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// operator=(com_ptr&&)
//

TEST_F(com_ptr_Test, opMove_EmptyToEmpty_IsEmpty) {
	com_ptr<IStream> oth;
	com_ptr<IStream> ptr;
	ptr = std::move(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
}

TEST_F(com_ptr_Test, opMove_ValueToEmpty_ReferencedIsMoved) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr;
	ptr = std::move(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_EmptyToValue_IsEmpty) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> oth;
	com_ptr<IStream> ptr(&m_object);
	ptr = std::move(oth);

	EXPECT_NULL(ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_ValueToValue_ReferencedIsMoved) {
	t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef()).InSequence(s);
	calls += EXPECT_CALL(m_other, Release()).InSequence(s);
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_other);
	ptr = std::move(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, opMove_ValueToSameValue_NoChange) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> oth(&m_object);
	com_ptr<IStream> ptr(&m_object);
	ptr = std::move(oth);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
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
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> ptr(&m_object);
	ptr = nullptr;

	EXPECT_NULL(ptr);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}


//
// operator->
//

TEST_F(com_ptr_Test, opMemberOf_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;
	IStream* p = ptr.operator->();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, opMemberOf_Value_CallFunction) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Commit(7)).WillOnce(t::Return(E_NOTIMPL));
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	HRESULT hr = ptr->Commit(7);

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(E_NOTIMPL, hr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// operator&
//

TEST_F(com_ptr_Test, opAddressOf_Empty_ReturnAddress) {
	com_ptr<IStream> ptr;
	IStream** pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NOT_NULL(pp);
}

TEST_F(com_ptr_Test, opAddressOf_Value_SetEmpty) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> ptr(&m_object);
	IStream** pp = &ptr;

	EXPECT_NULL(ptr);
	EXPECT_NULL(*pp);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, opAddressOf_SetValue_HasValue) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr;
	IStream** pp = &ptr;
	ASSERT_NOT_NULL(pp);

	m_object.AddRef();
	*pp = &m_object;

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// (bool)
//

TEST_F(com_ptr_Test, opBool_Empty_ReturnFalse) {
	com_ptr<IStream> ptr;

	bool b = (bool) ptr;

	EXPECT_FALSE(b);
}

TEST_F(com_ptr_Test, opBool_Value_ReturnTrue) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);

	bool b = (bool) ptr;

	EXPECT_TRUE(b);
	m_check.Call();
}


//
// get
//

TEST_F(com_ptr_Test, get_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;
	IStream* p = ptr.get();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, get_EmptyAndConst_ReturnNullptr) {
	const com_ptr<IStream> ptr;
	IStream* p = ptr.get();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, get_Value_ReturnPointer) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	IStream* p = ptr.get();

	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, get_ValueAndConst_ReturnPointer) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	const com_ptr<IStream> ptr(&m_object);
	IStream* p = ptr.get();

	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// get_owner()
//

TEST_F(com_ptr_Test, getOwner_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;
	IStream* p = ptr.get_owner();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, getOwner_Value_ReturnPointer) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release()).Times(2);

	com_ptr<IStream> ptr(&m_object);
	IStream* p = ptr.get_owner();

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();

	p->Release();
}


//
// get_owner<Q>()
//

TEST_F(com_ptr_Test, getOwnerAndQuery_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;
	IStream* p = ptr.get_owner();

	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, getOwnerAndQuery_Value_ReturnPointer) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef()).Times(2);
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_))
				 .WillOnce(QueryInterface(&m_object));
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).Times(2).After(check);

	com_ptr<IStream> ptr(&m_object);
	IPersistStream* p = ptr.get_owner<IPersistStream>();

	EXPECT_EQ(&m_object, ptr);
	EXPECT_EQ(&m_object, (void*) p);
	COM_MOCK_EXPECT_REFCOUNT(3u, m_object);
	m_check.Call();

	p->Release();
}

TEST_F(com_ptr_Test, getOwnerAndQuery_ValueWithUnknownInterface_ThrowsException) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_object, QueryInterface(IID_IPersistStream, t::_))
				 .WillOnce(m4t::QueryInterfaceFail());
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);

	com_ptr<IStream> ptr(&m_object);

#pragma warning(suppress : 4834)
	EXPECT_THROW(ptr.get_owner<IPersistStream>(), com_exception);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
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
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_object, Release());
	EXPECT_CALL(m_check, Call());

	com_ptr<IStream> ptr(&m_object);

	ptr.reset();

	EXPECT_NULL(ptr);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, reset_ValueWithValue_ReferenceIsAdded) {
	t::Sequence s;
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef()).InSequence(s);
	calls += EXPECT_CALL(m_other, Release()).InSequence(s);
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);

	com_ptr<IStream> ptr(&m_other);

	ptr.reset(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(1u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, reset_ValueWithSameValue_NoChange) {
	t::InSequence s;
	// allow with check for self reset and without
	EXPECT_CALL(m_object, AddRef()).Times(t::Between(1, 2));
	EXPECT_CALL(m_object, Release()).Times(t::Between(0, 1));
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);

	ptr.reset(&m_object);

	EXPECT_EQ(&m_object, ptr);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}


//
// release
//

TEST_F(com_ptr_Test, release_Empty_ReturnNullptr) {
	com_ptr<IStream> ptr;

	IStream* p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_NULL(p);
}

TEST_F(com_ptr_Test, release_Value_ReturnPointer) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);

	IStream* p = ptr.release();

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, p);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
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
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth;

	ptr.swap(oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, swap_EmptyWithValue_ValueAndEmpty) {
	t::InSequence s;
	EXPECT_CALL(m_other, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_other, Release());

	com_ptr<IStream> ptr;
	com_ptr<IStream> oth(&m_other);

	ptr.swap(oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, swap_ValueWithValue_ValueAndValue) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef());
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);
	EXPECT_CALL(m_other, Release()).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_other);

	ptr.swap(oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_other);
	m_check.Call();
}


//
// hash
//

TEST_F(com_ptr_Test, hash_Empty_ReturnHash) {
	com_ptr<IStream> ptr;
	size_t h = ptr.hash();

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
}

TEST_F(com_ptr_Test, hash_Value_ReturnHash) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	size_t h = ptr.hash();

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
	m_check.Call();
}


//
// operator==(const com_ptr<T>&, const com_ptr<U>&)
// operator!=(const com_ptr<T>&, const com_ptr<U>&)
//

TEST_F(com_ptr_Test, opEquals_EmptyAndEmpty_Equal) {
	com_ptr<IStream> ptr;
	com_ptr<IStream> oth;

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);
}

TEST_F(com_ptr_Test, opEquals_EmptyAndValue_NotEqual) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr;
	com_ptr<IStream> oth(&m_object);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndValue_NotEqual) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef());
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);
	EXPECT_CALL(m_other, Release()).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_other);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndSameValue_Equal) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release()).Times(2);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_object);

	EXPECT_TRUE(ptr == oth);
	EXPECT_TRUE(oth == ptr);

	EXPECT_FALSE(ptr != oth);
	EXPECT_FALSE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndValueOfDifferentType_NotEqual) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef());
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);
	EXPECT_CALL(m_other, Release()).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IUnknown> oth(&m_other);

	EXPECT_FALSE(ptr == oth);
	EXPECT_FALSE(oth == ptr);

	EXPECT_TRUE(ptr != oth);
	EXPECT_TRUE(oth != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEquals_ValueAndSameValueOfDifferentType_ReturnTrue) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef()).Times(2);
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release()).Times(2);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IUnknown> oth(&m_object);

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
	com_ptr<IStream> ptr;
	IStream* p = nullptr;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);
}

TEST_F(com_ptr_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	com_ptr<IStream> ptr;
	IStream* p = &m_other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	IStream* p = nullptr;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	IStream* p = &m_other;

	EXPECT_FALSE(ptr == p);
	EXPECT_FALSE(p == ptr);

	EXPECT_TRUE(ptr != p);
	EXPECT_TRUE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndSamePointer_Equal) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	IStream* p = &m_object;

	EXPECT_TRUE(ptr == p);
	EXPECT_TRUE(p == ptr);

	EXPECT_FALSE(ptr != p);
	EXPECT_FALSE(p != ptr);

	m_check.Call();
}

TEST_F(com_ptr_Test, opEqualsPointer_ValueAndSamePointerOfDifferentType_Equal) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	IUnknown* p = &m_object;

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
	com_ptr<IStream> ptr;

	EXPECT_TRUE(ptr == nullptr);
	EXPECT_TRUE(nullptr == ptr);

	EXPECT_FALSE(ptr != nullptr);
	EXPECT_FALSE(nullptr != ptr);
}

TEST_F(com_ptr_Test, opEqualsNullptr_Value_NotEqual) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);

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
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth;

	swap(ptr, oth);

	EXPECT_NULL(ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	m_check.Call();
}

TEST_F(com_ptr_Test, stdSwap_EmptyWithValue_ValueAndEmpty) {
	t::InSequence s;
	EXPECT_CALL(m_other, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_other, Release());

	com_ptr<IStream> ptr;
	com_ptr<IStream> oth(&m_other);

	swap(ptr, oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_NULL(oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_other);
	m_check.Call();
}

TEST_F(com_ptr_Test, stdSwap_ValueWithValue_ValueAndValue) {
	t::ExpectationSet calls;
	calls += EXPECT_CALL(m_object, AddRef());
	calls += EXPECT_CALL(m_other, AddRef());
	t::Expectation check = EXPECT_CALL(m_check, Call()).After(calls);
	EXPECT_CALL(m_object, Release()).After(check);
	EXPECT_CALL(m_other, Release()).After(check);

	com_ptr<IStream> ptr(&m_object);
	com_ptr<IStream> oth(&m_other);

	swap(ptr, oth);

	EXPECT_EQ(&m_other, ptr);
	EXPECT_EQ(&m_object, oth);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
	COM_MOCK_EXPECT_REFCOUNT(2u, m_other);
	m_check.Call();
}


//
// hash
//

TEST_F(com_ptr_Test, stdHash_Empty_ReturnHash) {
	com_ptr<IStream> ptr;
	size_t h = std::hash<com_ptr<IStream>>{}(ptr);

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
}

TEST_F(com_ptr_Test, stdHash_Value_ReturnHash) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	com_ptr<IStream> ptr(&m_object);
	size_t h = std::hash<com_ptr<IStream>>{}(ptr);

	EXPECT_EQ(std::hash<IStream*>{}(ptr.get()), h);
	m_check.Call();
}


//
// make_com
//

TEST_F(com_ptr_Test, makeCom_Interface_ReturnObject) {
	const ULONG objectCount = COM::GetObjectCount();

	com_ptr<IFoo> ptr = make_com<IFoo, Foo>();

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(0, ptr->GetValue());
	EXPECT_EQ(objectCount + 1, COM::GetObjectCount());
}

TEST_F(com_ptr_Test, makeCom_InterfaceWithArgs_ReturnObject) {
	const ULONG objectCount = COM::GetObjectCount();

	com_ptr<IFoo> ptr = make_com<IFoo, Foo>(7);

	EXPECT_NOT_NULL(ptr);
	EXPECT_EQ(7, ptr->GetValue());
	EXPECT_EQ(objectCount + 1, COM::GetObjectCount());
}

}  // namespace m3c::test
