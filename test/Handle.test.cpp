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

#include "m3c/Log.h"
#include "m3c/exception.h"

#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <detours_gmock.h>
#include <evntprov.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

#undef WIN32_FUNCTIONS
#define WIN32_FUNCTIONS(fn_)          \
	fn_(1, BOOL, WINAPI, CloseHandle, \
	    (HANDLE hObject),             \
	    (hObject),                    \
	    nullptr)

// NOLINTNEXTLINE(readability-identifier-naming): De-facto constexpr.
const HANDLE kInvalidHandleValue = INVALID_HANDLE_VALUE;  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast, performance-no-int-to-ptr): Constant definition uses cast.

class Handle_Test : public t::Test {
protected:
	void SetUp() override {
		ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hFoo, 0, FALSE, DUPLICATE_SAME_ACCESS));
		ASSERT_TRUE(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hOther, 0, FALSE, DUPLICATE_SAME_ACCESS));

		ON_CALL(m_win32, CloseHandle(m_hFoo))
		    .WillByDefault(t::Invoke([this](HANDLE hnd) {
			    ++m_fooClosed;
			    return m_win32.DTGM_Real_CloseHandle(hnd);
		    }));
		ON_CALL(m_win32, CloseHandle(m_hOther))
		    .WillByDefault(t::Invoke([this](HANDLE hnd) {
			    ++m_otherClosed;
			    return m_win32.DTGM_Real_CloseHandle(hnd);
		    }));
	}

	void TearDown() override {
		// close unused handles
		if (!m_fooUsed) {
			ASSERT_TRUE(CloseHandle(m_hFoo));
		}
		if (!m_otherUsed) {
			ASSERT_TRUE(CloseHandle(m_hOther));
		}

		ASSERT_EQ(1, m_fooClosed);
		ASSERT_EQ(1, m_otherClosed);
	}

	HANDLE UseFoo() {
		if (m_fooUsed) {
			throw std::logic_error("foo already used");
		}
		m_fooUsed = true;
		return m_hFoo;
	}

	HANDLE UseOther() {
		if (m_otherUsed) {
			throw std::logic_error("other already used");
		}
		m_otherUsed = true;
		return m_hOther;
	}

	HANDLE GetFoo() const {
		if (!m_fooUsed) {
			throw std::logic_error("foo not used");
		}
		return m_hFoo;
	}

	HANDLE GetOther() const {
		if (!m_otherUsed) {
			throw std::logic_error("other not used");
		}
		return m_hOther;
	}

	bool IsFooClosed() const {
		if (!m_fooUsed) {
			throw std::logic_error("foo not used");
		}
		return m_fooClosed > 0;
	}

	bool IsOtherClosed() const {
		if (!m_otherUsed) {
			throw std::logic_error("other not used");
		}
		return m_otherClosed > 0;
	}

protected:
	DTGM_API_MOCK(m_win32, WIN32_FUNCTIONS);
	m4t::LogListener m_log = m4t::LogListenerMode::kStrictAll;

private:
	HANDLE m_hFoo = kInvalidHandleValue;
	HANDLE m_hOther = kInvalidHandleValue;
	std::uint32_t m_fooClosed = 0;
	std::uint32_t m_otherClosed = 0;
	bool m_fooUsed = false;
	bool m_otherUsed = false;
};


//
// Handle()
//

TEST_F(Handle_Test, ctor_Default_IsInvalid) {
	Handle hnd;

	EXPECT_EQ(kInvalidHandleValue, hnd);
}


//
// Handle(Handle&&)
//

