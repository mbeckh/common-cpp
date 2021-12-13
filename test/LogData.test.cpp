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

#include "m3c/LogData.h"

#include "m3c/Log.h"
#include "m3c/finally.h"

#include <m4t/m4t.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <evntprov.h>
#include <oaidl.h>
#include <oleauto.h>
#include <propidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <rpc.h>
#include <wincodec.h>
#include <wtypes.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

static bool operator==(const FILETIME& lhs, const FILETIME& rhs) noexcept {
	return lhs.dwLowDateTime == rhs.dwLowDateTime && lhs.dwHighDateTime == rhs.dwHighDateTime;
}

static bool operator==(const SYSTEMTIME& lhs, const SYSTEMTIME& rhs) noexcept {
	return lhs.wYear == rhs.wYear && lhs.wMonth == rhs.wMonth && lhs.wDayOfWeek == rhs.wDayOfWeek && lhs.wDay == rhs.wDay
	       && lhs.wHour == rhs.wHour && lhs.wMinute == rhs.wMinute && lhs.wSecond == rhs.wSecond && lhs.wMilliseconds == rhs.wMilliseconds;
}

static bool operator==(const SID& lhs, const SID& rhs) noexcept {
	return lhs.Revision == rhs.Revision
	       && std::ranges::equal(std::span(lhs.IdentifierAuthority.Value), std::span(rhs.IdentifierAuthority.Value))
	       && lhs.SubAuthorityCount == rhs.SubAuthorityCount
	       && std::ranges::equal(std::span(lhs.SubAuthority, lhs.SubAuthorityCount), std::span(rhs.SubAuthority, rhs.SubAuthorityCount));
}

namespace m3c::test {
namespace {

namespace t = testing;

struct LongSID {
	SID sid;
	DWORD additionalSubAuthorities[1];
};

struct Tracking {
	mutable std::uint16_t created = 0;
	mutable std::uint16_t deleted = 0;
};


class CustomTypeTrivial {
public:
	using ValueType = std::uint16_t;

public:
	[[nodiscard]] explicit CustomTypeTrivial(const ValueType value) noexcept
	    : m_value(value) {
		// empty
	}
	[[nodiscard]] CustomTypeTrivial(const CustomTypeTrivial&) noexcept = default;
	[[nodiscard]] CustomTypeTrivial(CustomTypeTrivial&&) noexcept = default;
	~CustomTypeTrivial() noexcept = default;

public:
	void operator>>(_Inout_ LogEventArgs& eventArgs) const {
		eventArgs << m_value;
	}

public:
	[[nodiscard]] const ValueType& GetValue() const noexcept {
		return m_value;
	}

private:
	const ValueType m_value;
};

class CustomTypeTrivialEventData : public CustomTypeTrivial {
public:
	[[nodiscard]] explicit CustomTypeTrivialEventData(const ValueType value) noexcept
	    : CustomTypeTrivial(value) {
	}
	[[nodiscard]] CustomTypeTrivialEventData(const CustomTypeTrivialEventData&) noexcept = default;
	[[nodiscard]] CustomTypeTrivialEventData(CustomTypeTrivialEventData&&) noexcept = default;

public:
	void operator>>(_Inout_ LogEventArgs& args) const {
		args + FMT_FORMAT("CustomTrivialValue={}", GetValue());
	}
};

class CustomTypeMoveable : public CustomTypeTrivial {
public:
	[[nodiscard]] CustomTypeMoveable(const ValueType value, Tracking& tracking) noexcept
	    : CustomTypeTrivial(value)
	    , m_tracking(tracking) {
		++m_tracking.created;
	}
	[[nodiscard]] CustomTypeMoveable(const CustomTypeMoveable& oth) noexcept
	    : CustomTypeTrivial(oth)
	    , m_copies(oth.m_copies + 1)
	    , m_moves(oth.m_moves)
	    , m_tracking(oth.m_tracking) {
		++m_tracking.created;
	}
	[[nodiscard]] CustomTypeMoveable(CustomTypeMoveable&& oth) noexcept
	    : CustomTypeTrivial(std::move(oth))
	    , m_copies(oth.m_copies)
	    , m_moves(oth.m_moves + 1)
	    , m_tracking(oth.m_tracking) {
		++m_tracking.created;
	}
	virtual ~CustomTypeMoveable() noexcept {
		++m_tracking.deleted;
	};

public:
	[[nodiscard]] std::uint8_t GetCopies() const noexcept {
		return m_copies;
	}
	[[nodiscard]] std::uint8_t GetMoves() const noexcept {
		return m_moves;
	}
	[[nodiscard]] Tracking& GetTracking() noexcept {
		return m_tracking;
	}

private:
	const std::uint8_t m_copies = 0;
	const std::uint8_t m_moves = 0;
	Tracking& m_tracking;
};

class CustomTypeMoveableEventData : public CustomTypeMoveable {
public:
	[[nodiscard]] CustomTypeMoveableEventData(const ValueType value, Tracking& tracking) noexcept
	    : CustomTypeMoveable(value, tracking) {
	}
	[[nodiscard]] CustomTypeMoveableEventData(const CustomTypeMoveableEventData&) noexcept = default;
	[[nodiscard]] CustomTypeMoveableEventData(CustomTypeMoveableEventData&&) noexcept = default;

public:
	void operator>>(_Inout_ LogEventArgs& args) const {
		args + FMT_FORMAT("CustomMoveableValue={}", GetValue());
	}
};

class CustomTypeNonMoveable : public CustomTypeMoveable {
public:
	using CustomTypeMoveable::CustomTypeMoveable;
	[[nodiscard]] CustomTypeNonMoveable(const CustomTypeNonMoveable&) noexcept = default;
	CustomTypeNonMoveable(CustomTypeNonMoveable&&) = delete;
};

class CustomTypeThrowConstructible : public CustomTypeMoveable {
public:
	using CustomTypeMoveable::CustomTypeMoveable;
	[[nodiscard]] CustomTypeThrowConstructible(const CustomTypeThrowConstructible&) noexcept(false) = default;
	[[nodiscard]] CustomTypeThrowConstructible(CustomTypeThrowConstructible&&) noexcept(false) = default;
};

static_assert(std::is_trivially_copyable_v<CustomTypeTrivial>);
static_assert(std::is_trivially_copyable_v<CustomTypeTrivialEventData>);
static_assert(!std::is_trivially_copyable_v<CustomTypeMoveable>);
static_assert(!std::is_trivially_copyable_v<CustomTypeMoveableEventData>);
static_assert(!std::is_trivially_copyable_v<CustomTypeNonMoveable>);
static_assert(!std::is_trivially_copyable_v<CustomTypeThrowConstructible>);

static_assert(std::is_nothrow_copy_constructible_v<CustomTypeMoveable>);
static_assert(std::is_nothrow_copy_constructible_v<CustomTypeMoveableEventData>);
static_assert(std::is_nothrow_copy_constructible_v<CustomTypeNonMoveable>);
static_assert(!std::is_nothrow_copy_constructible_v<CustomTypeThrowConstructible>);

static_assert(std::is_nothrow_move_constructible_v<CustomTypeMoveable>);
static_assert(std::is_nothrow_move_constructible_v<CustomTypeMoveableEventData>);
static_assert(!std::is_nothrow_move_constructible_v<CustomTypeNonMoveable>);
static_assert(!std::is_nothrow_move_constructible_v<CustomTypeThrowConstructible>);

}  // namespace
}  // namespace m3c::test


