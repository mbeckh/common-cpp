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

#include "Bar.h"
#include "Foo.h"
#include "testevents.h"

#include "m3c/COM.h"
#include "m3c/exception.h"

#include <gtest/gtest.h>
#include <m4t/m4t.h>

#include <windows.h>
#include <objbase.h>
#include <unknwn.h>

#include <new>

// INSTALL_TRACKING_NEW_DELETE()

namespace m3c {
class com_error;
}

namespace m3c::test {

//
// GetObjectCount
//

TEST(ComObject_Test, GetObjectCount_Initial_Return0) {
	EXPECT_EQ(0u, COM::GetObjectCount());
}

TEST(ComObject_Test, GetObjectCount_Create1Object_Return1) {
	Foo* pFoo = new Foo();

	EXPECT_EQ(1u, COM::GetObjectCount());

	pFoo->Release();
}

TEST(ComObject_Test, GetObjectCount_Create2Objects_Return2) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();

	EXPECT_EQ(2u, COM::GetObjectCount());

	pFoo->Release();
	pBar->Release();
}

TEST(ComObject_Test, GetObjectCount_Create3Objects_Return3) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();
	Foo* pBaz = new Foo();

	EXPECT_EQ(3u, COM::GetObjectCount());

	pFoo->Release();
	pBar->Release();
	pBaz->Release();
}

TEST(ComObject_Test, GetObjectCount_AddRef_NoChange) {
	Foo* pFoo = new Foo();
	pFoo->AddRef();

	EXPECT_EQ(1u, COM::GetObjectCount());

	pFoo->Release();
	pFoo->Release();
}

TEST(ComObject_Test, GetObjectCount_ReleaseNonFinal_NoChange) {
	Foo* pFoo = new Foo();
	pFoo->AddRef();
	pFoo->Release();

	EXPECT_EQ(1u, COM::GetObjectCount());

	pFoo->Release();
}

TEST(ComObject_Test, GetObjectCount_ReleaseFinal_Reduce) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();
	pFoo->Release();

	EXPECT_EQ(1u, COM::GetObjectCount());

	pBar->Release();
}


//
// AddRef
// Release
//

TEST(ComObject_Test, ReferenceCount_AddAndRelease_IsChanged) {
	Foo* const pFoo = new Foo();

	EXPECT_EQ(2u, pFoo->AddRef());
	EXPECT_EQ(3u, pFoo->AddRef());

	EXPECT_EQ(2u, pFoo->Release());
	EXPECT_EQ(1u, pFoo->Release());
	EXPECT_EQ(0u, pFoo->Release());
	EXPECT_DELETED(pFoo);
}


//
// QueryInterface(REFIID, void**)
//

TEST(ComObject_Test, QueryInterface_IidIsNotSupported_ReturnNoInterface) {
	Foo foo;
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	HRESULT hr = foo.QueryInterface(IID_IBar, (void**) &pFoo);

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pFoo);
}

TEST(ComObject_Test, QueryInterface_ObjectIsNullptr_ReturnInvalidArgument) {
	Foo foo;
	HRESULT hr = foo.QueryInterface(IID_IUnknown, nullptr);

	EXPECT_EQ(E_INVALIDARG, hr);
	EXPECT_EQ(2u, foo.AddRef());

	foo.Release();
}

TEST(ComObject_Test, QueryInterface_Ok_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	HRESULT hr = foo.QueryInterface(IID_IUnknown, (void**) &pFoo);

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3u, foo.AddRef());

	foo.Release();
	pFoo->Release();
}


//
// QueryInterface(REFIID)
//

TEST(ComObject_Test, QueryInterfaceWithIID_IIDIsNotSupported_ThrowsException) {
	Foo foo;

	EXPECT_THAT([&foo]() { return foo.QueryInterface(IID_IStream); },
	            t::Throws<internal::ExceptionDetail<com_error>>(
					t::AllOf(
						t::Property(&com_error::code, t::Property(&std::error_code::value, E_NOINTERFACE)),
						t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::IUnknown_QueryInterface.Id)))));
}

TEST(ComObject_Test, QueryInterfaceWithIID_Ok_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* const pFoo = static_cast<IFoo*>(foo.QueryInterface(IID_IFoo));

	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3u, foo.AddRef());

	foo.Release();
	pFoo->Release();
}

}  // namespace m3c::test