TEST_F(Handle_Test, ctorMove_FromDefault_IsInvalid) {
	Handle oth;
	Handle hnd(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
}

TEST_F(Handle_Test, ctorMove_FromValue_ValueIsMoved) {
	Handle oth(UseFoo());
	Handle hnd(std::move(oth));

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(GetFoo(), hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
	EXPECT_FALSE(IsFooClosed());
}


//
// Handle(HANDLE)
//

TEST_F(Handle_Test, ctorFromNative_FromInvalid_IsInvalid) {
	HANDLE h = kInvalidHandleValue;
	Handle hnd(h);

	EXPECT_EQ(kInvalidHandleValue, hnd);
}

TEST_F(Handle_Test, ctorFromNative_FromValue_HasValue) {
	Handle hnd(UseFoo());

	EXPECT_EQ(GetFoo(), hnd);
	EXPECT_FALSE(IsFooClosed());
}


//
// ~Handle()
//

TEST_F(Handle_Test, dtor_Value_CloseHandle) {
	{
		Handle hnd(UseFoo());
	}
	EXPECT_TRUE(IsFooClosed());
}

TEST_F(Handle_Test, dtor_ValueAndErrorClosing_Log) {
	EXPECT_CALL(m_log, Debug(DTGM_ARG2)).Times(t::AnyNumber());

	t::MockFunction<void(int)> check;
	{
		Handle hnd(UseFoo());

		t::InSequence s;
		EXPECT_CALL(check, Call(0));
		EXPECT_CALL(m_win32, CloseHandle(GetFoo()))
		    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE))
		    .RetiresOnSaturation();
		EXPECT_CALL(m_log, Event(evt::HandleLeak_E.Id, DTGM_ARG3));
		EXPECT_CALL(check, Call(1));

		check.Call(0);
	}
	check.Call(1);
	EXPECT_FALSE(IsFooClosed());

	// close handle manually
	CloseHandle(GetFoo());
	EXPECT_TRUE(IsFooClosed());
}


//
// operator=(Handle&&)
//

TEST_F(Handle_Test, opMove_DefaultToDefault_IsInvalid) {
	Handle hnd;
	Handle oth;

	hnd = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
}

