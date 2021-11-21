/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed onan "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "m3c/ClassFactory.h"

#include "ComObjects.h"

#include "m3c/COM.h"
#include "m3c/com_ptr.h"

#include <m4t/m4t.h>

#include <gtest/gtest.h>

#include <windows.h>
#include <unknwn.h>

namespace m3c::test {
namespace {

//
// LockServer
//

TEST(ClassFactory_Test, LockServer_InitialCount_LockCountIs0) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();

	EXPECT_EQ(0, COM::GetLockCount());
}

TEST(ClassFactory_Test, LockServer_Lock_LockCountIs1) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();

	{
		const HRESULT hr = pClassFactory->LockServer(TRUE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(1, COM::GetLockCount());
	}
	{
		const HRESULT hr = pClassFactory->LockServer(FALSE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(0, COM::GetLockCount());
	}
}

TEST(ClassFactory_Test, LockServer_LockDifferentClassFactory_LockCountIs2) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	com_ptr<IClassFactory> pOtherClassFactory = make_com<IClassFactory, ClassFactory<Bar>>();
	ASSERT_HRESULT_SUCCEEDED(pClassFactory->LockServer(TRUE));

	{
		const HRESULT hr = pOtherClassFactory->LockServer(TRUE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(2, COM::GetLockCount());
	}
	{
		const HRESULT hr = pOtherClassFactory->LockServer(FALSE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(1, COM::GetLockCount());
	}

	ASSERT_HRESULT_SUCCEEDED(pClassFactory->LockServer(FALSE));
	EXPECT_EQ(0, COM::GetLockCount());
}


//
// CreateInstance
//

TEST(ClassFactory_Test, CreateInstance_SingleInterface_ReturnObject) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	const HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IFoo, reinterpret_cast<void**>(&pFoo));

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_NOT_NULL(pFoo);
	ASSERT_NE(m4t::kInvalidPtr<IFoo>, pFoo);

	EXPECT_EQ(0, pFoo->Release());
}

TEST(ClassFactory_Test, CreateInstance_TwoInterfaces_ReturnObject) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<FooBar>>();

	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	const HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IFoo, reinterpret_cast<void**>(&pFoo));

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_NOT_NULL(pFoo);
	ASSERT_NE(m4t::kInvalidPtr<IFoo>, pFoo);

	EXPECT_EQ(0, pFoo->Release());
}

TEST(ClassFactory_Test, CreateInstance_Aggregated_ReturnObject) {
	com_ptr<IFoo> outer = make_com<IFoo, Foo>();
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IUnknown* pUnknown = m4t::kInvalidPtr<IUnknown>;
	const HRESULT hr = pClassFactory->CreateInstance(outer.get(), IID_IUnknown, reinterpret_cast<void**>(&pUnknown));

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_NOT_NULL(pUnknown);
	ASSERT_NE(m4t::kInvalidPtr<IUnknown>, pUnknown);

	EXPECT_EQ(0, pUnknown->Release());
}

TEST(ClassFactory_Test, CreateInstance_AggregatedAndNotIUnknown_ReturnNoInterface) {
	com_ptr<IFoo> outer = make_com<IFoo, Foo>();
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IFoo* pFoo = m4t::kInvalidPtr<IFoo>;
	const HRESULT hr = pClassFactory->CreateInstance(outer.get(), IID_IFoo, reinterpret_cast<void**>(&pFoo));

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pFoo);
}

TEST(ClassFactory_Test, CreateInstance_IIDIsNotValid_ReturnNoInterface) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IBar* pBar = m4t::kInvalidPtr<IBar>;
	const HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IBar, reinterpret_cast<void**>(&pBar));

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pBar);
}

TEST(ClassFactory_Test, CreateInstance_ObjectIsNullptr_ReturnInvalidArgument) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	const HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IFoo, nullptr);

	EXPECT_EQ(E_INVALIDARG, hr);
}

}  // namespace
}  // namespace m3c::test
