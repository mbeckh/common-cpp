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

#include "m3c/ComObject.h"

#include "ComObjects.h"

#include "m3c/COM.h"
#include "m3c/exception.h"

#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <evntprov.h>
#include <objbase.h>
#include <objidl.h>
#include <unknwn.h>

#include <new>  // IWYU pragma: keep
#include <system_error>

namespace m3c::test {
namespace {

namespace t = testing;

class Outer : public ComObject<IBar> {
public:
	Outer() noexcept
	    : ComObject()
	    , m_foo(this)
	    , m_pFoo(m_foo.QueryInterface<IUnknown>()) {
	}
	~Outer() noexcept override {
		m_pFoo->Release();
	}

public:
	ULONG GetFooRefCount() noexcept {
		m_pFoo->AddRef();
		return m_pFoo->Release();
	}

protected:
	[[nodiscard]] _Ret_maybenull_ void* FindInterface(REFIID riid) noexcept override {
		if (IsEqualIID(riid, IID_IFoo)) {
			return static_cast<IFoo*>(&m_foo);
		}
		return nullptr;
	}

private:
	Foo m_foo;
	IUnknown* m_pFoo;
};


//
// GetObjectCount
//

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): Deleted in call to Release().
TEST(ComObject_Test, GetObjectCount_Initial_Return0) {
	EXPECT_EQ(0, COM::GetObjectCount());
}

TEST(ComObject_Test, GetObjectCount_Create1Object_Return1) {
	Foo* pFoo = new Foo();

	EXPECT_EQ(1, COM::GetObjectCount());

	pFoo->Release();

	EXPECT_EQ(0, COM::GetObjectCount());
}

TEST(ComObject_Test, GetObjectCount_Create2Objects_Return2) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();

	EXPECT_EQ(2, COM::GetObjectCount());

	pFoo->Release();
	pBar->Release();

	EXPECT_EQ(0, COM::GetObjectCount());
}

TEST(ComObject_Test, GetObjectCount_Create3Objects_Return3) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();
	Foo* pBaz = new Foo();

	EXPECT_EQ(3, COM::GetObjectCount());

	pFoo->Release();
	pBar->Release();
	pBaz->Release();

	EXPECT_EQ(0, COM::GetObjectCount());
}

TEST(ComObject_Test, GetObjectCount_AddRef_NoChange) {
	Foo* pFoo = new Foo();
	pFoo->AddRef();

	EXPECT_EQ(1, COM::GetObjectCount());

	pFoo->Release();

	EXPECT_EQ(1, COM::GetObjectCount());

	pFoo->Release();

	EXPECT_EQ(0, COM::GetObjectCount());
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)


//
// AddRef
// Release
//

TEST(ComObject_Test, ReferenceCount_AddAndRelease_IsChanged) {
	Foo* const pFoo = new Foo();

	EXPECT_EQ(2, pFoo->AddRef());
	EXPECT_EQ(3, pFoo->AddRef());

	EXPECT_EQ(2, pFoo->Release());
	EXPECT_EQ(1, pFoo->Release());
	EXPECT_EQ(0, pFoo->Release());
	EXPECT_DELETED(pFoo);
}

TEST(ComObject_Test, ReferenceCount_AddAndReleaseAggregated_IsChanged) {
	Outer* const pOuter = new Outer();  // Outer == 1, Foo == 2

	EXPECT_EQ(2, pOuter->GetFooRefCount());

	IFoo* const pFoo = pOuter->QueryInterface<IFoo>();  // Outer == 2, Foo == 2
	EXPECT_NE(static_cast<void*>(pOuter), static_cast<void*>(pFoo));
	EXPECT_EQ(2, pOuter->GetFooRefCount());

	EXPECT_EQ(3, pFoo->AddRef());  // Outer == 3, Foo == 2
	EXPECT_EQ(4, pFoo->AddRef());  // Outer == 4, Foo == 2

	EXPECT_EQ(2, pOuter->GetFooRefCount());

	EXPECT_EQ(3, pFoo->Release());  // Outer == 3, Foo == 2
	EXPECT_EQ(2, pFoo->Release());  // Outer == 2, Foo == 2
	EXPECT_EQ(1, pFoo->Release());  // Outer == 1, Foo == 2

	EXPECT_EQ(0, pOuter->Release());  // Outer == 0, Foo == 0
	EXPECT_DELETED(pOuter);
}


//
// QueryInterface(REFIID, void**)
//

TEST(ComObject_Test, QueryInterface_IIDIsNotSupported_ReturnNoInterface) {
	Foo foo;
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	const HRESULT hr = foo.QueryInterface(IID_IBar, reinterpret_cast<void**>(&pFoo));

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pFoo);
}