TEST_F(Handle_Test, opMove_ValueToDefault_ValueIsMoved) {
	Handle hnd;
	Handle oth(UseFoo());

	hnd = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(GetFoo(), hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
	EXPECT_FALSE(IsFooClosed());
}

TEST_F(Handle_Test, opMove_ValueToValue_ValueIsMoved) {
	Handle hnd(UseFoo());
	Handle oth(UseOther());

	hnd = std::move(oth);

	m4t::EnableMovedFromCheck(oth);
	EXPECT_EQ(GetOther(), hnd);  // value must have changed
	EXPECT_EQ(kInvalidHandleValue, oth);
	EXPECT_TRUE(IsFooClosed());
	EXPECT_FALSE(IsOtherClosed());
}

TEST_F(Handle_Test, opMove_ValueToValueAndErrorClosing_LogAndValueIsMoved) {
	EXPECT_CALL(m_log, Debug(DTGM_ARG2)).Times(t::AnyNumber());

	t::MockFunction<void()> check;
	{
		Handle hnd(UseFoo());
		Handle oth(UseOther());

		t::InSequence s;
		EXPECT_CALL(m_win32, CloseHandle(GetFoo()))
		    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
		EXPECT_CALL(m_log, Event(evt::HandleLeak_E.Id, DTGM_ARG3));
		EXPECT_CALL(check, Call);

		hnd = std::move(oth);

		m4t::EnableMovedFromCheck(oth);
		EXPECT_EQ(GetOther(), hnd);  // value must have changed
		EXPECT_EQ(kInvalidHandleValue, oth);
		EXPECT_FALSE(IsFooClosed());
		EXPECT_FALSE(IsOtherClosed());
	}
	check.Call();

	// close handle manually after error
	CloseHandle(GetFoo());
}


//
// operator=(HANDLE)
//

TEST_F(Handle_Test, opAssign_InvalidToDefault_IsInvalid) {
	Handle hnd;
	hnd = kInvalidHandleValue;

	EXPECT_EQ(kInvalidHandleValue, hnd);
}

TEST_F(Handle_Test, opAssign_InvalidToValue_IsClosed) {
	Handle hnd(UseFoo());
	hnd = kInvalidHandleValue;

	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_TRUE(IsFooClosed());
}

TEST_F(Handle_Test, opAssign_ValueToValue_IsClosed) {
	Handle hnd(UseFoo());
	hnd = UseOther();

	EXPECT_EQ(GetOther(), hnd);
	EXPECT_TRUE(IsFooClosed());
	EXPECT_FALSE(IsOtherClosed());
}

TEST_F(Handle_Test, opAssign_ValueToValueAndErrorClosing_LogAndIsClosed) {
	EXPECT_CALL(m_log, Debug(DTGM_ARG2)).Times(t::AnyNumber());

	t::MockFunction<void()> check;
	{
		Handle hnd(UseFoo());

		t::InSequence s;
		EXPECT_CALL(m_win32, CloseHandle(GetFoo()))
		    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
		EXPECT_CALL(m_log, Event(evt::HandleLeak_E.Id, DTGM_ARG3));
		EXPECT_CALL(check, Call);

		hnd = UseOther();
		EXPECT_EQ(GetOther(), hnd);
		EXPECT_FALSE(IsFooClosed());
		EXPECT_FALSE(IsOtherClosed());
	}
	check.Call();

	// close handle manually
	CloseHandle(GetFoo());
}


//
// operator&()
//

TEST_F(Handle_Test, opAddressOf_Default_ReturnAddress) {
	Handle hnd;

	HANDLE* const ptr = &hnd;
	*ptr = UseFoo();

	EXPECT_EQ(GetFoo(), hnd);
}

TEST_F(Handle_Test, opAddressOf_Value_ReleaseAndReturnAddress) {
	Handle hnd(UseFoo());

	HANDLE* const ptr = &hnd;
	*ptr = UseOther();

	EXPECT_TRUE(IsFooClosed());
	EXPECT_EQ(GetOther(), hnd);
}


//
// (bool)
//

TEST_F(Handle_Test, opBool_Default_ReturnFalse) {
	Handle hnd;

	bool b = (bool) hnd;

	EXPECT_FALSE(b);
}

TEST_F(Handle_Test, opBool_Value_ReturnTrue) {
	Handle hnd(UseFoo());

	bool b = (bool) hnd;

	EXPECT_TRUE(b);
}


//
// (HANDLE)
//

TEST_F(Handle_Test, opHANDLE_Default_ReturnInvalid) {
	Handle hnd;

	HANDLE h = hnd;

	EXPECT_EQ(kInvalidHandleValue, h);
}

TEST_F(Handle_Test, opHANDLE_Value_ReturnNative) {
	Handle hnd(UseFoo());

	HANDLE h = hnd;

	EXPECT_EQ(GetFoo(), h);
}


//
// close
//

TEST_F(Handle_Test, close_Default_IsInvalid) {
	Handle hnd;

	hnd.close();

	EXPECT_EQ(kInvalidHandleValue, hnd);
}

TEST_F(Handle_Test, close_Value_IsClosedAndInvalid) {
	Handle hnd(UseFoo());

	hnd.close();

	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_TRUE(IsFooClosed());
}

TEST_F(Handle_Test, close_ValueAndErrorClosing_Throw) {
	t::MockFunction<void()> check;
	Handle hnd(UseFoo());

	t::InSequence s;
	EXPECT_CALL(m_win32, CloseHandle(GetFoo()))
	    .WillOnce(m4t::SetLastErrorAndReturn(ERROR_INTERNAL_ERROR, FALSE));
	EXPECT_CALL(check, Call);

	EXPECT_THAT(([&hnd]() {
		            hnd.close();
	            }),
	            (t::Throws<internal::ExceptionDetail<windows_error, const EVENT_DESCRIPTOR&>>(
	                t::AllOf(
	                    t::Property(&system_error::code, t::Property(&std::error_code::value, ERROR_INTERNAL_ERROR)),
	                    t::Property(&internal::BaseException<const EVENT_DESCRIPTOR&>::GetEvent, t::Field(&EVENT_DESCRIPTOR::Id, evt::HandleLeak_E.Id))))));

	EXPECT_EQ(GetFoo(), hnd);
	EXPECT_FALSE(IsFooClosed());
	check.Call();
}


//
// release
//

TEST_F(Handle_Test, release_Default_ReturnInvalid) {
	Handle hnd;
	HANDLE h = hnd.release();

	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_EQ(kInvalidHandleValue, h);
}

TEST_F(Handle_Test, release_Value_ReturnNative) {
	HANDLE h = kInvalidHandleValue;
	{
		Handle hnd(UseFoo());
		h = hnd.release();

		EXPECT_EQ(kInvalidHandleValue, hnd);
		EXPECT_EQ(GetFoo(), h);
	}

	EXPECT_FALSE(IsFooClosed());
	ASSERT_TRUE(CloseHandle(h));
	EXPECT_TRUE(IsFooClosed());
}

//
// swap
//

TEST_F(Handle_Test, swap_DefaultAndDefault_AreInvalid) {
	Handle hnd;
	Handle oth;

	hnd.swap(oth);

	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
}

TEST_F(Handle_Test, swap_ValueAndDefault_InvalidAndValue) {
	Handle hnd(UseFoo());
	Handle oth;

	hnd.swap(oth);

	EXPECT_EQ(kInvalidHandleValue, hnd);
	EXPECT_EQ(GetFoo(), oth);
	EXPECT_FALSE(IsFooClosed());
}

TEST_F(Handle_Test, swap_DefaultAndValue_ValueAndInvalid) {
	Handle hnd;
	Handle oth(UseOther());

	hnd.swap(oth);

	EXPECT_EQ(GetOther(), hnd);
	EXPECT_EQ(kInvalidHandleValue, oth);
	EXPECT_FALSE(IsOtherClosed());
}

TEST_F(Handle_Test, swap_ValueAndValue_ValueAndValue) {
	Handle hnd(UseFoo());
	Handle oth(UseOther());

	hnd.swap(oth);

	EXPECT_EQ(GetOther(), hnd);
	EXPECT_EQ(GetFoo(), oth);
	EXPECT_FALSE(IsFooClosed());
	EXPECT_FALSE(IsOtherClosed());
}

TEST_F(Handle_Test, swap_stdValueAndValue_ValueAndValue) {
	Handle hnd(UseFoo());
	Handle oth(UseOther());

	swap(hnd, oth);

	EXPECT_EQ(GetOther(), hnd);
	EXPECT_EQ(GetFoo(), oth);
	EXPECT_FALSE(IsFooClosed());
	EXPECT_FALSE(IsOtherClosed());
}


//
// hash
//

TEST_F(Handle_Test, hash_Default_ReturnHash) {
	Handle hnd;
	std::size_t h = hnd.hash();

	EXPECT_EQ(std::hash<HANDLE>{}(kInvalidHandleValue), h);
}

TEST_F(Handle_Test, hash_Value_ReturnHash) {
	Handle hnd(UseFoo());
	std::size_t h = hnd.hash();

	EXPECT_EQ(std::hash<HANDLE>{}(GetFoo()), h);
}

TEST_F(Handle_Test, hash_stdValue_ReturnHash) {
	Handle hnd(UseFoo());
	std::size_t h = std::hash<Handle>{}(hnd);

	EXPECT_EQ(std::hash<HANDLE>{}(GetFoo()), h);
}


//
// operator==(const Handle&, const Handle&)
// operator!=(const Handle&, const Handle&)
//

TEST_F(Handle_Test, opEquals_DefaultAndDefault_Equal) {
	Handle hnd;
	Handle oth;

	EXPECT_TRUE(hnd == oth);
	EXPECT_TRUE(oth == hnd);

	EXPECT_FALSE(hnd != oth);
	EXPECT_FALSE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_DefaultAndValue_NotEqual) {
	Handle hnd;
	Handle oth(UseFoo());

	EXPECT_FALSE(hnd == oth);
	EXPECT_FALSE(oth == hnd);

	EXPECT_TRUE(hnd != oth);
	EXPECT_TRUE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_ValueAndValueDifferent_NotEqual) {
	Handle hnd(UseFoo());
	Handle oth(UseOther());

	EXPECT_FALSE(hnd == oth);
	EXPECT_FALSE(oth == hnd);

	EXPECT_TRUE(hnd != oth);
	EXPECT_TRUE(oth != hnd);
}

TEST_F(Handle_Test, opEquals_ValueAndValueSame_Equal) {
	Handle hnd(UseFoo());
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

TEST_F(Handle_Test, opEqualsHANDLE_DefaultAndInvalid_Equal) {
	Handle hnd;
	HANDLE h = kInvalidHandleValue;

	EXPECT_TRUE(hnd == h);
	EXPECT_TRUE(h == hnd);

	EXPECT_FALSE(hnd != h);
	EXPECT_FALSE(h != hnd);
}

TEST_F(Handle_Test, DefaultAndNative_NotEqual) {
	Handle hnd;
	HANDLE h = UseOther();

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);

	// close native handle manually
	CloseHandle(h);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndInvalid_NotEqual) {
	Handle hnd(UseFoo());
	HANDLE h = kInvalidHandleValue;

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndNativeDifferent_NotEqual) {
	Handle hnd(UseFoo());
	HANDLE h = UseOther();

	EXPECT_FALSE(hnd == h);
	EXPECT_FALSE(h == hnd);

	EXPECT_TRUE(hnd != h);
	EXPECT_TRUE(h != hnd);

	// close native handle manually
	CloseHandle(h);
}

TEST_F(Handle_Test, opEqualsHANDLE_ValueAndNativeSame_Equal) {
	Handle hnd(UseFoo());
	HANDLE h = GetFoo();

	EXPECT_TRUE(hnd == h);
	EXPECT_TRUE(h == hnd);

	EXPECT_FALSE(hnd != h);
	EXPECT_FALSE(h != hnd);
}


//
// Formatting
//

TEST_F(Handle_Test, format_Empty_PrintInvalidHandle) {
	Handle arg;

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ(fmt::to_string(fmt::ptr(kInvalidHandleValue)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0xf+"));
}

TEST_F(Handle_Test, format_EmptyCentered_PrintInvalidHandleCentered) {
	Handle arg;

	const std::string str = FMT_FORMAT("{:^20}", arg);

	EXPECT_EQ(20, str.length());
	EXPECT_EQ(FMT_FORMAT("{:^20}", fmt::ptr(kInvalidHandleValue)), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+0xf+\\s+"));
}

TEST_F(Handle_Test, format_Value_PrintHandle) {
	Handle arg(UseFoo());

	const std::string str = fmt::to_string(arg);

	EXPECT_EQ(fmt::to_string(fmt::ptr(GetFoo())), str);
	EXPECT_THAT(str, m4t::MatchesRegex("0x[0-9a-f]+"));
}

TEST_F(Handle_Test, format_ValueW_PrintHandle) {
	Handle arg(UseFoo());

	const std::wstring str = fmt::to_wstring(arg);

	EXPECT_EQ(fmt::to_wstring(fmt::ptr(GetFoo())), str);
	EXPECT_THAT(str, m4t::MatchesRegex(L"0x[0-9a-f]+"));
}

TEST_F(Handle_Test, format_ValueCentered_PrintHandleCentered) {
	Handle arg(UseFoo());

	const std::string str = FMT_FORMAT("{:^20}", arg);

	EXPECT_EQ(20, str.length());
	EXPECT_EQ(FMT_FORMAT("{:^20}", fmt::ptr(GetFoo())), str);
	EXPECT_THAT(str, m4t::MatchesRegex("\\s+0x[0-9a-f]+\\s+"));
}


//
// Logging
//

TEST_F(Handle_Test, LogString_Empty_LogInvalidHandle) {
	Handle arg;

	EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("{}\t", fmt::ptr(kInvalidHandleValue))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(Handle_Test, LogString_Value_LogHandle) {
	Handle arg(UseFoo());

	EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("{}\t", fmt::ptr(GetFoo()))));

	Log::Info("{}{}", arg, '\t');
}

TEST_F(Handle_Test, LogEvent_Empty_LogInvalidHandle) {
	Handle arg;

	EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(kInvalidHandleValue))));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(kInvalidHandleValue)));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(Handle_Test, LogEvent_Value_LogHandle) {
	Handle arg(UseFoo());

	EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(GetFoo()))));
	EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2));
	EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(GetFoo())));
	EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(evt::Test_LogPtr, arg, '\t');
}

TEST_F(Handle_Test, LogException_Empty_LogInvalidHandle) {
	try {
		Handle arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(kInvalidHandleValue)))).InSequence(seqString);
		EXPECT_CALL(m_log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(kInvalidHandleValue))).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(m_log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TEST_F(Handle_Test, LogException_Value_LogHandle) {
	try {
		Handle arg(UseFoo());

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(m_log, Debug(t::_, FMT_FORMAT("ptr={}\t", fmt::ptr(GetFoo())))).InSequence(seqString);
		EXPECT_CALL(m_log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(m_log, Event(evt::Test_LogPtr.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(0, sizeof(void*), m4t::PointeeAs<void*>(GetFoo()))).InSequence(seqEvent);
		EXPECT_CALL(m_log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(m_log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + evt::Test_LogPtr << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
