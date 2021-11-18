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

#pragma once

#include "m3c/ComObject.h"

#include <guiddef.h>
#include <unknwn.h>

namespace m3c::test {

// NOLINTBEGIN(readability-identifier-naming): IID.
extern const IID IID_IFoo;
extern const IID IID_IBar;
// NOLINTEND(readability-identifier-naming)


MIDL_INTERFACE("3997F2B2-973A-4070-8AB6-EA7BD63EA2BF")
IFoo : public IUnknown {  // NOLINT(cppcoreguidelines-virtual-class-destructor): Interface class.
public:
	virtual int __stdcall GetValue() noexcept = 0;
};

MIDL_INTERFACE("9B4D833C-BDA0-422D-A55E-A4E681D3D75C")
IBar : public IUnknown{
           // no additional methods
       };

class Foo : public ComObject<IFoo> {
public:
	Foo() noexcept = default;
	explicit Foo(IUnknown* const pOuter) noexcept
	    : ComObject(pOuter) {
		// empty
	}
	explicit Foo(int value) noexcept
	    : m_value(value) {
		// empty
	}

public:
	int __stdcall GetValue() noexcept override {
		return m_value;
	}

private:
	int m_value = 0;
};

class Bar : public ComObject<IBar> {
	// default
};

class FooBar : public ComObject<IFoo, IBar> {
public:
	int __stdcall GetValue() noexcept override {
		return 42;
	}
};

}  // namespace m3c::test