template <>
struct fmt::formatter<m3c::test::CustomTypeTrivial> : public fmt::formatter<std::string> {
public:
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::test::CustomTypeTrivial& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(FMT_FORMAT("(CustomTypeTrivial: {})", arg.GetValue()), ctx);
	}
};

template <>
struct fmt::formatter<m3c::test::CustomTypeTrivialEventData> : public fmt::formatter<m3c::test::CustomTypeTrivial> {
};

template <>
struct fmt::formatter<m3c::test::CustomTypeMoveable> : public fmt::formatter<std::string> {
public:
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::test::CustomTypeMoveable& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(FMT_FORMAT("(CustomTypeMoveable: {}, copy #{}, move #{})", arg.GetValue(), arg.GetCopies(), arg.GetMoves()), ctx);
	}
};

template <>
struct fmt::formatter<m3c::test::CustomTypeMoveableEventData> : public fmt::formatter<m3c::test::CustomTypeMoveable> {
};

template <>
struct fmt::formatter<m3c::test::CustomTypeNonMoveable> : public fmt::formatter<std::string> {
public:
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::test::CustomTypeNonMoveable& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(FMT_FORMAT("(CustomTypeNonMoveable: {}, copy #{}, move #{})", arg.GetValue(), arg.GetCopies(), arg.GetMoves()), ctx);
	}
};

template <>
struct fmt::formatter<m3c::test::CustomTypeThrowConstructible> : public fmt::formatter<std::string> {
public:
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::test::CustomTypeThrowConstructible& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(FMT_FORMAT("(CustomTypeThrowConstructible: {}, copy #{}, move #{})", arg.GetValue(), arg.GetCopies(), arg.GetMoves()), ctx);
	}
};


namespace m3c::test {
namespace {

class LogData_Test : public t::Test {
protected:
	template <typename T>
	void TestMethod(const T& data, const std::string& result, const std::string& eventData = "") {
		TestMethod<T, false>(data, result, eventData);
	}

