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

#include "Bar.h"
#include "Foo.h"

#include "m3c/COM.h"
#include "m3c/com_ptr.h"

#include <gtest/gtest.h>
#include <m4t/m4t.h>

#include <windows.h>
#include <unknwn.h>

namespace m3c::test {

//
// LockServer
//

TEST(ClassFactory_Test, LockServer_InitialCount_LockCountIs0) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();

	EXPECT_EQ(0u, COM::GetLockCount());
}

TEST(ClassFactory_Test, LockServer_Lock_LockCountIs1) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();

	{
		HRESULT hr = pClassFactory->LockServer(TRUE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(1u, COM::GetLockCount());
	}
	{
		HRESULT hr = pClassFactory->LockServer(FALSE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(0u, COM::GetLockCount());
	}
}

TEST(ClassFactory_Test, LockServer_LockDifferentClassFactory_LockCountIs2) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	com_ptr<IClassFactory> pOtherClassFactory = make_com<IClassFactory, ClassFactory<Bar>>();
	ASSERT_HRESULT_SUCCEEDED(pClassFactory->LockServer(TRUE));

	{
		HRESULT hr = pOtherClassFactory->LockServer(TRUE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(2u, COM::GetLockCount());
	}
	{
		HRESULT hr = pOtherClassFactory->LockServer(FALSE);

		ASSERT_HRESULT_SUCCEEDED(hr);
		EXPECT_EQ(1u, COM::GetLockCount());
	}

	ASSERT_HRESULT_SUCCEEDED(pClassFactory->LockServer(FALSE));
	EXPECT_EQ(0u, COM::GetLockCount());
}


//
// CreateInstance
//

TEST(ClassFactory_Test, CreateInstance_OuterIsNotNullptr_ReturnNoAggregation) {
	com_ptr<IFoo> foo = make_com<IFoo, Foo>();
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IUnknown* pClass = m4t::kInvalidPtr<IUnknown>;
	HRESULT hr = pClassFactory->CreateInstance(foo.get(), IID_IFoo, (void**) &pClass);

	EXPECT_EQ(CLASS_E_NOAGGREGATION, hr);
	EXPECT_NULL(pClass);
}

TEST(ClassFactory_Test, CreateInstance_IIDIsNotValid_ReturnNoInterface) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IUnknown* pClass = m4t::kInvalidPtr<IUnknown>;
	HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IClassFactory, (void**) &pClass);

	EXPECT_EQ(E_NOINTERFACE, hr);
	EXPECT_NULL(pClass);
}

TEST(ClassFactory_Test, CreateInstance_ObjectIsNullptr_ReturnInvalidArgument) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IFoo, nullptr);

	EXPECT_EQ(E_INVALIDARG, hr);
}

TEST(ClassFactory_Test, CreateInstance_Ok_ReturnObject) {
	com_ptr<IClassFactory> pClassFactory = make_com<IClassFactory, ClassFactory<Foo>>();
	IUnknown* pClass = m4t::kInvalidPtr<IUnknown>;
	HRESULT hr = pClassFactory->CreateInstance(nullptr, IID_IFoo, (void**) &pClass);

	ASSERT_HRESULT_SUCCEEDED(hr);
	ASSERT_NOT_NULL(pClass);
	ASSERT_NE(m4t::kInvalidPtr<IUnknown>, pClass);

	EXPECT_EQ(0u, pClass->Release());
}

}  // namespace m3c::test