TEST(ComObject_Test, QueryInterface_ObjectIsNullptr_ReturnInvalidArgument) {
	Foo foo;
	const HRESULT hr = foo.QueryInterface(IID_IUnknown, nullptr);

	EXPECT_EQ(E_INVALIDARG, hr);
	EXPECT_EQ(2, foo.AddRef());

	foo.Release();
}

TEST(ComObject_Test, QueryInterface_Unknown_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IUnknown* pUnknown = m4t::kInvalidPtr<IUnknown>;
	const HRESULT hr = foo.QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&pUnknown));

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_NOT_NULL(pUnknown);
	ASSERT_NE(&foo, pUnknown);  // returns internal IUnknown implementation
	EXPECT_EQ(3, foo.AddRef());

	foo.Release();
	pUnknown->Release();
}

TEST(ComObject_Test, QueryInterface_Object_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	const HRESULT hr = foo.QueryInterface(IID_IFoo, reinterpret_cast<void**>(&pFoo));

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3, foo.AddRef());

	foo.Release();
	pFoo->Release();
}

TEST(ComObject_Test, QueryInterface_Aggregated_ReturnObjectAndIncrementReferenceCount) {
	Outer* const pOuter = new Outer();                  // Outer == 1, Foo == 2
	IFoo* const pFoo = pOuter->QueryInterface<IFoo>();  // Outer == 2, Foo == 2
	ASSERT_NE(static_cast<void*>(pOuter), static_cast<void*>(pFoo));

	IBar* pBar = m4t::kInvalidPtr<IBar>;
	const HRESULT hr = pFoo->QueryInterface(IID_IBar, reinterpret_cast<void**>(&pBar));  // Outer == 3, Foo == 2

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_EQ(pBar, pOuter);
	EXPECT_EQ(4, pBar->AddRef());   // Outer == 4, Foo == 2
	EXPECT_EQ(3, pBar->Release());  // Outer == 3, Foo == 2

	EXPECT_EQ(2, pOuter->GetFooRefCount());

	EXPECT_EQ(2, pFoo->Release());  // Outer == 2, Foo == 2
	EXPECT_EQ(1, pBar->Release());  // Outer == 1, Foo == 2

	EXPECT_EQ(0, pOuter->Release());  // Outer == 0, Foo == 0
}


//
// QueryInterface(REFIID)
//

TEST(ComObject_Test, QueryInterfaceWithIID_IIDIsNotSupported_ThrowsException) {
	Foo foo;
	EXPECT_THAT([&foo]() { return foo.QueryInterface(IID_IStream); },
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));
}

TEST(ComObject_Test, QueryInterfaceWithIID_SingleInterface_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* const pFoo = static_cast<IFoo*>(foo.QueryInterface(IID_IFoo));

	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3, foo.AddRef());
	foo.Release();

	pFoo->Release();
}

TEST(ComObject_Test, QueryInterfaceWithIID_TwoInterfaces_ReturnObjectAndIncrementReferenceCount) {
	FooBar fooBar;

	IFoo* const pFoo = static_cast<IFoo*>(fooBar.QueryInterface(IID_IFoo));
	ASSERT_EQ(&fooBar, pFoo);
	EXPECT_EQ(3, fooBar.AddRef());
	fooBar.Release();

	IBar* const pBar = static_cast<IBar*>(fooBar.QueryInterface(IID_IBar));
	ASSERT_EQ(&fooBar, pBar);
	EXPECT_EQ(4, fooBar.AddRef());
	fooBar.Release();

	EXPECT_EQ(2, pFoo->Release());
	EXPECT_EQ(1, pBar->Release());
}


//
// QueryInterface<Interface>()
//

TEST(ComObject_Test, QueryInterfaceWithType_IIDIsNotSupported_ThrowsException) {
	Foo foo;
	EXPECT_THAT([&foo]() { return foo.QueryInterface<IStream>(); },
	            (t::Throws<internal::ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface_H.Id))))));
}

TEST(ComObject_Test, QueryInterfaceWithType_SingleInterface_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* const pFoo = foo.QueryInterface<IFoo>();

	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3, foo.AddRef());
	foo.Release();

	EXPECT_EQ(1, pFoo->Release());
}

TEST(ComObject_Test, QueryInterfaceWithType_TwoInterfaces_ReturnObjectAndIncrementReferenceCount) {
	FooBar fooBar;

	IFoo* const pFoo = fooBar.QueryInterface<IFoo>();
	ASSERT_EQ(&fooBar, pFoo);
	EXPECT_EQ(3, fooBar.AddRef());
	fooBar.Release();

	IBar* const pBar = fooBar.QueryInterface<IBar>();
	ASSERT_EQ(&fooBar, pBar);
	EXPECT_EQ(4, fooBar.AddRef());
	fooBar.Release();

	EXPECT_EQ(2, pFoo->Release());
	EXPECT_EQ(1, pBar->Release());
}

}  // namespace
}  // namespace m3c::test