	template <typename T>
	void CopyMove(const T& data, const std::string& result, const std::string& eventData = "") {
		TestMethod<T, true>(data, result, eventData);
	}

private:
	template <typename T, bool kCopyMove>
	void TestMethod(const T& data, const std::string& result, const std::string& eventData) {
		LogData source;
		LogData moveAssign;
		Tracking tracking;

		std::reference_wrapper<LogData> ref(source);
		{
			T arg = data;
			if constexpr (kCopyMove) {
				// prepend one custom argument to prevent usage of std::memcpy in LogData
				source << CustomTypeMoveable(42, tracking);
			} else {
				// prepend a trivial argument
				source << 'a';
			}
			source << std::move(arg);
		}

		if constexpr (kCopyMove) {
			// first create a copy
			LogData copy(source);  // copy + 1

			// then move that copy to a new instance
			LogData move(std::move(copy));  // move + 1

			// assign the moved-to instance to the next
			LogData assign;
			assign = move;  // copy + 1

			// and finally move assign the value
			moveAssign = std::move(assign);  // move + 1

			// optimum is initial copy/move, copy + 2 and move + 2

			ref = moveAssign;
		}

		const LogData& logData = ref;

		{
			LogFormatArgs args;
			logData.CopyArgumentsTo(args);

			EXPECT_EQ(2, args.size());
			do {
				if constexpr (std::is_floating_point_v<T>) {
					if (result.empty()) {
						args << std::numeric_limits<T>::max_digits10;
						const std::string str = fmt::vformat("{1:.{2}g}", *args);

						char sz[1024];
						EXPECT_GT(sprintf_s(sz, "%.*g", std::numeric_limits<T>::max_digits10, data), 0);  // NOLINT(cppcoreguidelines-pro-type-vararg): Cross-check with old-style printf.
						EXPECT_EQ(sz, str);
						break;  // exit while loop
					}
				}
				const std::string str = fmt::vformat("{1}", *args);
				EXPECT_EQ(result, str);
			} while (false);
		}

		{
			LogEventArgs args;
			logData.CopyArgumentsTo(args);

			// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
			ASSERT_EQ(2, args.size());
			if constexpr (is_any_of_v<T, const char*, const wchar_t*>) {
				using CharType = std::remove_pointer_t<T>;
				const std::size_t length = std::char_traits<CharType>::length(data) + 1;  // including terminating null character
				ASSERT_EQ(length * sizeof(CharType), args[1].Size);
				EXPECT_EQ(data, std::basic_string_view(reinterpret_cast<T>(args[1].Ptr)));
			} else if constexpr (is_any_of_v<T, std::string, std::wstring, std::string_view, std::wstring_view>) {
				ASSERT_EQ((data.length() + 1) * sizeof(typename T::value_type), args[1].Size);  // including terminating null character
				EXPECT_EQ(data, reinterpret_cast<const typename T::value_type*>(args[1].Ptr));
			} else if constexpr (std::is_base_of_v<CustomTypeTrivial, T>) {
				if (eventData.empty()) {
					ASSERT_EQ(sizeof(CustomTypeTrivial::ValueType), args[1].Size);
					EXPECT_EQ(data.GetValue(), *reinterpret_cast<const CustomTypeTrivial::ValueType*>(args[1].Ptr));
				} else {
					ASSERT_EQ(eventData.size() + 1, args[1].Size);
					EXPECT_EQ(eventData, std::string_view(reinterpret_cast<const char*>(args[1].Ptr)));
				}
			} else {
				ASSERT_EQ(sizeof(T), args[1].Size);
				EXPECT_EQ(data, *reinterpret_cast<const T*>(args[1].Ptr));
			}
			// NOLINTEND(performance-no-int-to-ptr)
		}
	}
};


//
// No Arguments
//

TEST_F(LogData_Test, CopyArgumentsTo_NoArguments_ReturnEmpty) {
	LogData logData;

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(0, args.size());
		EXPECT_THAT(([&args]() { fmt::vformat("{}", *args); }),
		            t::Throws<fmt::format_error>());
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(0, args.size());
	}
}

TEST_F(LogData_Test, CopyMove_NoArguments_ReturnEmpty) {
	LogData source;
	LogData moveAssign;

	{
		// first create a copy
		LogData copy(source);  // copy + 1

		// then move that copy to a new instance
		LogData move(std::move(copy));  // move + 1

		// assign the moved-to instance to the next
		LogData assign;
		assign = move;  // copy + 1

		// and finally move assign the value
		moveAssign = std::move(assign);  // move + 1
	}

	// optimum is initial copy/move, copy + 2 and move + 2

	{
		LogFormatArgs args;
		moveAssign.CopyArgumentsTo(args);

		EXPECT_EQ(0, args.size());
		EXPECT_THAT(([&args]() { fmt::vformat("{}", *args); }),
		            t::Throws<fmt::format_error>());
	}

	{
		LogEventArgs args;
		moveAssign.CopyArgumentsTo(args);

		EXPECT_EQ(0, args.size());
	}
}

//
// bool
//

TEST_F(LogData_Test, CopyArgumentsTo_boolFalse_PrintFalse) {
	TestMethod<bool>(false, "false");
}

TEST_F(LogData_Test, CopyArgumentsTo_boolTrue_PrintTrue) {
	TestMethod<bool>(true, "true");
}

TEST_F(LogData_Test, CopyMove_boolTrue_PrintFalse) {
	CopyMove<bool>(true, "true");
}


//
// char
//

TEST_F(LogData_Test, CopyArgumentsTo_charLetter_PrintLetter) {
	TestMethod<char>('a', "a");
}

TEST_F(LogData_Test, CopyArgumentsTo_charNewline_PrintNewline) {
	TestMethod<char>('\n', "\n");
}

TEST_F(LogData_Test, CopyMove_charLetter_PrintLetter) {
	CopyMove<char>('a', "a");
}

//
// wchar_t
//

TEST_F(LogData_Test, CopyArgumentsTo_wcharLetter_PrintLetter) {
	TestMethod<wchar_t>(L'a', "a");
}

TEST_F(LogData_Test, CopyArgumentsTo_wcharNewline_PrintNewline) {
	TestMethod<wchar_t>(L'\n', "\n");
}

TEST_F(LogData_Test, CopyArgumentsTo_wcharHighAscii_PrintUtf8) {
	TestMethod<wchar_t>(L'\u00FC', "\xC3\xBC");
}

TEST_F(LogData_Test, CopyMove_wcharLetter_PrintLetter) {
	CopyMove<wchar_t>(L'a', "a");
}


//
// signed char
//

TEST_F(LogData_Test, CopyArgumentsTo_charSignedPositive_PrintPositive) {
	TestMethod<signed char>(64, "64");
}

TEST_F(LogData_Test, CopyArgumentsTo_charSignedNegative_PrintNegative) {
	TestMethod<signed char>(-64, "-64");
}

TEST_F(LogData_Test, CopyMove_charSignedPositive_PrintPositive) {
	CopyMove<signed char>(64, "64");
}


//
// unsigned char
//

TEST_F(LogData_Test, CopyArgumentsTo_charUnsignedPositive_PrintPositive) {
	TestMethod<unsigned char>(179u, "179");
}

TEST_F(LogData_Test, CopyMove_charUnsignedPositive_PrintPositive) {
	CopyMove<unsigned char>(179u, "179");
}


//
// signed short
//

TEST_F(LogData_Test, CopyArgumentsTo_shortSignedPositive_PrintPositive) {
	TestMethod<signed short>(2790, "2790");
}

TEST_F(LogData_Test, CopyArgumentsTo_shortSignedNegative_PrintNegative) {
	TestMethod<signed short>(-2790, "-2790");
}

TEST_F(LogData_Test, CopyMove_shortSignedPositive_PrintPositive) {
	CopyMove<signed short>(2790, "2790");
}


//
// unsigned short
//

TEST_F(LogData_Test, CopyArgumentsTo_shortUnsignedPositive_PrintPositive) {
	TestMethod<unsigned short>(37900u, "37900");
}

TEST_F(LogData_Test, CopyMove_shortUnsignedPositive_PrintPositive) {
	CopyMove<unsigned short>(37900u, "37900");
}


//
// signed int
//

TEST_F(LogData_Test, CopyArgumentsTo_intSignedPositive_PrintPositive) {
	TestMethod<signed int>(279000, "279000");
}

TEST_F(LogData_Test, CopyArgumentsTo_intSignedNegative_PrintNegative) {
	TestMethod<signed int>(-279000, "-279000");
}

TEST_F(LogData_Test, CopyMove_intSignedPositive_PrintPositive) {
	CopyMove<signed int>(279000, "279000");
}


//
// unsigned int
//

TEST_F(LogData_Test, CopyArgumentsTo_intUnsignedPositive_PrintPositive) {
	TestMethod<unsigned int>(379000u, "379000");
}

TEST_F(LogData_Test, CopyMove_intUnsignedPositive_PrintPositive) {
	CopyMove<unsigned int>(379000u, "379000");
}


//
// signed long
//

TEST_F(LogData_Test, CopyArgumentsTo_longSignedPositive_PrintPositive) {
	TestMethod<signed long>(279000L, "279000");
}

TEST_F(LogData_Test, CopyArgumentsTo_longSignedNegative_PrintNegative) {
	TestMethod<signed long>(-279000L, "-279000");
}

TEST_F(LogData_Test, CopyMove_longSignedPositive_PrintPositive) {
	CopyMove<signed long>(279000L, "279000");
}


//
// unsigned long
//

TEST_F(LogData_Test, CopyArgumentsTo_longUnsignedPositive_PrintPositive) {
	TestMethod<unsigned long>(3790000000uL, "3790000000");
}

TEST_F(LogData_Test, CopyMove_longUnsignedPositive_PrintPositive) {
	CopyMove<unsigned long>(3790000000uL, "3790000000");
}


//
// signed long long
//

TEST_F(LogData_Test, CopyArgumentsTo_longlongSignedPositive_PrintPositive) {
	TestMethod<signed long long>(379000000000LL, "379000000000");
}

TEST_F(LogData_Test, CopyArgumentsTo_longlongSignedNegative_PrintNegative) {
	TestMethod<signed long long>(-379000000000LL, "-379000000000");
}

TEST_F(LogData_Test, CopyMove_longlongSignedPositive_PrintPositive) {
	CopyMove<signed long long>(379000000000LL, "379000000000");
}


//
// unsigned long long
//

TEST_F(LogData_Test, CopyArgumentsTo_longlongUnsignedPositive_PrintPositive) {
	TestMethod<unsigned long long>(10790000000000000000uLL, "10790000000000000000");
}

TEST_F(LogData_Test, CopyMove_longlongUnsignedPositive_PrintPositive) {
	CopyMove<unsigned long long>(10790000000000000000uLL, "10790000000000000000");
}


//
// float
//

TEST_F(LogData_Test, CopyArgumentsTo_floatPositive_PrintFloat) {
	TestMethod<float>(8.8f, "8.8");
}

TEST_F(LogData_Test, CopyArgumentsTo_floatMin_PrintFloat) {
	TestMethod<float>(FLT_MIN, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_floatMax_PrintFloat) {
	TestMethod<float>(FLT_MAX, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_floatEpsilon_PrintFloat) {
	TestMethod<float>(FLT_EPSILON, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_floatTrueMin_PrintFloat) {
	TestMethod<float>(FLT_TRUE_MIN, "");
}

TEST_F(LogData_Test, CopyMove_floatPositive_PrintFloat) {
	CopyMove<float>(8.8f, "8.8");
}


//
// double
//

TEST_F(LogData_Test, CopyArgumentsTo_doublePositive_PrintFloat) {
	TestMethod<double>(8.8, "8.8");
}

TEST_F(LogData_Test, CopyArgumentsTo_doubleMin_PrintFloat) {
	TestMethod<double>(DBL_MIN, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_doubleMax_PrintFloat) {
	TestMethod<double>(DBL_MAX, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_doubleEpsilon_PrintFloat) {
	TestMethod<double>(DBL_EPSILON, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_doubleTrueMin_PrintFloat) {
	TestMethod<double>(DBL_TRUE_MIN, "");
}

TEST_F(LogData_Test, CopyMove_doublePositive_PrintFloat) {
	CopyMove<double>(8.8, "8.8");
}


//
// long double
//

TEST_F(LogData_Test, CopyArgumentsTo_longdoublePositive_PrintFloat) {
	TestMethod<long double>(8.8, "8.8");
}

TEST_F(LogData_Test, CopyArgumentsTo_longdoubleMin_PrintFloat) {
	TestMethod<long double>(LDBL_MIN, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_longdoubleMax_PrintFloat) {
	TestMethod<long double>(LDBL_MAX, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_longdoubleEpsilon_PrintFloat) {
	TestMethod<long double>(LDBL_EPSILON, "");
}

TEST_F(LogData_Test, CopyArgumentsTo_longdoubleTrueMin_PrintFloat) {
	TestMethod<long double>(LDBL_TRUE_MIN, "");
}

TEST_F(LogData_Test, CopyMove_longdoublePositive_PrintFloat) {
	CopyMove<long double>(8.8, "8.8");
}


//
// const void*
//

TEST_F(LogData_Test, CopyArgumentsTo_voidptrValue_PrintPointer) {
	TestMethod<const void*>(reinterpret_cast<const void*>(0x37906758), "0x37906758");
}

TEST_F(LogData_Test, CopyArgumentsTo_voidptrNullptr_PrintNullptr) {
	TestMethod<const void*>(nullptr, "0x0");
}

TEST_F(LogData_Test, CopyMove_voidptrValue_PrintPointer) {
	CopyMove<const void*>(reinterpret_cast<const void*>(0x37906758), "0x37906758");
}


//
// nullptr_t
//

TEST_F(LogData_Test, CopyArgumentsTo_nullptr_PrintNullptr) {
	TestMethod<std::nullptr_t>(nullptr, "0x0");
}

TEST_F(LogData_Test, CopyMove_nullptr_PrintNullptr) {
	CopyMove<std::nullptr_t>(nullptr, "0x0");
}


//
// const char*
//

TEST_F(LogData_Test, CopyArgumentsTo_charptrText_PrintText) {
	TestMethod<const char*>("Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_charptrEmpty_PrintEmpty) {
	TestMethod<const char*>("", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_charptrUtf8_PrintUtf8) {
	TestMethod<const char*>(reinterpret_cast<const char*>(u8"\u00FC"), "\xC3\xBC");
}

TEST_F(LogData_Test, CopyArgumentsTo_charptrControlChars_PrintControlChars) {
	TestMethod<const char*>("\\\n\r\t\b\f\v\a\u0002\u0019", "\\\n\r\t\b\f\v\a\u0002\u0019");
}


// Tests that string in buffer is still accessible after grow
TEST_F(LogData_Test, CopyArgumentsTo_charptrLiteralLong_GetValue) {
	LogData logData;
	{
		logData << "Test\nNext\\Line" << std::string(1024, 'x');
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} {:.3}", *args);
		EXPECT_EQ("Test\nNext\\Line xxx", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(15, args[0].Size);    // including terminating null character
		ASSERT_EQ(1025, args[1].Size);  // including terminating null character
		EXPECT_EQ("Test\nNext\\Line", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));
		EXPECT_EQ(std::string(1024, 'x'), std::string_view(reinterpret_cast<const char*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

TEST_F(LogData_Test, CopyMove_charptrText_PrintText) {
	CopyMove<const char*>("Test", "Test");
}


//
// const wchar_t*
//

TEST_F(LogData_Test, CopyArgumentsTo_wcharptrText_PrintText) {
	TestMethod<const wchar_t*>(L"Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_wcharptrEmpty_PrintEmpty) {
	TestMethod<const wchar_t*>(L"", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_wcharptrHighAscii_PrintUtf8) {
	TestMethod<const wchar_t*>(L"\u00FC", "\xC3\xBC");
}

TEST_F(LogData_Test, CopyArgumentsTo_wcharptrControlChars_PrintControlChars) {
	TestMethod<const wchar_t*>(L"\\\n\r\t\b\f\v\a\u0002\u0019", "\\\n\r\t\b\f\v\a\u0002\u0019");
}

// Tests that string in buffer is still accessible after grow
TEST_F(LogData_Test, CopyArgumentsTo_wcharptrLiteralLong_GetValue) {
	LogData logData;
	{
		logData << L"Test\nNext\\Line" << std::wstring(1024, L'x');
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} {:.3}", *args);
		EXPECT_EQ("Test\nNext\\Line xxx", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(30, args[0].Size);    // including terminating null character
		ASSERT_EQ(2050, args[1].Size);  // including terminating null character
		EXPECT_EQ(L"Test\nNext\\Line", std::wstring_view(reinterpret_cast<const wchar_t*>(args[0].Ptr)));
		EXPECT_EQ(std::wstring(1024, L'x'), std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

TEST_F(LogData_Test, CopyMove_wcharptrText_PrintText) {
	CopyMove<const wchar_t*>(L"Test", "Test");
}


//
// std::string
//

TEST_F(LogData_Test, CopyArgumentsTo_stringText_PrintText) {
	TestMethod<std::string>("Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_stringEmpty_PrintEmpty) {
	TestMethod<std::string>("", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_stringUtf8_PrintUtf8) {
	TestMethod<std::string>("\u00FC", "\xC3\xBC");
}

TEST_F(LogData_Test, CopyArgumentsTo_stringTemporary_GetValue) {
	LogData logData;
	{
		logData << std::string("Test");
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		const std::string str = fmt::vformat("{}", *args);
		EXPECT_EQ("Test", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(5, args[0].Size);                                                                                        // including terminating null character
		EXPECT_EQ("Test", std::string_view(reinterpret_cast<const char*>(args[0].Ptr), args[0].Size / sizeof(char) - 1));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyMove_stringText_PrintText) {
	CopyMove<std::string>("Test", "Test");
}


//
// std::wstring
//

TEST_F(LogData_Test, CopyArgumentsTo_wstringText_PrintText) {
	TestMethod<std::wstring>(L"Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_wstringEmpty_PrintEmpty) {
	TestMethod<std::wstring>(L"", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_wstringHightAscii_PrintUtf8) {
	TestMethod<std::wstring>(L"\u00FC", "\xC3\xBC");
}

TEST_F(LogData_Test, CopyArgumentsTo_wstringTemporary_GetValue) {
	LogData logData;
	{
		logData << std::wstring(L"Test");
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		const std::string str = fmt::vformat("{}", *args);
		EXPECT_EQ("Test", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(10, args[0].Size);                                                           // including terminating null character
		EXPECT_EQ(L"Test", std::wstring_view(reinterpret_cast<const wchar_t*>(args[0].Ptr)));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyMove_wstringText_PrintText) {
	CopyMove<std::wstring>(L"Test", "Test");
}


//
// std::string_view
//

TEST_F(LogData_Test, CopyArgumentsTo_stringviewText_PrintText) {
	TestMethod<std::string_view>("Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_stringviewEmpty_PrintEmpty) {
	TestMethod<std::string_view>("", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_stringviewUtf8_PrintUtf8) {
	TestMethod<std::string_view>("\u00C3\u00BC", "\u00C3\u00BC");
}

TEST_F(LogData_Test, CopyMove_stringviewText_PrintText) {
	CopyMove<std::string_view>("Test", "Test");
}


//
// std::wstring_view
//

TEST_F(LogData_Test, CopyArgumentsTo_wstringviewText_PrintText) {
	TestMethod<std::wstring_view>(L"Test", "Test");
}

TEST_F(LogData_Test, CopyArgumentsTo_wstringviewEmpty_PrintEmpty) {
	TestMethod<std::wstring_view>(L"", "");
}

TEST_F(LogData_Test, CopyArgumentsTo_wstringviewHightAscii_PrintUtf8) {
	TestMethod<std::wstring_view>(L"\u00FC", "\xC3\xBC");
}

TEST_F(LogData_Test, CopyMove_wstringviewText_PrintText) {
	CopyMove<std::wstring_view>(L"Test", "Test");
}


//
// GUID
//

TEST_F(LogData_Test, CopyArgumentsTo_GUID_PrintGUID) {
	TestMethod<GUID>({0xcbab4eac, 0xd957, 0x4fad, {0x8a, 0xc7, 0x60, 0x6a, 0x2b, 0xd1, 0x2d, 0x4f}}, "cbab4eac-d957-4fad-8ac7-606a2bd12d4f");
}

TEST_F(LogData_Test, CopyMove_GUID_PrintGUID) {
	CopyMove<GUID>({0xcbab4eac, 0xd957, 0x4fad, {0x8a, 0xc7, 0x60, 0x6a, 0x2b, 0xd1, 0x2d, 0x4f}}, "cbab4eac-d957-4fad-8ac7-606a2bd12d4f");
}


//
// FILETIME
//

TEST_F(LogData_Test, CopyArgumentsTo_FILETIME_PrintTime) {
	TestMethod<FILETIME>({.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456}, "2010-12-27T12:22:06.950Z");
}

TEST_F(LogData_Test, CopyMove_FILETIME_PrintTime) {
	CopyMove<FILETIME>({.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456}, "2010-12-27T12:22:06.950Z");
}


//
// SYSTEMTIME
//

TEST_F(LogData_Test, CopyArgumentsTo_SYSTEMTIME_PrintTime) {
	TestMethod<SYSTEMTIME>({.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379}, "2021-07-31T14:31:26.379Z");
}

TEST_F(LogData_Test, CopyMove_SYSTEMTIME_PrintTime) {
	CopyMove<SYSTEMTIME>({.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379}, "2021-07-31T14:31:26.379Z");
}


//
// SID
//
TEST_F(LogData_Test, CopyArgumentsTo_SID_PrintValue) {
	LogData logData;
	{
		const LongSID sid{.sid = {.Revision = SID_REVISION,
		                          .SubAuthorityCount = 2,
		                          .IdentifierAuthority = SECURITY_NT_AUTHORITY,
		                          .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
		                  .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};
		logData << sid.sid;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		const std::string str = fmt::vformat("{}", *args);
		EXPECT_EQ("S-1-5-32-547", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(1, args.size());
		ASSERT_EQ(sizeof(SID) + sizeof(DWORD), args[0].Size);
		ASSERT_EQ(sizeof(LongSID), args[0].Size);
		EXPECT_EQ(SID_REVISION, reinterpret_cast<const SID*>(args[0].Ptr)->Revision);
		ASSERT_EQ(2, reinterpret_cast<const SID*>(args[0].Ptr)->SubAuthorityCount);
		EXPECT_THAT(reinterpret_cast<const SID*>(args[0].Ptr)->IdentifierAuthority.Value, t::ElementsAreArray<BYTE>(SECURITY_NT_AUTHORITY));
		EXPECT_EQ(static_cast<DWORD>(SECURITY_BUILTIN_DOMAIN_RID), reinterpret_cast<const SID*>(args[0].Ptr)->SubAuthority[0]);
		EXPECT_EQ(static_cast<DWORD>(DOMAIN_ALIAS_RID_POWER_USERS), reinterpret_cast<const LongSID*>(args[0].Ptr)->additionalSubAuthorities[0]);
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

TEST_F(LogData_Test, CopyMove_SID_PrintValue) {
	Tracking tracking;
	LogData source;
	LogData moveAssign;
	{
		const LongSID sid{.sid = {.Revision = SID_REVISION,
		                          .SubAuthorityCount = 2,
		                          .IdentifierAuthority = SECURITY_NT_AUTHORITY,
		                          .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
		                  .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};
		// prepend custom type to prevent use of std::memcpy for copying
		source << CustomTypeMoveable(42, tracking) << sid.sid;
	}

	{
		// first create a copy
		LogData copy(source);  // copy + 1

		// then move that copy to a new instance
		LogData move(std::move(copy));  // move + 1

		// assign the moved-to instance to the next
		LogData assign;
		assign = move;  // copy + 1

		// and finally move assign the value
		moveAssign = std::move(assign);  // move + 1

		// optimum is initial copy/move, copy + 2 and move + 2
	}

	{
		LogFormatArgs args;
		moveAssign.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{1}", *args);
		EXPECT_EQ("S-1-5-32-547", str);
	}

	{
		LogEventArgs args;
		moveAssign.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(sizeof(SID) + sizeof(DWORD), args[1].Size);
		ASSERT_EQ(sizeof(LongSID), args[1].Size);
		EXPECT_EQ(SID_REVISION, reinterpret_cast<const SID*>(args[1].Ptr)->Revision);
		ASSERT_EQ(2, reinterpret_cast<const SID*>(args[1].Ptr)->SubAuthorityCount);
		EXPECT_THAT(reinterpret_cast<const SID*>(args[1].Ptr)->IdentifierAuthority.Value, t::ElementsAreArray<BYTE>(SECURITY_NT_AUTHORITY));
		EXPECT_EQ(static_cast<DWORD>(SECURITY_BUILTIN_DOMAIN_RID), reinterpret_cast<const SID*>(args[1].Ptr)->SubAuthority[0]);
		EXPECT_EQ(static_cast<DWORD>(DOMAIN_ALIAS_RID_POWER_USERS), reinterpret_cast<const LongSID*>(args[1].Ptr)->additionalSubAuthorities[0]);
		// NOLINTEND(performance-no-int-to-ptr)
	}
}


//
// Errors
//

TEST_F(LogData_Test, CopyArgumentsTo_win32error_PrintMessage) {
	LogData logData;
	{
		logData << win32_error(ERROR_ACCESS_DENIED);
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", *args);
			EXPECT_EQ("Access is denied. (5)", str);
		});
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(sizeof(DWORD), args[0].Size);
		EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), *reinterpret_cast<const DWORD*>(args[0].Ptr));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyArgumentsTo_rpcstatus_PrintMessage) {
	LogData logData;
	{
		logData << rpc_status(RPC_S_ALREADY_REGISTERED);
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", *args);
			EXPECT_EQ("The object universal unique identifier (UUID) has already been registered. (1711)", str);
		});
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(sizeof(RPC_STATUS), args[0].Size);
		EXPECT_EQ(static_cast<RPC_STATUS>(RPC_S_ALREADY_REGISTERED), *reinterpret_cast<const RPC_STATUS*>(args[0].Ptr));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyArgumentsTo_hresult_PrintMessage) {
	LogData logData;
	{
		logData << hresult(E_INVALIDARG);
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", *args);
			EXPECT_EQ("The parameter is incorrect. (0x80070057)", str);
		});
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(sizeof(HRESULT), args[0].Size);
		EXPECT_EQ(E_INVALIDARG, *reinterpret_cast<const HRESULT*>(args[0].Ptr));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyMove_win32error_PrintMessage) {
	Tracking tracking;
	LogData source;
	LogData moveAssign;
	{
		// prepend custom type to prevent use of std::memcpy for copying
		source << CustomTypeMoveable(42, tracking) << win32_error(ERROR_ACCESS_DENIED);
	}

	{
		// first create a copy
		LogData copy(source);  // copy + 1

		// then move that copy to a new instance
		LogData move(std::move(copy));  // move + 1

		// assign the moved-to instance to the next
		LogData assign;
		assign = move;  // copy + 1

		// and finally move assign the value
		moveAssign = std::move(assign);  // move + 1

		// optimum is initial copy/move, copy + 2 and move + 2
	}

	{
		LogFormatArgs args;
		moveAssign.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{1}", *args);
			EXPECT_EQ("Access is denied. (5)", str);
		});
	}

	{
		LogEventArgs args;
		moveAssign.CopyArgumentsTo(args);

		ASSERT_EQ(2, args.size());
		ASSERT_EQ(sizeof(DWORD), args[1].Size);
		EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), *reinterpret_cast<const DWORD*>(args[1].Ptr));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}


//
// Custom types
//

TEST_F(LogData_Test, CopyArgumentsTo_CustomTrivial_PrintData) {
	const CustomTypeTrivial arg(234);
	TestMethod<CustomTypeTrivial>(arg, "(CustomTypeTrivial: 234)");
}

TEST_F(LogData_Test, CopyArgumentsTo_CustomTrivialEventData_PrintData) {
	const CustomTypeTrivialEventData arg(234);
	TestMethod<CustomTypeTrivialEventData>(arg, "(CustomTypeTrivial: 234)", "CustomTrivialValue=234");
}

TEST_F(LogData_Test, CopyMove_CustomTrivial_PrintData) {
	const CustomTypeTrivial arg(234);
	CopyMove<CustomTypeTrivial>(arg, "(CustomTypeTrivial: 234)");
}

TEST_F(LogData_Test, CopyArgumentsTo_CustomMoveable_PrintData) {
	Tracking tracking;

	{
		const CustomTypeMoveable arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		TestMethod<CustomTypeMoveable>(arg, "(CustomTypeMoveable: 234, copy #1, move #1)");
	}
	EXPECT_EQ(3, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyArgumentsTo_CustomMoveableEventData_PrintData) {
	Tracking tracking;

	{
		const CustomTypeMoveableEventData arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		TestMethod<CustomTypeMoveableEventData>(arg, "(CustomTypeMoveable: 234, copy #1, move #1)", "CustomMoveableValue=234");
	}
	EXPECT_EQ(3, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyMove_CustomMoveable_PrintData) {
	Tracking tracking;

	{
		const CustomTypeMoveable arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		CopyMove<CustomTypeMoveable>(arg, "(CustomTypeMoveable: 234, copy #3, move #3)");
	}
	EXPECT_EQ(7, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyArgumentsTo_CustomNonMoveable_PrintData) {
	Tracking tracking;

	{
		const CustomTypeNonMoveable arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		TestMethod<CustomTypeNonMoveable>(arg, "(CustomTypeNonMoveable: 234, copy #2, move #0)");
	}
	EXPECT_EQ(3, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyMove_CustomNonMoveable_PrintData) {
	Tracking tracking;

	{
		const CustomTypeNonMoveable arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		CopyMove<CustomTypeNonMoveable>(arg, "(CustomTypeNonMoveable: 234, copy #6, move #0)");
	}
	EXPECT_EQ(7, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyArgumentsTo_CustomThrowConstructible_PrintData) {
	Tracking tracking;

	{
		const CustomTypeThrowConstructible arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		TestMethod<CustomTypeThrowConstructible>(arg, "(CustomTypeThrowConstructible: 234, copy #1, move #1)");
	}
	EXPECT_EQ(3, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}

TEST_F(LogData_Test, CopyMove_CustomThrowConstructible_PrintData) {
	Tracking tracking;

	{
		const CustomTypeThrowConstructible arg(234, tracking);
		EXPECT_EQ(1, tracking.created);
		CopyMove<CustomTypeThrowConstructible>(arg, "(CustomTypeThrowConstructible: 234, copy #1, move #1)");
	}
	EXPECT_EQ(3, tracking.created);
	EXPECT_EQ(tracking.created, tracking.deleted);
}


//
// Copy and move heap buffer
//

TEST_F(LogData_Test, CopyMove_HeapBuffer_IsSame) {
	Tracking moveableTracking;
	Tracking moveableEventDataTracking;
	Tracking nonMoveableTracking;
	Tracking throwConstructibleTracking;

	{
		LogData logData;
		LogData moveAssign;
		{
			const bool arg00 = true;
			const char arg01 = 'c';

			const signed char arg02 = -2;
			const unsigned char arg03 = 3;
			const signed short arg04 = -4;
			const unsigned short arg05 = 5;
			const signed int arg06 = -6;
			const unsigned int arg07 = 7;
			const signed long arg08 = -8;
			const unsigned long arg09 = 9;
			const signed long long arg10 = -10;
			const unsigned long long arg11 = 11;

			const float arg12 = 12.12f;
			const double arg13 = 13.13;

			const void* ptr14 = reinterpret_cast<const void*>(0x140014);
			const nullptr_t ptr15 = nullptr;

			const char* arg16 = "Test16";
			const wchar_t* arg17 = L"Test17";
			const std::string arg18(512, 'x');
			const std::wstring arg19(256, 'y');

			const GUID arg20{0xcbab4eac, 0xd957, 0x4fad, {0x8a, 0xc7, 0x60, 0x6a, 0x2b, 0xd1, 0x2d, 0x4f}};
			const FILETIME arg21{.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};
			const SYSTEMTIME arg22{.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};
			const LongSID arg23{.sid = {.Revision = SID_REVISION,
			                            .SubAuthorityCount = 2,
			                            .IdentifierAuthority = SECURITY_NT_AUTHORITY,
			                            .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
			                    .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

			auto arg24 = win32_error(ERROR_ACCESS_DENIED);
			auto arg25 = rpc_status(RPC_S_ALREADY_REGISTERED);
			auto arg26 = hresult(E_INVALIDARG);

			CustomTypeTrivial arg27(27);
			CustomTypeTrivialEventData arg28(28);
			CustomTypeMoveable arg29(29, moveableTracking);
			CustomTypeMoveableEventData arg30(30, moveableEventDataTracking);
			CustomTypeNonMoveable arg31(31, nonMoveableTracking);
			CustomTypeThrowConstructible arg32(32, throwConstructibleTracking);

			logData << arg00 << arg01 << arg02 << arg03 << arg04 << arg05
			        << arg06 << arg07 << arg08 << arg09 << arg10 << arg11
			        << arg12 << arg13
			        << ptr14 << ptr15
			        << arg16 << arg17 << arg18 << arg19
			        << arg20 << arg21 << arg22 << arg23.sid
			        << arg24 << arg25 << arg26
			        << arg27 << arg28 << arg29 << arg30 << arg31 << arg32;
		}

		{
			// first create a copy
			LogData copy(logData);

			// then move that copy to a new instance
			LogData move(std::move(copy));

			// assign the moved-to instance to the next
			LogData assign;
			assign = move;

			// and finally move assign the value
			moveAssign = std::move(assign);

			// optimum is initial copy/move, copy + 2 and move + 2
		}

		// now check that the result is still the same
		{
			LogEventArgs args;
			moveAssign.CopyArgumentsTo(args);

			// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
			ASSERT_EQ(33, args.size());

			ASSERT_EQ(sizeof(bool), args[0].Size);
			EXPECT_EQ(true, *reinterpret_cast<const bool*>(args[0].Ptr));

			ASSERT_EQ(sizeof(char), args[1].Size);
			EXPECT_EQ('c', *reinterpret_cast<const char*>(args[1].Ptr));

			ASSERT_EQ(sizeof(signed char), args[2].Size);
			EXPECT_EQ(-2, *reinterpret_cast<const signed char*>(args[2].Ptr));
			ASSERT_EQ(sizeof(unsigned char), args[3].Size);
			EXPECT_EQ(3, *reinterpret_cast<const unsigned char*>(args[3].Ptr));

			ASSERT_EQ(sizeof(signed short), args[4].Size);
			EXPECT_EQ(-4, *reinterpret_cast<const signed short*>(args[4].Ptr));
			ASSERT_EQ(sizeof(unsigned short), args[5].Size);
			EXPECT_EQ(5, *reinterpret_cast<const unsigned short*>(args[5].Ptr));

			ASSERT_EQ(sizeof(signed int), args[6].Size);
			EXPECT_EQ(-6, *reinterpret_cast<const signed int*>(args[6].Ptr));
			ASSERT_EQ(sizeof(unsigned int), args[7].Size);
			EXPECT_EQ(7, *reinterpret_cast<const unsigned int*>(args[7].Ptr));

			ASSERT_EQ(sizeof(signed long), args[8].Size);
			EXPECT_EQ(-8, *reinterpret_cast<const signed long*>(args[8].Ptr));
			ASSERT_EQ(sizeof(unsigned long), args[9].Size);
			EXPECT_EQ(9, *reinterpret_cast<const unsigned long*>(args[9].Ptr));

			ASSERT_EQ(sizeof(signed long long), args[10].Size);
			EXPECT_EQ(-10, *reinterpret_cast<const signed long long*>(args[10].Ptr));
			ASSERT_EQ(sizeof(unsigned long long), args[11].Size);
			EXPECT_EQ(11, *reinterpret_cast<const unsigned long long*>(args[11].Ptr));

			ASSERT_EQ(sizeof(float), args[12].Size);
			EXPECT_EQ(12.12f, *reinterpret_cast<const float*>(args[12].Ptr));
			ASSERT_EQ(sizeof(double), args[13].Size);
			EXPECT_EQ(13.13, *reinterpret_cast<const double*>(args[13].Ptr));

			ASSERT_EQ(sizeof(void*), args[14].Size);
			EXPECT_EQ(reinterpret_cast<const void*>(0x140014), *reinterpret_cast<const void**>(args[14].Ptr));
			ASSERT_EQ(sizeof(void*), args[15].Size);
			EXPECT_EQ(nullptr, *reinterpret_cast<const void**>(args[15].Ptr));

			ASSERT_EQ(7, args[16].Size);  // including terminating null character
			EXPECT_EQ("Test16", std::string_view(reinterpret_cast<const char*>(args[16].Ptr)));
			ASSERT_EQ(14, args[17].Size);  // including terminating null character
			EXPECT_EQ(L"Test17", std::wstring_view(reinterpret_cast<const wchar_t*>(args[17].Ptr)));
			ASSERT_EQ(513, args[18].Size);  // including terminating null character
			EXPECT_EQ(std::string(512, 'x'), std::string_view(reinterpret_cast<const char*>(args[18].Ptr)));
			ASSERT_EQ(514, args[19].Size);  // including terminating null character
			EXPECT_EQ(std::wstring(256, 'y'), std::wstring_view(reinterpret_cast<const wchar_t*>(args[19].Ptr)));

			const GUID guid{0xcbab4eac, 0xd957, 0x4fad, {0x8a, 0xc7, 0x60, 0x6a, 0x2b, 0xd1, 0x2d, 0x4f}};
			const FILETIME ft{.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456};
			const SYSTEMTIME st{.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379};
			const LongSID sid{.sid = {.Revision = SID_REVISION,
			                          .SubAuthorityCount = 2,
			                          .IdentifierAuthority = SECURITY_NT_AUTHORITY,
			                          .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
			                  .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};

			ASSERT_EQ(sizeof(GUID), args[20].Size);
			EXPECT_EQ(guid, *reinterpret_cast<const GUID*>(args[20].Ptr));
			ASSERT_EQ(sizeof(FILETIME), args[21].Size);
			EXPECT_EQ(ft, *reinterpret_cast<const FILETIME*>(args[21].Ptr));
			ASSERT_EQ(sizeof(SYSTEMTIME), args[22].Size);
			EXPECT_EQ(st, *reinterpret_cast<const SYSTEMTIME*>(args[22].Ptr));
			ASSERT_EQ(sizeof(SID) + sizeof(DWORD), args[23].Size);
			EXPECT_EQ(sid.sid, *reinterpret_cast<const SID*>(args[23].Ptr));

			ASSERT_EQ(sizeof(DWORD), args[24].Size);
			EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), *reinterpret_cast<const DWORD*>(args[24].Ptr));
			ASSERT_EQ(sizeof(RPC_STATUS), args[25].Size);
			EXPECT_EQ(RPC_S_ALREADY_REGISTERED, *reinterpret_cast<const RPC_STATUS*>(args[25].Ptr));
			ASSERT_EQ(sizeof(HRESULT), args[26].Size);
			EXPECT_EQ(E_INVALIDARG, *reinterpret_cast<const HRESULT*>(args[26].Ptr));

			ASSERT_EQ(sizeof(CustomTypeTrivial::ValueType), args[27].Size);
			EXPECT_EQ(27, *reinterpret_cast<const CustomTypeTrivial::ValueType*>(args[27].Ptr));
			constexpr std::string_view kTrivialEventData = "CustomTrivialValue=28";
			ASSERT_EQ(kTrivialEventData.size() + 1, args[28].Size);
			EXPECT_EQ(kTrivialEventData, std::string_view(reinterpret_cast<const char*>(args[28].Ptr)));

			ASSERT_EQ(sizeof(CustomTypeTrivial::ValueType), args[29].Size);
			EXPECT_EQ(29, *reinterpret_cast<const CustomTypeTrivial::ValueType*>(args[29].Ptr));
			constexpr std::string_view kMoveableEventData = "CustomMoveableValue=30";
			ASSERT_EQ(kMoveableEventData.size() + 1, args[30].Size);
			EXPECT_EQ(kMoveableEventData, std::string_view(reinterpret_cast<const char*>(args[30].Ptr)));

			ASSERT_EQ(sizeof(CustomTypeTrivial::ValueType), args[31].Size);
			EXPECT_EQ(31, *reinterpret_cast<const CustomTypeTrivial::ValueType*>(args[31].Ptr));

			ASSERT_EQ(sizeof(CustomTypeTrivial::ValueType), args[32].Size);
			EXPECT_EQ(32, *reinterpret_cast<const CustomTypeTrivial::ValueType*>(args[32].Ptr));
			// NOLINTEND(performance-no-int-to-ptr)
		}
	}
	EXPECT_EQ(moveableTracking.created, moveableTracking.deleted);
	EXPECT_EQ(moveableEventDataTracking.created, moveableEventDataTracking.deleted);
	EXPECT_EQ(nonMoveableTracking.created, nonMoveableTracking.deleted);
	EXPECT_EQ(throwConstructibleTracking.created, throwConstructibleTracking.deleted);
}


//
// VARIANT
//

TEST_F(LogData_Test, CopyArgumentsTo_VARIANT_PrintTypeAndValue) {
	LogData logData;
	{
		VARIANT var;
		ASSERT_HRESULT_SUCCEEDED(InitVariantFromInt32(42, &var));

		logData << var;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} - {}", *args);
		EXPECT_EQ("I4 - 42", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(3, args[0].Size);  // including terminating null character
		ASSERT_EQ(6, args[1].Size);  // including terminating null character
		EXPECT_EQ("I4", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));
		EXPECT_EQ(L"42", std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

TEST_F(LogData_Test, CopyArgumentsTo_VARIANTByRef_PrintTypeAndValue) {
	LogData logData;
	{
		int val = 42;
		VARIANT var;
		var.vt = VT_I4 | VT_BYREF;
		var.pintVal = &val;
		logData << var;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} - {}", *args);
		EXPECT_EQ("I4|BYREF - 42", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(9, args[0].Size);  // including terminating null character
		ASSERT_EQ(6, args[1].Size);  // including terminating null character
		EXPECT_EQ("I4|BYREF", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));
		EXPECT_EQ(L"42", std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

//
// PROPVARIANT
//

TEST_F(LogData_Test, CopyArgumentsTo_PROPVARIANT_PrintTypeAndValue) {
	LogData logData;
	{
		PROPVARIANT pv;
		ASSERT_HRESULT_SUCCEEDED(InitPropVariantFromStringAsVector(L"red;green;blue", &pv));

		logData << pv;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} - {}", *args);
		EXPECT_EQ("LPWSTR|VECTOR - red; green; blue", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(14, args[0].Size);  // including terminating null character
		ASSERT_EQ(34, args[1].Size);  // including terminating null character
		EXPECT_EQ("LPWSTR|VECTOR", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));
		EXPECT_EQ(L"red; green; blue", std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr): Must use existing API.
	}
}

TEST_F(LogData_Test, CopyArgumentsTo_PROPVARIANTByRef_PrintTypeAndValue) {
	LogData logData;
	{
		BSTR bstr = SysAllocString(L"Test");
		const auto release = finally([bstr]() noexcept {
			SysFreeString(bstr);
		});
		ASSERT_NOT_NULL(bstr);
		PROPVARIANT pv;
		pv.vt = VT_BSTR | VT_BYREF;
		pv.pbstrVal = &bstr;

		logData << pv;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(2, args.size());
		const std::string str = fmt::vformat("{} - {}", *args);
		EXPECT_EQ("BSTR|BYREF - Test", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(2, args.size());
		ASSERT_EQ(11, args[0].Size);  // including terminating null character
		ASSERT_EQ(10, args[1].Size);  // including terminating null character
		EXPECT_EQ("BSTR|BYREF", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));
		EXPECT_EQ(L"Test", std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr)));
		// NOLINTEND(performance-no-int-to-ptr): Must use existing API.
	}
}


//
// PROPERTYKEY
//

TEST_F(LogData_Test, CopyArgumentsTo_PROPERTYKEY_PrintName) {
	LogData logData;
	{
		const PROPERTYKEY& key = PKEY_Contact_BusinessAddress;

		logData << key;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(1, args.size());
		const std::string str = fmt::vformat("{}", *args);
		EXPECT_EQ("System.Contact.BusinessAddress", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1, args.size());
		ASSERT_EQ(31, args[0].Size);                                                                                // including terminating null character
		EXPECT_EQ("System.Contact.BusinessAddress", std::string_view(reinterpret_cast<const char*>(args[0].Ptr)));  // NOLINT(performance-no-int-to-ptr): Must use existing API.
	}
}


//
// WICRect
//

TEST_F(LogData_Test, CopyArgumentsTo_WICRect_PrintTopLeftWidthHeight) {
	LogData logData;
	{
		const WICRect rect = {10, 20, 320, 160};

		logData << rect;
	}

	{
		LogFormatArgs args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(4, args.size());
		const std::string str = fmt::vformat("{}/{} - {}/{}", *args);
		EXPECT_EQ("10/20 - 320/160", str);
	}

	{
		LogEventArgs args;
		logData.CopyArgumentsTo(args);

		// NOLINTBEGIN(performance-no-int-to-ptr): Must use existing API.
		ASSERT_EQ(4, args.size());
		ASSERT_EQ(sizeof(int), args[0].Size);
		ASSERT_EQ(sizeof(int), args[1].Size);
		ASSERT_EQ(sizeof(int), args[2].Size);
		ASSERT_EQ(sizeof(int), args[3].Size);
		EXPECT_EQ(10, *reinterpret_cast<const int*>(args[0].Ptr));
		EXPECT_EQ(20, *reinterpret_cast<const int*>(args[1].Ptr));
		EXPECT_EQ(320, *reinterpret_cast<const int*>(args[2].Ptr));
		EXPECT_EQ(160, *reinterpret_cast<const int*>(args[3].Ptr));
		// NOLINTEND(performance-no-int-to-ptr)
	}
}

}  // namespace
}  // namespace m3c::test
