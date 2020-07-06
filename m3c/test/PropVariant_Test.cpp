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

#include "m3c/PropVariant.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/IStream_Mock.h>
#include <m4t/m4t.h>

#include <objidl.h>
#include <wtypes.h>


namespace m3c::test {

class PropVariantTest : public t::Test {
protected:
	PropVariantTest() {
		COM_MOCK_SETUP(m_object, IStream);
	}

	~PropVariantTest() {
		COM_MOCK_VERIFY(m_object);
	}

protected:
	COM_MOCK_DECLARE(m_object, t::StrictMock<m4t::IStream_Mock>);
	t::MockFunction<void()> m_check;
};


//
// PropVariant
//

TEST_F(PropVariantTest, ctor_Ok_IsEmpty) {
	PropVariant pv;

	EXPECT_EQ(VT_EMPTY, pv.vt);
}


//
// ~PropVariant
//

TEST_F(PropVariantTest, dtor_Value_Cleanup) {
	t::InSequence s;
	EXPECT_CALL(m_object, AddRef());
	EXPECT_CALL(m_check, Call());
	EXPECT_CALL(m_object, Release());

	{
		PropVariant pv;
		pv.vt = VT_UNKNOWN;
		pv.punkVal = &m_object;
		m_object.AddRef();

		COM_MOCK_EXPECT_REFCOUNT(2u, m_object);
		m_check.Call();
	}
	COM_MOCK_EXPECT_REFCOUNT(1u, m_object);
}

}  // namespace m3c::test
