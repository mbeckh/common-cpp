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

extern const IID IID_IFoo;

MIDL_INTERFACE("3997F2B2-973A-4070-8AB6-EA7BD63EA2BF")
IFoo : public IUnknown {
	virtual int __stdcall GetValue() = 0;
};

class Foo : public ComObject<IFoo> {
public:
	Foo() = default;
	Foo(int value)
		: m_value(value) {
		// empty
	}
	~Foo() = default;

public:
	virtual int __stdcall GetValue() {
		return m_value;
	}

private:
	int m_value = 0;
};

}  // namespace m3c::test
