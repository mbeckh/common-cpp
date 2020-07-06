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

#include "m3c/rpc_string.h"

#include <gtest/gtest.h>
#include <llamalog/llamalog.h>
#include <m4t/m4t.h>

#include <rpc.h>
#include <unknwn.h>

#include <cstddef>
#include <functional>
#include <utility>


namespace m3c::test {

class rpc_stringTest : public t::Test {
public:
	rpc_stringTest() {
		Initialize();
	}

private:
	void Initialize() {
		ASSERT_EQ(RPC_S_OK, UuidToStringA(&IID_IClassFactory, &m_str));
		EXPECT_NOT_NULL(m_str);

		ASSERT_EQ(RPC_S_OK, UuidToStringA(&IID_IClassFactory, &m_other));
		EXPECT_NOT_NULL(m_other);
	}

protected:
	rpc_string m_str;
	rpc_string m_other;
};


//
// RpcString()
//

TEST_F(rpc_stringTest, ctor_Default_IsEmpty) {
	rpc_string str;

	EXPECT_EQ(nullptr, str);
}


//
// RpcString(RpcString&&)
//

TEST_F(rpc_stringTest, ctorMove_WithEmpty_IsEmpty) {
	rpc_string oth;
	rpc_string str(std::move(oth));

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_stringTest, ctorMove_WithValue_ValueIsMoved) {
	RPC_CSTR p = m_other.get();
	rpc_string str(std::move(m_other));

	EXPECT_NOT_NULL(str);
	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}


//
// RpcString(nullptr_t)
//

TEST_F(rpc_stringTest, ctorFromNullptr_WithNullptr_IsEmpty) {
	rpc_string str(nullptr);

	EXPECT_NULL(str);
}


//
// operator=(unique_ptr&&)
//

TEST_F(rpc_stringTest, opMove_EmptyToEmpty_IsEmpty) {
	rpc_string str;
	rpc_string oth;

	str = std::move(oth);

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_stringTest, opMove_ValueToEmpty_ValueIsMoved) {
	rpc_string str;
	RPC_CSTR p = m_other.get();

	str = std::move(m_other);

	EXPECT_NOT_NULL(str);
	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_stringTest, opMove_ValueToValue_ValueIsMoved) {
	RPC_CSTR p = m_other.get();

	m_str = std::move(m_other);

	EXPECT_NOT_NULL(m_str);
	EXPECT_EQ(p, m_str);  // value must have changed
	EXPECT_NULL(m_other);
}


//
// operator=(nullptr_t)
//

TEST_F(rpc_stringTest, opAssign_NullptrToEmpty_IsEmpty) {
	rpc_string str;
	str = nullptr;

	EXPECT_NULL(str);
}

TEST_F(rpc_stringTest, opAssign_NullptrToValue_ValueIsCleared) {
	m_str = nullptr;

	EXPECT_NULL(m_str);
}


//
// operator&
//

TEST_F(rpc_stringTest, opAddressOf_Empty_ReturnAddress) {
	rpc_string str;
	RPC_CSTR* pp = &str;

	EXPECT_NULL(str);
	EXPECT_NOT_NULL(pp);
}

TEST_F(rpc_stringTest, opAddressOf_Value_SetEmpty) {
	RPC_CSTR* pp = &m_str;

	EXPECT_NULL(m_str);
	EXPECT_NOT_NULL(pp);
}

TEST_F(rpc_stringTest, opAddressOf_SetValue_HasValue) {
	RPC_CSTR* pp = &m_str;
	ASSERT_NOT_NULL(pp);

	ASSERT_EQ(RPC_S_OK, UuidToStringA(&IID_IClassFactory, pp));

	EXPECT_NOT_NULL(*pp);
	EXPECT_EQ(*pp, m_str);
}


//
// (bool)
//

TEST_F(rpc_stringTest, opBool_Empty_ReturnFalse) {
	rpc_string str;

	EXPECT_FALSE(str);
}

TEST_F(rpc_stringTest, opBool_Value_ReturnTrue) {
	EXPECT_TRUE(m_str);
}


//
// get
//

TEST_F(rpc_stringTest, Get_Empty_ReturnNullptr) {
	rpc_string str;
	RPC_CSTR p = str.get();

	EXPECT_NULL(p);
}

TEST_F(rpc_stringTest, Get_Value_ReturnPointer) {
	RPC_CSTR p = m_str.get();

	EXPECT_NOT_NULL(p);
}


//
// as_native
//

TEST_F(rpc_stringTest, AsNative_Empty_ReturnNullptr) {
	rpc_string str;
	const char* p = str.as_native();

	EXPECT_NULL(p);
}

TEST_F(rpc_stringTest, AsNative_Value_ReturnPointer) {
	const char* p = m_str.as_native();

	EXPECT_NOT_NULL(p);
}


//
// Release
//

TEST_F(rpc_stringTest, Release_Empty_ReturnNullptr) {
	rpc_string str;
	RPC_CSTR p = str.release();

	EXPECT_NULL(str);
	EXPECT_NULL(p);
}

TEST_F(rpc_stringTest, Release_Value_ReturnPointer) {
	RPC_CSTR p;
	{
		RPC_CSTR addr = m_str.get();
		p = m_str.release();

		EXPECT_NULL(m_str);
		EXPECT_EQ(addr, p);
	}

	EXPECT_EQ(RPC_S_OK, RpcStringFreeA(&p));
}


//
// swap
//

TEST_F(rpc_stringTest, Swap_EmptyWithEmpty_AreEmpty) {
	rpc_string str;
	rpc_string oth;

	str.swap(oth);

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_stringTest, Swap_ValueWithEmpty_EmptyAndValue) {
	RPC_CSTR p = m_str.get();
	rpc_string oth;

	m_str.swap(oth);

	EXPECT_NULL(m_str);
	EXPECT_EQ(p, oth);
}

TEST_F(rpc_stringTest, Swap_EmptyWithValue_ValueAndEmpty) {
	rpc_string str;
	RPC_CSTR p = m_other.get();

	str.swap(m_other);

	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_stringTest, Swap_ValueWithValue_ValueAndValue) {
	RPC_CSTR p = m_str.get();
	RPC_CSTR o = m_other.get();

	m_str.swap(m_other);

	EXPECT_EQ(o, m_str);
	EXPECT_EQ(p, m_other);
}


//
// hash
//

TEST_F(rpc_stringTest, Hash_Empty_ReturnHash) {
	rpc_string str;
	std::size_t h = str.hash();

	EXPECT_EQ(std::hash<void*>{}(str.get()), h);
}

TEST_F(rpc_stringTest, Hash_Value_ReturnHash) {
	std::size_t h = m_str.hash();

	EXPECT_EQ(std::hash<void*>{}(m_str.get()), h);
}


//
// operator==(const RpcString&, const RpcString&)
// operator!=(const RpcString&, const RpcString&)
//

TEST_F(rpc_stringTest, opEquals_EmptyAndEmpty_Equal) {
	rpc_string str;
	rpc_string oth;

	EXPECT_TRUE(str == oth);
	EXPECT_TRUE(oth == str);

	EXPECT_FALSE(str != oth);
	EXPECT_FALSE(oth != str);
}

TEST_F(rpc_stringTest, opEquals_EmptyAndValue_NotEqual) {
	rpc_string str;

	EXPECT_FALSE(str == m_other);
	EXPECT_FALSE(m_other == str);

	EXPECT_TRUE(str != m_other);
	EXPECT_TRUE(m_other != str);
}

TEST_F(rpc_stringTest, opEquals_ValueAndValue_NotEqual) {
	EXPECT_FALSE(m_str == m_other);
	EXPECT_FALSE(m_other == m_str);

	EXPECT_TRUE(m_str != m_other);
	EXPECT_TRUE(m_other != m_str);
}

// currently not possibly
// TEST(rpc_stringTest, opEquals_ValueAndSameValue_Equal);


//
// operator==(const RpcString&, U* p)
// operator==(U* p, RpcString&)
// operator!=(const RpcString&, U* p)
// operator!=(U* p, RpcString&)
//

TEST_F(rpc_stringTest, opEqualsPointer_EmptyAndNullptr_Equal) {
	rpc_string str;
	RPC_CSTR p = nullptr;

	EXPECT_TRUE(str == p);
	EXPECT_TRUE(p == str);

	EXPECT_FALSE(str != p);
	EXPECT_FALSE(p != str);
}

TEST_F(rpc_stringTest, opEqualsPointer_EmptyAndPointer_NotEqual) {
	rpc_string str;
	RPC_CSTR p = m_str.get();

	EXPECT_FALSE(str == p);
	EXPECT_FALSE(p == str);

	EXPECT_TRUE(str != p);
	EXPECT_TRUE(p != str);
}

TEST_F(rpc_stringTest, opEqualsPointer_ValueAndNullptr_NotEqual) {
	RPC_CSTR p = nullptr;

	EXPECT_FALSE(m_str == p);
	EXPECT_FALSE(p == m_str);

	EXPECT_TRUE(m_str != p);
	EXPECT_TRUE(p != m_str);
}

TEST_F(rpc_stringTest, opEqualsPointer_ValueAndPointer_NotEqual) {
	RPC_CSTR p = m_other.get();

	EXPECT_FALSE(m_str == p);
	EXPECT_FALSE(p == m_str);

	EXPECT_TRUE(m_str != p);
	EXPECT_TRUE(p != m_str);
}

TEST_F(rpc_stringTest, opEqualsPointer_ValueAndSamePointer_Equal) {
	RPC_CSTR p = m_str.get();

	EXPECT_TRUE(m_str == p);
	EXPECT_TRUE(p == m_str);

	EXPECT_FALSE(m_str != p);
	EXPECT_FALSE(p != m_str);
}


//
// operator==(const com_heap_ptr<T>&, nullptr_t)
// operator==(nullptr_t, const com_heap_ptr<T>&)
// operator!=(const com_heap_ptr<T>&, nullptr_t)
// operator!=(nullptr_t, com_heap_ptr com_ptr<T>&)
//

TEST_F(rpc_stringTest, opEqualsNullptr_Empty_Equal) {
	rpc_string str;

	EXPECT_TRUE(str == nullptr);
	EXPECT_TRUE(nullptr == str);

	EXPECT_FALSE(str != nullptr);
	EXPECT_FALSE(nullptr != str);
}

TEST_F(rpc_stringTest, opEqualsNullptr_Value_NoEqual) {
	EXPECT_FALSE(m_str == nullptr);
	EXPECT_FALSE(nullptr == m_str);

	EXPECT_TRUE(m_str != nullptr);
	EXPECT_TRUE(nullptr != m_str);
}


//
// std::swap
//

TEST_F(rpc_stringTest, stdSwap_EmptyWithEmpty_AreEmpty) {
	rpc_string str;
	rpc_string oth;

	std::swap(str, oth);

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_stringTest, stdSwap_ValueWithEmpty_EmptyAndValue) {
	RPC_CSTR p = m_str.get();
	rpc_string oth;

	std::swap(m_str, oth);

	EXPECT_NULL(m_str);
	EXPECT_EQ(p, oth);
}

TEST_F(rpc_stringTest, stdSwap_EmptyWithValue_ValueAndEmpty) {
	rpc_string str;
	RPC_CSTR p = m_other.get();

	std::swap(str, m_other);

	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_stringTest, stdSwap_ValueWithValue_ValueAndValue) {
	RPC_CSTR p = m_str.get();
	RPC_CSTR o = m_other.get();

	std::swap(m_str, m_other);

	EXPECT_EQ(o, m_str);
	EXPECT_EQ(p, m_other);
}


//
// std::hash
//

TEST_F(rpc_stringTest, stdHash_Empty_ReturnHash) {
	rpc_string str;
	std::size_t h = std::hash<rpc_string>{}(str);

	EXPECT_EQ(std::hash<void*>{}(str.get()), h);
}

TEST_F(rpc_stringTest, stdHash_Value_ReturnHash) {
	std::size_t h = std::hash<rpc_string>{}(m_str);

	EXPECT_EQ(std::hash<void*>{}(m_str.get()), h);
}


}  // namespace m3c::test
