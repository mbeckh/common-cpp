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

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <rpc.h>
#include <unknwn.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <string>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

class rpc_string_Test : public t::Test {
protected:
	void SetUp() override {
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

TEST_F(rpc_string_Test, ctor_Default_IsEmpty) {
	const rpc_string str;

	EXPECT_EQ(nullptr, str);
}


//
// RpcString(RpcString&&)
//

TEST_F(rpc_string_Test, ctorMove_WithEmpty_IsEmpty) {
	rpc_string oth;
	const rpc_string str(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_string_Test, ctorMove_WithValue_ValueIsMoved) {
	const RPC_CSTR p = m_other.get();
	const rpc_string str(std::move(m_other));

	EXPECT_NOT_NULL(str);
	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}


//
// RpcString(nullptr_t)
//

TEST_F(rpc_string_Test, ctorFromNullptr_WithNullptr_IsEmpty) {
	const rpc_string str(nullptr);

	EXPECT_NULL(str);
}


//
// operator=(unique_ptr&&)
//

TEST_F(rpc_string_Test, opMove_EmptyToEmpty_IsEmpty) {
	rpc_string str;
	rpc_string oth;

	str = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_string_Test, opMove_ValueToEmpty_ValueIsMoved) {
	rpc_string str;
	const RPC_CSTR p = m_other.get();

	str = std::move(m_other);

	EXPECT_NOT_NULL(str);
	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_string_Test, opMove_ValueToValue_ValueIsMoved) {
	const RPC_CSTR p = m_other.get();

	m_str = std::move(m_other);

	EXPECT_NOT_NULL(m_str);
	EXPECT_EQ(p, m_str);  // value must have changed
	EXPECT_NULL(m_other);
}


//
// operator=(nullptr_t)
//

TEST_F(rpc_string_Test, opAssign_NullptrToEmpty_IsEmpty) {
	rpc_string str;
	str = nullptr;

	EXPECT_NULL(str);
}

TEST_F(rpc_string_Test, opAssign_NullptrToValue_ValueIsCleared) {
	m_str = nullptr;

	EXPECT_NULL(m_str);
}


//
// operator&
//

TEST_F(rpc_string_Test, opAddressOf_Empty_ReturnAddress) {
	rpc_string str;
	RPC_CSTR* const pp = &str;

	EXPECT_NULL(str);
	EXPECT_NOT_NULL(pp);
}

TEST_F(rpc_string_Test, opAddressOf_Value_SetEmpty) {
	RPC_CSTR* const pp = &m_str;

	EXPECT_NULL(m_str);
	EXPECT_NOT_NULL(pp);
}

TEST_F(rpc_string_Test, opAddressOf_SetValue_HasValue) {
	RPC_CSTR* const pp = &m_str;
	ASSERT_NOT_NULL(pp);

	ASSERT_EQ(RPC_S_OK, UuidToStringA(&IID_IClassFactory, pp));

	EXPECT_NOT_NULL(*pp);
	EXPECT_EQ(*pp, m_str);
}


//
// (bool)
//

TEST_F(rpc_string_Test, opBool_Empty_ReturnFalse) {
	const rpc_string str;

	EXPECT_FALSE(str);
}

TEST_F(rpc_string_Test, opBool_Value_ReturnTrue) {
	EXPECT_TRUE(m_str);
}


//
// get
//

TEST_F(rpc_string_Test, get_Empty_ReturnNullptr) {
	const rpc_string str;
	const RPC_CSTR p = str.get();

	EXPECT_NULL(p);
}

TEST_F(rpc_string_Test, get_Value_ReturnPointer) {
	const RPC_CSTR p = m_str.get();

	EXPECT_NOT_NULL(p);
}


//
// c_str
//

TEST_F(rpc_string_Test, cstr_Empty_ReturnNullptr) {
	const rpc_string str;
	const char* const p = str.c_str();

	EXPECT_STREQ("", p);
}

TEST_F(rpc_string_Test, cstr_Value_ReturnPointer) {
	const char* const p = m_str.c_str();

	EXPECT_STREQ("00000001-0000-0000-c000-000000000046", p);
}


//
// size
//

TEST_F(rpc_string_Test, size_Empty_ReturnZero) {
	const rpc_string str;

	EXPECT_EQ(0, str.size());
}

TEST_F(rpc_string_Test, size_Value_ReturnLength) {
	EXPECT_EQ(36, m_str.size());
}


//
// release
//

TEST_F(rpc_string_Test, release_Empty_ReturnNullptr) {
	rpc_string str;
	const RPC_CSTR p = str.release();

	EXPECT_NULL(str);
	EXPECT_NULL(p);
}

TEST_F(rpc_string_Test, release_Value_ReturnPointer) {
	RPC_CSTR p = nullptr;
	{
		const RPC_CSTR addr = m_str.get();
		p = m_str.release();

		EXPECT_NULL(m_str);
		EXPECT_EQ(addr, p);
	}

	EXPECT_EQ(RPC_S_OK, RpcStringFreeA(&p));
}


//
// swap
//

TEST_F(rpc_string_Test, swap_EmptyWithEmpty_AreEmpty) {
	rpc_string str;
	rpc_string oth;

	str.swap(oth);

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_string_Test, swap_ValueWithEmpty_EmptyAndValue) {
	const RPC_CSTR p = m_str.get();
	rpc_string oth;

	m_str.swap(oth);

	EXPECT_NULL(m_str);
	EXPECT_EQ(p, oth);
}

TEST_F(rpc_string_Test, swap_EmptyWithValue_ValueAndEmpty) {
	rpc_string str;
	const RPC_CSTR p = m_other.get();

	str.swap(m_other);

	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_string_Test, swap_ValueWithValue_ValueAndValue) {
	const RPC_CSTR p = m_str.get();
	const RPC_CSTR o = m_other.get();

	m_str.swap(m_other);

	EXPECT_EQ(o, m_str);
	EXPECT_EQ(p, m_other);
}


//
// hash
//

TEST_F(rpc_string_Test, hash_Empty_ReturnHash) {
	const rpc_string str;
	const std::size_t h = str.hash();

	EXPECT_EQ(std::hash<void*>{}(str.get()), h);
}

TEST_F(rpc_string_Test, hash_Value_ReturnHash) {
	const std::size_t h = m_str.hash();

	EXPECT_EQ(std::hash<void*>{}(m_str.get()), h);
}


//
// operator==(const RpcString&, const RpcString&)
// operator!=(const RpcString&, const RpcString&)
//

TEST_F(rpc_string_Test, opEquals_EmptyAndEmpty_Equal) {
	const rpc_string str;
	const rpc_string oth;

	EXPECT_TRUE(str == oth);
	EXPECT_TRUE(oth == str);

	EXPECT_FALSE(str != oth);
	EXPECT_FALSE(oth != str);
}

TEST_F(rpc_string_Test, opEquals_EmptyAndValue_NotEqual) {
	const rpc_string str;

	EXPECT_FALSE(str == m_other);
	EXPECT_FALSE(m_other == str);

	EXPECT_TRUE(str != m_other);
	EXPECT_TRUE(m_other != str);
}

TEST_F(rpc_string_Test, opEquals_ValueAndValue_NotEqual) {
	EXPECT_FALSE(m_str == m_other);
	EXPECT_FALSE(m_other == m_str);

	EXPECT_TRUE(m_str != m_other);
	EXPECT_TRUE(m_other != m_str);
}

// currently not possibly
// TEST(rpc_string_Test, opEquals_ValueAndSameValue_Equal);


//
// operator==(const RpcString&, U* p)
// operator==(U* p, RpcString&)
// operator!=(const RpcString&, U* p)
// operator!=(U* p, RpcString&)
//

TEST_F(rpc_string_Test, opEqualsPointer_EmptyAndNullptr_Equal) {
	const rpc_string str;
	const RPC_CSTR p = nullptr;

	EXPECT_TRUE(str == p);
	EXPECT_TRUE(p == str);

	EXPECT_FALSE(str != p);
	EXPECT_FALSE(p != str);
}

TEST_F(rpc_string_Test, opEqualsPointer_EmptyAndPointer_NotEqual) {
	const rpc_string str;
	const RPC_CSTR p = m_str.get();

	EXPECT_FALSE(str == p);
	EXPECT_FALSE(p == str);

	EXPECT_TRUE(str != p);
	EXPECT_TRUE(p != str);
}

TEST_F(rpc_string_Test, opEqualsPointer_ValueAndNullptr_NotEqual) {
	const RPC_CSTR p = nullptr;

	EXPECT_FALSE(m_str == p);
	EXPECT_FALSE(p == m_str);

	EXPECT_TRUE(m_str != p);
	EXPECT_TRUE(p != m_str);
}

TEST_F(rpc_string_Test, opEqualsPointer_ValueAndPointer_NotEqual) {
	const RPC_CSTR p = m_other.get();

	EXPECT_FALSE(m_str == p);
	EXPECT_FALSE(p == m_str);

	EXPECT_TRUE(m_str != p);
	EXPECT_TRUE(p != m_str);
}

TEST_F(rpc_string_Test, opEqualsPointer_ValueAndSamePointer_Equal) {
	const RPC_CSTR p = m_str.get();

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

TEST_F(rpc_string_Test, opEqualsNullptr_Empty_Equal) {
	const rpc_string str;

	EXPECT_TRUE(str == nullptr);
	EXPECT_TRUE(nullptr == str);

	EXPECT_FALSE(str != nullptr);
	EXPECT_FALSE(nullptr != str);
}

TEST_F(rpc_string_Test, opEqualsNullptr_Value_NoEqual) {
	EXPECT_FALSE(m_str == nullptr);
	EXPECT_FALSE(nullptr == m_str);

	EXPECT_TRUE(m_str != nullptr);
	EXPECT_TRUE(nullptr != m_str);
}


//
// std::swap
//

TEST_F(rpc_string_Test, stdSwap_EmptyWithEmpty_AreEmpty) {
	rpc_string str;
	rpc_string oth;

	std::swap(str, oth);

	EXPECT_NULL(str);
	EXPECT_NULL(oth);
}

TEST_F(rpc_string_Test, stdSwap_ValueWithEmpty_EmptyAndValue) {
	const RPC_CSTR p = m_str.get();
	rpc_string oth;

	std::swap(m_str, oth);

	EXPECT_NULL(m_str);
	EXPECT_EQ(p, oth);
}

TEST_F(rpc_string_Test, stdSwap_EmptyWithValue_ValueAndEmpty) {
	rpc_string str;
	const RPC_CSTR p = m_other.get();

	std::swap(str, m_other);

	EXPECT_EQ(p, str);
	EXPECT_NULL(m_other);
}

TEST_F(rpc_string_Test, stdSwap_ValueWithValue_ValueAndValue) {
	const RPC_CSTR p = m_str.get();
	const RPC_CSTR o = m_other.get();

	std::swap(m_str, m_other);

	EXPECT_EQ(o, m_str);
	EXPECT_EQ(p, m_other);
}


//
// std::hash
//

TEST_F(rpc_string_Test, stdHash_Empty_ReturnHash) {
	const rpc_string str;
	const std::size_t h = std::hash<rpc_string>{}(str);

	EXPECT_EQ(std::hash<void*>{}(str.get()), h);
}

TEST_F(rpc_string_Test, stdHash_Value_ReturnHash) {
	const std::size_t h = std::hash<rpc_string>{}(m_str);

	EXPECT_EQ(std::hash<void*>{}(m_str.get()), h);
}


//
// Formatting
//

TEST_F(rpc_string_Test, format_Empty_PrintEmpty) {
	const rpc_string str;

	EXPECT_EQ("", fmt::to_string(str));
}

TEST_F(rpc_string_Test, format_EmptyCentered_PrintEmptyCentered) {
	const rpc_string str;

	EXPECT_EQ("  ", fmt::format("{:^2}", str));
}

TEST_F(rpc_string_Test, format_Value_PrintValue) {
	const std::string str = fmt::to_string(m_str);

	EXPECT_EQ("00000001-0000-0000-c000-000000000046", str);
}

TEST_F(rpc_string_Test, format_ValueW_PrintValue) {
	const std::wstring str = fmt::to_wstring(m_str);

	EXPECT_EQ(L"00000001-0000-0000-c000-000000000046", str);
}

TEST_F(rpc_string_Test, format_ValueCentered_PrintValueCentered) {
	const std::string str = fmt::format("{:^38}", m_str);

	EXPECT_EQ(" 00000001-0000-0000-c000-000000000046 ", str);
}


//
// Logging
//

TEST_F(rpc_string_Test, LogString_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	const rpc_string str;

	EXPECT_CALL(log, Debug(t::_, "\t"));

	Log::Info("{}{}", str, '\t');
}

TEST_F(rpc_string_Test, LogString_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	EXPECT_CALL(log, Debug(t::_, "00000001-0000-0000-c000-000000000046\t"));

	Log::Info("{}{}", m_str, '\t');
}

TEST_F(rpc_string_Test, LogEvent_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	const rpc_string str;

	EXPECT_CALL(log, Debug(t::_, "char[]=\t"));
	EXPECT_CALL(log, Event(evt::Test_LogString.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, 1, m4t::PointerAs<char>(t::StrEq(""))));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogString, str, '\t');
}

TEST_F(rpc_string_Test, LogEvent_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	EXPECT_CALL(log, Debug(t::_, "char[]=00000001-0000-0000-c000-000000000046\t"));
	EXPECT_CALL(log, Event(evt::Test_LogString.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, 37, m4t::PointerAs<char>(t::StrEq("00000001-0000-0000-c000-000000000046"))));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogString, m_str, '\t');
}

TEST_F(rpc_string_Test, LogException_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	try {
		const rpc_string str;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "char[]=\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogString.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, 1, m4t::PointerAs<char>(t::StrEq("")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogString << str << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST_F(rpc_string_Test, LogException_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);

	try {
		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, "char[]=00000001-0000-0000-c000-000000000046\t")).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(evt::Test_LogString.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, 37, m4t::PointerAs<char>(t::StrEq("00000001-0000-0000-c000-000000000046")))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogString << m_str << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
