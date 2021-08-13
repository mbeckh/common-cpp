/*
Copyright 2021 Michael Beckh

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

#include "m3c/Handle.h"

#include "testevents.h"

#include "m3c/exception.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <detours_gmock.h>

namespace m3c::test {

namespace t = testing;
namespace dtgm = detours_gmock;

#define WIN32_FUNCTIONS(fn_)                                                                                                                                                                         \
	fn_(8, ULONG, __stdcall, EventWriteEx,                                                                                                                                                           \
	    (REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor, ULONG64 Filter, ULONG Flags, LPCGUID ActivityId, LPCGUID RelatedActivityId, ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData), \
	    (RegHandle, EventDescriptor, Filter, Flags, ActivityId, RelatedActivityId, UserDataCount, UserData),                                                                                         \
	    nullptr);                                                                                                                                                                                    \
	fn_(1, BOOL, WINAPI, CloseHandle,                                                                                                                                                                \
	    (HANDLE hObject),                                                                                                                                                                            \
	    (hObject),                                                                                                                                                                                   \
	    nullptr);

DTGM_DECLARE_API_MOCK(Win32, WIN32_FUNCTIONS);

class Handle_Test : public t::Test {
protected:
	void SetUp() override {
		ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hFoo, 0, FALSE, DUPLICATE_SAME_ACCESS));
		ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hOther, 0, FALSE, DUPLICATE_SAME_ACCESS));

		ON_CALL(m_win32, CloseHandle(m_hFoo))
			.WillByDefault(t::Invoke([this](HANDLE hnd) {
				m_fooClosed = true;
				const BOOL result = (DTGM_REAL(Win32, CloseHandle))(hnd);
				if (result) {
					m_hFoo = INVALID_HANDLE_VALUE;
				}
				return result;
			}));
		ON_CALL(m_win32, CloseHandle(m_hOther))
			.WillByDefault(t::Invoke([this](HANDLE hnd) {
				m_otherClosed = true;
				const BOOL result = (DTGM_REAL(Win32, CloseHandle))(hnd);
				if (result) {
					m_hOther = INVALID_HANDLE_VALUE;
				}
				return result;
			}));
	}
	void TearDown() override {
		ASSERT_TRUE(CloseHandle(m_hFoo));
		ASSERT_TRUE(CloseHandle(m_hOther));

		ASSERT_TRUE(m_fooClosed);
		ASSERT_TRUE(m_otherClosed);
		DTGM_DETACH_API_MOCK(Win32);
	}

protected:
	HANDLE m_hFoo = INVALID_HANDLE_VALUE;
	HANDLE m_hOther = INVALID_HANDLE_VALUE;
	bool m_fooClosed = false;
	bool m_otherClosed = false;
	DTGM_DEFINE_API_MOCK(Win32, m_win32);
};

//
// Handle()
//

TEST_F(Handle_Test, ctor_Default_IsInvalid) {
	Handle hnd;

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
}


//
// Handle(Handle&&)
//

TEST_F(Handle_Test, ctorMove_WithInvalid_IsInvalid) {
	Handle oth;
	Handle hnd(std::move(oth));

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
}

TEST_F(Handle_Test, ctorMove_WithValue_ValueIsMoved) {
	Handle oth(m_hFoo);
	Handle hnd(std::move(oth));

	EXPECT_EQ(m_hFoo, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
	EXPECT_FALSE(m_fooClosed);
}


//
// Handle(HANDLE)
//

TEST_F(Handle_Test, ctorFromHandle_WithInvalid_IsInvalid) {
	HANDLE h = INVALID_HANDLE_VALUE;
	Handle hnd(h);

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
}

TEST_F(Handle_Test, ctorFromHandle_WithValue_HasValue) {
	Handle hnd(m_hFoo);

	EXPECT_EQ(m_hFoo, hnd);
	EXPECT_FALSE(m_fooClosed);
}


//
// ~Handle()
//

TEST_F(Handle_Test, dtor_Value_CloseHandle) {
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	{
		Handle hnd(m_hFoo);
	}
	EXPECT_TRUE(m_fooClosed);
}

TEST_F(Handle_Test, dtor_ValueAndErrorClosing_Log) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, CloseHandle(m_hFoo))
		.InSequence(seq)
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::CloseHandle.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	{
		Handle hnd(m_hFoo);
	}
	EXPECT_FALSE(m_fooClosed);
}


//
// operator=(Handle&&)
//

TEST_F(Handle_Test, opMove_InvalidToInvalid_IsInvalid) {
	Handle hnd;
	Handle oth;

	hnd = std::move(oth);

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
}

TEST_F(Handle_Test, opMove_ValueToInvalid_ValueIsMoved) {
	Handle hnd;
	Handle oth(m_hFoo);

	hnd = std::move(oth);

	EXPECT_EQ(m_hFoo, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
	EXPECT_FALSE(m_fooClosed);
}

TEST_F(Handle_Test, opMove_ValueToValue_ValueIsMoved) {
	Handle hnd(m_hFoo);
	Handle oth(m_hOther);

	hnd = std::move(oth);

	EXPECT_EQ(m_hOther, hnd);  // value must have changed
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
	EXPECT_TRUE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}

TEST_F(Handle_Test, opMove_ValueToValueAndErrorClosing_LogAndValueIsMoved) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, CloseHandle(m_hFoo))
		.InSequence(seq)
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::CloseHandle.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	Handle hnd(m_hFoo);
	Handle oth(m_hOther);

	hnd = std::move(oth);

	EXPECT_EQ(m_hOther, hnd);  // value must have changed
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
	EXPECT_FALSE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}


//
// operator=(HANDLE)
//

TEST_F(Handle_Test, opAssign_InvalidToInvalid_IsInvalid) {
	Handle hnd;
	hnd = INVALID_HANDLE_VALUE;

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
}

TEST_F(Handle_Test, opAssign_InvalidToValue_IsClosed) {
	Handle hnd(m_hFoo);
	hnd = INVALID_HANDLE_VALUE;

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_TRUE(m_fooClosed);
}

TEST_F(Handle_Test, opAssign_ValueToValue_IsClosed) {
	Handle hnd(m_hFoo);
	hnd = m_hOther;

	EXPECT_EQ(m_hOther, hnd);
	EXPECT_TRUE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}

TEST_F(Handle_Test, opAssign_ValueToValueAndErrorClosing_LogAndIsClosed) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, CloseHandle(m_hFoo))
		.InSequence(seq)
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::CloseHandle.Id), DTGM_ARG6))
		.Times(1)
		.InSequence(seq);

	Handle hnd(m_hFoo);
	hnd = m_hOther;

	EXPECT_EQ(m_hOther, hnd);
	EXPECT_FALSE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}

//
// (bool)
//

TEST_F(Handle_Test, opBool_Empty_ReturnFalse) {
	Handle hnd;

	bool b = (bool) hnd;

	EXPECT_FALSE(b);
}

TEST_F(Handle_Test, opBool_Value_ReturnTrue) {
	Handle hnd(m_hFoo);

	bool b = (bool) hnd;

	EXPECT_TRUE(b);
}


//
// (HANDLE)
//

TEST_F(Handle_Test, opHANDLE_Empty_ReturnInvalud) {
	Handle hnd;

	HANDLE h = hnd;

	EXPECT_EQ(INVALID_HANDLE_VALUE, h);
}

TEST_F(Handle_Test, opHANDLE_Value_ReturnHandle) {
	Handle hnd(m_hFoo);

	HANDLE h = hnd;

	EXPECT_EQ(m_hFoo, h);
}


//
// close
//

TEST_F(Handle_Test, close_Invalid_IsInvalid) {
	Handle hnd;

	hnd.close();

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
}

TEST_F(Handle_Test, close_Value_IsClosedAndInvalid) {
	Handle hnd(m_hFoo);

	hnd.close();

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_TRUE(m_fooClosed);
}

TEST_F(Handle_Test, close_ValueAndErrorClosing_Throw) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, CloseHandle(m_hFoo))
		.InSequence(seq)
		.WillOnce(dtgm::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE))
		.RetiresOnSaturation();
	EXPECT_CALL(m_win32, EventWriteEx(DTGM_ARG8)).Times(0);

	Handle hnd(m_hFoo);

	EXPECT_THAT(([&hnd]() {
					hnd.close();
				}),
	            t::Throws<internal::ExceptionDetail<windows_error>>(
					t::AllOf(
						t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_INTERNAL_ERROR)),
						t::Property(&internal::BaseException::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::CloseHandle.Id)))));

	EXPECT_EQ(m_hFoo, hnd);
	EXPECT_FALSE(m_fooClosed);
}


//
// release
//

TEST_F(Handle_Test, release_Invalid_ReturnInvalid) {
	Handle hnd;
	HANDLE h = hnd.release();

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, h);
}

TEST_F(Handle_Test, release_Value_ReturnHandle) {
	HANDLE h;
	{
		Handle hnd(m_hFoo);
		h = hnd.release();

		EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
		EXPECT_EQ(m_hFoo, h);
	}

	EXPECT_FALSE(m_fooClosed);
	ASSERT_TRUE(CloseHandle(h));
	EXPECT_TRUE(m_fooClosed);
}

//
// swap
//

TEST_F(Handle_Test, swap_InvalidWithInvalid_AreInvalid) {
	Handle hnd;
	Handle oth;

	hnd.swap(oth);

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
}

TEST_F(Handle_Test, swap_ValueWithInvalid_InvalidAndValue) {
	Handle hnd(m_hFoo);
	Handle oth;

	hnd.swap(oth);

	EXPECT_EQ(INVALID_HANDLE_VALUE, hnd);
	EXPECT_EQ(m_hFoo, oth);
	EXPECT_FALSE(m_fooClosed);
}

TEST_F(Handle_Test, swap_InvalidWithValue_ValueAndInvalid) {
	Handle hnd;
	Handle oth(m_hOther);

	hnd.swap(oth);

	EXPECT_EQ(m_hOther, hnd);
	EXPECT_EQ(INVALID_HANDLE_VALUE, oth);
	EXPECT_FALSE(m_otherClosed);
}

TEST_F(Handle_Test, swap_ValueWithValue_ValueAndValue) {
	Handle hnd(m_hFoo);
	Handle oth(m_hOther);

	hnd.swap(oth);

	EXPECT_EQ(m_hOther, hnd);
	EXPECT_EQ(m_hFoo, oth);
	EXPECT_FALSE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}

TEST_F(Handle_Test, swap_stdValueWithValue_ValueAndValue) {
	Handle hnd(m_hFoo);
	Handle oth(m_hOther);

	swap(hnd, oth);

	EXPECT_EQ(m_hOther, hnd);
	EXPECT_EQ(m_hFoo, oth);
	EXPECT_FALSE(m_fooClosed);
	EXPECT_FALSE(m_otherClosed);
}


//
// hash
//

TEST_F(Handle_Test, hash_Invalid_ReturnHash) {
	Handle hnd;
	std::size_t h = hnd.hash();

	EXPECT_EQ(std::hash<HANDLE>{}(INVALID_HANDLE_VALUE), h);
}

TEST_F(Handle_Test, hash_Value_ReturnHash) {
	Handle hnd(m_hFoo);
	std::size_t h = hnd.hash();

	EXPECT_EQ(std::hash<HANDLE>{}(m_hFoo), h);
}

TEST_F(Handle_Test, hash_stdValue_ReturnHash) {
	Handle hnd(m_hFoo);
	std::size_t h = std::hash<Handle>{}(hnd);

	EXPECT_EQ(std::hash<HANDLE>{}(m_hFoo), h);
}


//
// operator==(const Handle&, const Handle&)
// operator!=(const Handle&, const Handle&)
//

TEST_F(Handle_Test, opEquals_InvalidAndInvalid_Equal) {
	Handle hnd;
	Handle oth;

	EXPECT_TRUE(hnd == oth);
	EXPECT_TRUE(oth == hnd);

	EXPECT_FALSE(hnd != oth);
	EXPECT_FALSE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_InvalidAndValue_NotEqual) {
	Handle hnd;
	Handle oth(m_hFoo);

	EXPECT_FALSE(hnd == oth);
	EXPECT_FALSE(oth == hnd);

	EXPECT_TRUE(hnd != oth);
	EXPECT_TRUE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_ValueAndValue_NotEqual) {
	Handle hnd(m_hFoo);
	Handle oth(m_hOther);

	EXPECT_FALSE(hnd == oth);
	EXPECT_FALSE(oth == hnd);

	EXPECT_TRUE(hnd != oth);
	EXPECT_TRUE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_ValueAndSameValue_Equal) {
	Handle hnd(m_hFoo);
	Handle oth(hnd.get());

	EXPECT_TRUE(hnd == oth);
	EXPECT_TRUE(oth == hnd);

	EXPECT_FALSE(hnd != oth);
	EXPECT_FALSE(oth != hnd);

	// MUST release to prevent double free
	EXPECT_EQ(hnd.get(), oth.release());
}


//
// operator==(const Handle&, HANDLE h)
// operator==(HANDLE h, const Handle&)
// operator!=(const Handle&, HANDLE h)
// operator!=(HANDLE h, const Handle&)
//

TEST_F(Handle_Test, opEqualsHANDLE_InvalidAndInvalid_Equal) {
	Handle hnd;
	HANDLE h = INVALID_HANDLE_VALUE;

	EXPECT_TRUE(hnd == h);
	EXPECT_TRUE(h == hnd);

	EXPECT_FALSE(hnd != h);
	EXPECT_FALSE(h != hnd);
}

TEST_F(Handle_Test, opEqualsHANDLE_InvalidAndValue_NotEqual) {
	Handle hnd;
	HANDLE h = m_hOther;

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndInvalid_NotEqual) {
	Handle hnd(m_hFoo);
	HANDLE h = INVALID_HANDLE_VALUE;

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndHandle_NotEqual) {
	Handle hnd(m_hFoo);
	HANDLE h = m_hOther;

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndSameHandle_Equal) {
	Handle hnd(m_hFoo);
	HANDLE h = m_hFoo;

	EXPECT_TRUE(hnd == h);
	EXPECT_TRUE(h == hnd);

	EXPECT_FALSE(hnd != h);
	EXPECT_FALSE(h != hnd);
}

}  // namespace m3c::test
