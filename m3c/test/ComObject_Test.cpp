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

#include "Bar.h"
#include "Foo.h"
#include "m3c/exception.h"

#include <gtest/gtest.h>
#include <m4t/m4t.h>

#include <objbase.h>
#include <unknwn.h>
#include <windows.h>

#include <new>


namespace m3c::test {

//
// GetObjectCount
//

TEST(ComObjectTest, GetObjectCount_Initial_Return0) {
	EXPECT_EQ(0u, AbstractComObject::GetObjectCount());
}

TEST(ComObjectTest, GetObjectCount_Create1Object_Return1) {
	Foo* pFoo = new Foo();

	EXPECT_EQ(1u, AbstractComObject::GetObjectCount());

	pFoo->Release();
}

TEST(ComObjectTest, GetObjectCount_Create2Objects_Return2) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();

	EXPECT_EQ(2u, AbstractComObject::GetObjectCount());

	pFoo->Release();
	pBar->Release();
}

TEST(ComObjectTest, GetObjectCount_Create3Objects_Return3) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();
	Foo* pBaz = new Foo();

	EXPECT_EQ(3u, AbstractComObject::GetObjectCount());

	pFoo->Release();
	pBar->Release();
	pBaz->Release();
}

TEST(ComObjectTest, GetObjectCount_AddRef_NoChange) {
	Foo* pFoo = new Foo();
	pFoo->AddRef();

	EXPECT_EQ(1u, AbstractComObject::GetObjectCount());

	pFoo->Release();
	pFoo->Release();
}

TEST(ComObjectTest, GetObjectCount_ReleaseNonFinal_NoChange) {
	Foo* pFoo = new Foo();
	pFoo->AddRef();
	pFoo->Release();

	EXPECT_EQ(1u, AbstractComObject::GetObjectCount());

	pFoo->Release();
}

TEST(ComObjectTest, GetObjectCount_ReleaseFinal_Reduce) {
	Foo* pFoo = new Foo();
	Bar* pBar = new Bar();
	pFoo->Release();

	EXPECT_EQ(1u, AbstractComObject::GetObjectCount());

	pBar->Release();
}


//
// AddRef
// Release
//

TEST(ComObjectTest, ReferenceCount_AddAndRelease_IsChanged) {
	Foo* pFoo = new Foo();

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

TEST(ComObjectTest, QueryInterface_IidIsNotSupported_ReturnNoInterface) {
	Foo foo;
	IFoo* pFoo = (IFoo*) m4t::kInvalidPtr;
	HRESULT hr = foo.QueryInterface(IID_IBar, (void**) &pFoo);

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pFoo);
}

TEST(ComObjectTest, QueryInterface_ObjectIsNullptr_ReturnInvalidArgument) {
	Foo foo;
#pragma warning(suppress : 6387)
	HRESULT hr = foo.QueryInterface(IID_IUnknown, nullptr);

	EXPECT_EQ(E_INVALIDARG, hr);
	EXPECT_EQ(2u, foo.AddRef());

	foo.Release();
}

TEST(ComObjectTest, QueryInterface_Ok_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* pFoo = (IFoo*) m4t::kInvalidPtr;
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

TEST(ComObjectTest, QueryInterfaceWithIID_IIDIsNotSupported_ThrowsException) {
	Foo foo;

#pragma warning(suppress : 4834)
	EXPECT_THROW(foo.QueryInterface(IID_IStream), com_exception);
}

TEST(ComObjectTest, QueryInterfaceWithIID_Ok_ReturnObjectAndIncrementReferenceCount) {
	Foo foo;
	IFoo* pFoo = (IFoo*) foo.QueryInterface(IID_IFoo);

	ASSERT_EQ(&foo, pFoo);
	EXPECT_EQ(3u, foo.AddRef());

	foo.Release();
	pFoo->Release();
}

}  // namespace m3c::test
