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

#include "testevents.h"

#include <guiddef.h>

//#include "llamalog/custom_types.h"
//#include "llamalog/exception.h"
#include "m3c/finally.h"
#include "m3c/source_location.h"
#include "m3c/type_traits.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <m4t/m4t.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <span>
#include <string>

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

struct LongSID {
	SID sid;
	DWORD additionalSubAuthorities[1];
};

#if 0

class CustomTypeTrivial {
public:
	CustomTypeTrivial(int value) noexcept
		: m_value(value) {
		// empty
	}
	CustomTypeTrivial(const CustomTypeTrivial& oth) noexcept = default;
	CustomTypeTrivial(CustomTypeTrivial&& oth) noexcept = default;

	int GetValue() const noexcept {
		return m_value;
	}

private:
	int m_value;
};

class CustomTypeCopyOnly {
public:
	CustomTypeCopyOnly() noexcept
		: m_copies(0) {
		// empty
	}
	CustomTypeCopyOnly(const CustomTypeCopyOnly& oth) noexcept
		: m_copies(oth.m_copies + 1) {
		// empty
	}
	CustomTypeCopyOnly(CustomTypeCopyOnly&& oth) = delete;

	int GetCopies() const noexcept {
		return m_copies;
	}

private:
	const int m_copies;
};

class CustomTypeMove {
public:
	CustomTypeMove() noexcept
		: m_copies(0)
		, m_moves(0) {
		// empty
	}
	CustomTypeMove(const CustomTypeMove& oth) noexcept
		: m_copies(oth.m_copies + 1)
		, m_moves(oth.m_moves) {
		// empty
	}
	CustomTypeMove(CustomTypeMove&& oth) noexcept
		: m_copies(oth.m_copies)
		, m_moves(oth.m_moves + 1) {
		// empty
	}

	int GetCopies() const noexcept {
		return m_copies;
	}
	int GetMoves() const noexcept {
		return m_moves;
	}

private:
	const int m_copies;
	const int m_moves;
};
#endif
}  // namespace m3c::test
#if 0
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeTrivial& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeCopyOnly& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeMove& arg) {
	return logLine.AddCustomArgument(arg);
}

template <>
struct fmt::formatter<llamalog::test::CustomTypeTrivial> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.end();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeTrivial& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "({})", arg.GetValue());
	}
};

template <>
struct fmt::formatter<llamalog::test::CustomTypeCopyOnly> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.begin();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeCopyOnly& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "(copy #{})", arg.GetCopies());
	}
};

template <>
struct fmt::formatter<llamalog::test::CustomTypeMove> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.begin();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeMove& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "(copy #{} move #{})", arg.GetCopies(), arg.GetMoves());
	}
};
#endif

namespace m3c::test {

#if 0
template <typename T>
class LogDataTestX : public t::Test {
protected:
	static std::vector<std::tuple<T, std::string>> m_data;
};
#endif

class LogDataTest : public t::Test {
protected:
	template <typename T>
	void TestMethod(const T& data, const std::string& result) {
		internal::LogData logData;
		{
			const T arg = data;
			logData << data;
		}

		{
			fmt::dynamic_format_arg_store<fmt::format_context> args;
			logData.CopyArgumentsTo(args);

			if constexpr (std::is_floating_point_v<T>) {
				if (result.empty()) {
					args.push_back(std::numeric_limits<T>::max_digits10);
					const std::string str = fmt::vformat("{:.{}g}", args);

					char sz[1024];
					sprintf_s(sz, "%.*g", std::numeric_limits<T>::max_digits10, data);
					EXPECT_EQ(sz, str);
				} else {
					const std::string str = fmt::vformat("{}", args);
					EXPECT_EQ(result, str);
				}
			} else {
				const std::string str = fmt::vformat("{}", args);
				EXPECT_EQ(result, str);
			}
		}

		{
			std::vector<EVENT_DATA_DESCRIPTOR> args;
			logData.CopyArgumentsTo(args);

			ASSERT_EQ(1u, args.size());
			if constexpr (is_any_v<T, const char*, const wchar_t*>) {
				using CharType = std::remove_pointer_t<T>;
				const std::size_t length = std::char_traits<CharType>::length(data) + 1;  // including terminating null character
				ASSERT_EQ(length * sizeof(CharType), args[0].Size);
				EXPECT_EQ(data, std::basic_string_view(reinterpret_cast<T>(args[0].Ptr), length - 1));
			} else if constexpr (is_any_v<T, std::string, std::wstring, std::string_view, std::wstring_view>) {
				ASSERT_EQ((data.length() + 1) * sizeof(T::value_type), args[0].Size);  // including terminating null character
				EXPECT_EQ(data, reinterpret_cast<const T::value_type*>(args[0].Ptr));
			} else {
				ASSERT_EQ(sizeof(T), args[0].Size);
				EXPECT_EQ(data, *reinterpret_cast<const T*>(args[0].Ptr));
			}
		}
	}
};

#if 0
using LogDataTestTypes = t::Types<
	bool,
	char, wchar_t,
	signed char, unsigned char,
	signed short, unsigned short,
	signed int, unsigned int,
	signed long, unsigned long,
	signed long long, unsigned long long,
	float, double,
	const char*, const wchar_t*,
	const std::string, const std::wstring,
	const std::string_view, const std::wstring_view,
	const void*, std::nullptr_t,
	const GUID, const FILETIME, const SYSTEMTIME>;

#if 0
		//,                                  internal::win32_error, internal::rpc_status, internal::hresult
		> ;
#endif

#if 0
template <>
std::vector<std::tuple<internal::win32_error, std::string>> LogDataTest<internal::win32_error>::m_data{{make_win32_error(ERROR_ACCESS_DENIED), "0x0"}};
template <>
std::vector<std::tuple<internal::rpc_status, std::string>> LogDataTest<internal::rpc_status>::m_data{{make_rpc_status(RPC_S_ADDRESS_ERROR), "0x0"}};
template <>
std::vector<std::tuple<internal::hresult, std::string>> LogDataTest<internal::hresult>::m_data{{make_hresult(E_APPLICATION_EXITING), "0x0"}};
#endif
struct LogDataTestNames {
	template <typename T>
	static std::string GetName(int) noexcept {
		if constexpr (std::is_same_v<signed long long, T>) {
			return "long_long";
		} else if constexpr (std::is_same_v<unsigned long long, T>) {
			return "unsigned_long_long";
		} else if constexpr (std::is_same_v<std::nullptr_t, T>) {
			return "nullptr_t";
		} else if constexpr (std::is_same_v<const std::string, T>) {
			return "string";
		} else if constexpr (std::is_same_v<const std::wstring, T>) {
			return "wstring";
		} else if constexpr (std::is_same_v<const std::string_view, T>) {
			return "string_view";
		} else if constexpr (std::is_same_v<const std::wstring_view, T>) {
			return "wstring_view";
		} else if constexpr (std::is_same_v<const GUID, T>) {
			return "GUID";
		} else if constexpr (std::is_same_v<const FILETIME, T>) {
			return "FILETIME";
		} else if constexpr (std::is_same_v<const SYSTEMTIME, T>) {
			return "SYSTEMTIME";
		} else {
			std::string name = typeid(std::remove_pointer_t<std::remove_cvref_t<T>>).name();
			std::replace_if(name.begin(), name.end(), std::bind(std::equal_to(), std::placeholders::_1, ' '), '_');
			if constexpr (std::is_pointer_v<T>) {
				return name + "_ptr";
			} else {
				return name;
			}
		}
	}
};
TYPED_TEST_SUITE(LogDataTest, LogDataTestTypes, LogDataTestNames);
#endif

//
// No Arguments
//

TEST_F(LogDataTest, CopyArgumentsTo_NoArguments_ReturnEmpty) {
	internal::LogData logData;

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		EXPECT_THROW(fmt::vformat("{}", args), fmt::format_error);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		EXPECT_EQ(0u, args.size());
	}
}


//
// bool
//

TEST_F(LogDataTest, CopyArgumentsTo_boolFalse_PrintFalse) {
	TestMethod<bool>(false, "false");
}

TEST_F(LogDataTest, CopyArgumentsTo_boolTrue_PrintTrue) {
	TestMethod<bool>(true, "true");
}


//
// char
//

TEST_F(LogDataTest, CopyArgumentsTo_charLetter_PrintLetter) {
	TestMethod<char>('a', "a");
}

TEST_F(LogDataTest, CopyArgumentsTo_charNewline_PrintNewline) {
	TestMethod<char>('\n', "\n");
}


//
// wchar_t
//

TEST_F(LogDataTest, CopyArgumentsTo_wcharLetter_PrintLetter) {
	TestMethod<wchar_t>(L'a', "a");
}

TEST_F(LogDataTest, CopyArgumentsTo_wcharNewline_PrintNewline) {
	TestMethod<wchar_t>(L'\n', "\n");
}

TEST_F(LogDataTest, CopyArgumentsTo_wcharHighAscii_PrintUtf8) {
	TestMethod<wchar_t>(L'\u00FC', "\u00C3\u00BC");
}


//
// signed char
//

TEST_F(LogDataTest, CopyArgumentsTo_charSignedPositive_PrintPositive) {
	TestMethod<signed char>(64, "64");
}

TEST_F(LogDataTest, CopyArgumentsTo_charSignedNegative_PrintNegative) {
	TestMethod<signed char>(-64, "-64");
}


//
// unsigned char
//

TEST_F(LogDataTest, CopyArgumentsTo_charUnsignedPositive_PrintPositive) {
	TestMethod<unsigned char>(179u, "179");
}


//
// signed short
//

TEST_F(LogDataTest, CopyArgumentsTo_shortSignedPositive_PrintPositive) {
	TestMethod<signed short>(2790, "2790");
}

TEST_F(LogDataTest, CopyArgumentsTo_shortSignedNegative_PrintNegative) {
	TestMethod<signed short>(-2790, "-2790");
}


//
// unsigned short
//

TEST_F(LogDataTest, CopyArgumentsTo_shortUnsignedPositive_PrintPositive) {
	TestMethod<unsigned short>(37900u, "37900");
}


//
// signed int
//

TEST_F(LogDataTest, CopyArgumentsTo_intSignedPositive_PrintPositive) {
	TestMethod<signed int>(279000, "279000");
}

TEST_F(LogDataTest, CopyArgumentsTo_intSignedNegative_PrintNegative) {
	TestMethod<signed int>(-279000, "-279000");
}


//
// unsigned int
//

TEST_F(LogDataTest, CopyArgumentsTo_intUnsignedPositive_PrintPositive) {
	TestMethod<unsigned int>(379000u, "379000");
}


//
// signed long
//

TEST_F(LogDataTest, CopyArgumentsTo_longSignedPositive_PrintPositive) {
	TestMethod<signed long>(279000L, "279000");
}

TEST_F(LogDataTest, CopyArgumentsTo_longSignedNegative_PrintNegative) {
	TestMethod<signed long>(-279000L, "-279000");
}


//
// unsigned long
//

TEST_F(LogDataTest, CopyArgumentsTo_longUnsignedPositive_PrintPositive) {
	TestMethod<unsigned long>(3790000000uL, "3790000000");
}


//
// signed long long
//

TEST_F(LogDataTest, CopyArgumentsTo_longlongSignedPositive_PrintPositive) {
	TestMethod<signed long long>(379000000000LL, "379000000000");
}

TEST_F(LogDataTest, CopyArgumentsTo_longlongSignedNegative_PrintNegative) {
	TestMethod<signed long long>(-379000000000LL, "-379000000000");
}


//
// unsigned long long
//

TEST_F(LogDataTest, CopyArgumentsTo_longlongUnsignedPositive_PrintPositive) {
	TestMethod<unsigned long long>(10790000000000000000uLL, "10790000000000000000");
}


//
// float
//

TEST_F(LogDataTest, CopyArgumentsTo_floatPositive_PrintFloat) {
	TestMethod<float>(8.8f, "8.8");
}

TEST_F(LogDataTest, CopyArgumentsTo_floatMin_PrintFloat) {
	TestMethod<float>(FLT_MIN, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_floatMax_PrintFloat) {
	TestMethod<float>(FLT_MAX, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_floatEpsilon_PrintFloat) {
	TestMethod<float>(FLT_EPSILON, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_floatTrueMin_PrintFloat) {
	TestMethod<float>(FLT_TRUE_MIN, "");
}


//
// double
//

TEST_F(LogDataTest, CopyArgumentsTo_doublePositive_PrintFloat) {
	TestMethod<double>(8.8, "8.8");
}

TEST_F(LogDataTest, CopyArgumentsTo_doubleMin_PrintFloat) {
	TestMethod<double>(DBL_MIN, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_doubleMax_PrintFloat) {
	TestMethod<double>(DBL_MAX, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_doubleEpsilon_PrintFloat) {
	TestMethod<double>(DBL_EPSILON, "");
}

TEST_F(LogDataTest, CopyArgumentsTo_doubleTrueMin_PrintFloat) {
	TestMethod<double>(DBL_TRUE_MIN, "");
}


//
// const void*
//

TEST_F(LogDataTest, CopyArgumentsTo_voidptrValue_PrintPointer) {
	TestMethod<const void*>(reinterpret_cast<const void*>(0x37906758), "0x37906758");
}

TEST_F(LogDataTest, CopyArgumentsTo_voidptrNullptr_PrintNullptr) {
	TestMethod<const void*>(nullptr, "0x0");
}


//
// nullptr_t
//

TEST_F(LogDataTest, CopyArgumentsTo_nullptr_PrintNullptr) {
	TestMethod<std::nullptr_t>(nullptr, "0x0");
}


//
// const char*
//

TEST_F(LogDataTest, CopyArgumentsTo_charptrText_PrintText) {
	TestMethod<const char*>("Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_charptrEmpty_PrintEmpty) {
	TestMethod<const char*>("", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_charptrUtf8_PrintUtf8) {
	TestMethod<const char*>(reinterpret_cast<const char*>(u8"\u00FC"), "\u00C3\u00BC");
}

TEST_F(LogDataTest, CopyArgumentsTo_charptrControlChars_PrintControlChars) {
	TestMethod<const char*>("\\\n\r\t\b\f\v\a\u0002\u0019", "\\\n\r\t\b\f\v\a\u0002\u0019");
}


// Tests that string in buffer is still accessible after grow
TEST_F(LogDataTest, CopyArgumentsTo_charptrLiteralLong_GetValue) {
	internal::LogData logData;
	{
		logData << "Test\nNext\\Line" << std::string(1024, 'x');
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		const std::string str = fmt::vformat("{} {:.3}", args);
		EXPECT_EQ("Test\nNext\\Line xxx", str);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(2u, args.size());
		ASSERT_EQ(15u, args[0].Size);    // including terminating null character
		ASSERT_EQ(1025u, args[1].Size);  // including terminating null character
		EXPECT_EQ("Test\nNext\\Line", std::string_view(reinterpret_cast<const char*>(args[0].Ptr), args[0].Size / sizeof(char) - 1));
		EXPECT_EQ(std::string(1024, 'x'), std::string_view(reinterpret_cast<const char*>(args[1].Ptr), args[1].Size / sizeof(char) - 1));
	}
}


//
// const wchar_t*
//

TEST_F(LogDataTest, CopyArgumentsTo_wcharptrText_PrintText) {
	TestMethod<const wchar_t*>(L"Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_wcharptrEmpty_PrintEmpty) {
	TestMethod<const wchar_t*>(L"", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_wcharptrHighAscii_PrintUtf8) {
	TestMethod<const wchar_t*>(L"\u00FC", "\u00C3\u00BC");
}

TEST_F(LogDataTest, CopyArgumentsTo_wcharptrControlChars_PrintControlChars) {
	TestMethod<const wchar_t*>(L"\\\n\r\t\b\f\v\a\u0002\u0019", "\\\n\r\t\b\f\v\a\u0002\u0019");
}

// Tests that string in buffer is still accessible after grow
TEST_F(LogDataTest, CopyArgumentsTo_wcharptrLiteralLong_GetValue) {
	internal::LogData logData;
	{
		logData << L"Test\nNext\\Line" << std::wstring(1024, L'x');
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		const std::string str = fmt::vformat("{} {:.3}", args);
		EXPECT_EQ("Test\nNext\\Line xxx", str);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(2u, args.size());
		ASSERT_EQ(30u, args[0].Size);    // including terminating null character
		ASSERT_EQ(2050u, args[1].Size);  // including terminating null character
		EXPECT_EQ(L"Test\nNext\\Line", std::wstring_view(reinterpret_cast<const wchar_t*>(args[0].Ptr), args[0].Size / sizeof(wchar_t) - 1));
		EXPECT_EQ(std::wstring(1024, L'x'), std::wstring_view(reinterpret_cast<const wchar_t*>(args[1].Ptr), args[1].Size / sizeof(wchar_t) - 1));
	}
}


//
// std::string
//

TEST_F(LogDataTest, CopyArgumentsTo_stringText_PrintText) {
	TestMethod<std::string>("Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_stringEmpty_PrintEmpty) {
	TestMethod<std::string>("", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_stringUtf8_PrintUtf8) {
	TestMethod<std::string>("\u00C3\u00BC", "\u00C3\u00BC");
}

TEST_F(LogDataTest, CopyArgumentsTo_stringTemporary_GetValue) {
	internal::LogData logData;
	{
		logData << std::string("Test");
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		const std::string str = fmt::vformat("{}", args);
		EXPECT_EQ("Test", str);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(5u, args[0].Size);  // including terminating null character
		EXPECT_EQ("Test", std::string_view(reinterpret_cast<const char*>(args[0].Ptr), args[0].Size / sizeof(char) - 1));
	}
}


//
// std::wstring
//

TEST_F(LogDataTest, CopyArgumentsTo_wstringText_PrintText) {
	TestMethod<std::wstring>(L"Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_wstringEmpty_PrintEmpty) {
	TestMethod<std::wstring>(L"", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_wstringHightAscii_PrintUtf8) {
	TestMethod<std::wstring>(L"\u00FC", "\u00C3\u00BC");
}

TEST_F(LogDataTest, CopyArgumentsTo_wstringTemporary_GetValue) {
	internal::LogData logData;
	{
		logData << std::wstring(L"Test");
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		const std::string str = fmt::vformat("{}", args);
		EXPECT_EQ("Test", str);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(10u, args[0].Size);  // including terminating null character
		EXPECT_EQ(L"Test", std::wstring_view(reinterpret_cast<const wchar_t*>(args[0].Ptr), args[0].Size / sizeof(wchar_t) - 1));
	}
}


//
// std::string_view
//

TEST_F(LogDataTest, CopyArgumentsTo_stringviewText_PrintText) {
	TestMethod<std::string_view>("Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_stringviewEmpty_PrintEmpty) {
	TestMethod<std::string_view>("", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_stringviewUtf8_PrintUtf8) {
	TestMethod<std::string_view>("\u00C3\u00BC", "\u00C3\u00BC");
}


//
// std::wstring
//

TEST_F(LogDataTest, CopyArgumentsTo_wstringviewText_PrintText) {
	TestMethod<std::wstring_view>(L"Test", "Test");
}

TEST_F(LogDataTest, CopyArgumentsTo_wstringviewEmpty_PrintEmpty) {
	TestMethod<std::wstring_view>(L"", "");
}

TEST_F(LogDataTest, CopyArgumentsTo_wstringviewHightAscii_PrintUtf8) {
	TestMethod<std::wstring_view>(L"\u00FC", "\u00C3\u00BC");
}


//
// GUID
//

TEST_F(LogDataTest, CopyArgumentsTo_GUID_PrintGUID) {
	TestMethod<GUID>({0xcbab4eac, 0xd957, 0x4fad, {0x8a, 0xc7, 0x60, 0x6a, 0x2b, 0xd1, 0x2d, 0x4f}}, "cbab4eac-d957-4fad-8ac7-606a2bd12d4f");
}


//
// FILETIME
//

TEST_F(LogDataTest, CopyArgumentsTo_FILETIME_PrintTime) {
	TestMethod<FILETIME>({.dwLowDateTime = 2907012345, .dwHighDateTime = 30123456}, "2010-12-27T12:22:06.950Z");
}


//
// SYSTEMTIME
//

TEST_F(LogDataTest, CopyArgumentsTo_SYSTEMTIME_PrintTime) {
	TestMethod<SYSTEMTIME>({.wYear = 2021, .wMonth = 7, .wDayOfWeek = 6, .wDay = 31, .wHour = 14, .wMinute = 31, .wSecond = 26, .wMilliseconds = 379}, "2021-07-31T14:31:26.379Z");
}


//
// SID
//
TEST_F(LogDataTest, CopyArgumentsTo_SID_GetValue) {
	internal::LogData logData;
	{
		const LongSID sid{.sid = {.Revision = SID_REVISION,
		                          .SubAuthorityCount = 2,
		                          .IdentifierAuthority = SECURITY_NT_AUTHORITY,
		                          .SubAuthority = {SECURITY_BUILTIN_DOMAIN_RID}},
		                  .additionalSubAuthorities = {DOMAIN_ALIAS_RID_POWER_USERS}};
		logData << sid.sid;
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		const std::string str = fmt::vformat("{}", args);
		EXPECT_EQ("S-1-5-32-547", str);
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(sizeof(SID) + sizeof(DWORD), args[0].Size);
		EXPECT_EQ(SID_REVISION, reinterpret_cast<const SID*>(args[0].Ptr)->Revision);
		ASSERT_EQ(2, reinterpret_cast<const SID*>(args[0].Ptr)->SubAuthorityCount);
		EXPECT_THAT(reinterpret_cast<const SID*>(args[0].Ptr)->IdentifierAuthority.Value, t::ElementsAreArray<BYTE>(SECURITY_NT_AUTHORITY));
		EXPECT_EQ(static_cast<DWORD>(SECURITY_BUILTIN_DOMAIN_RID), reinterpret_cast<const SID*>(args[0].Ptr)->SubAuthority[0]);
		EXPECT_EQ(static_cast<DWORD>(DOMAIN_ALIAS_RID_POWER_USERS), reinterpret_cast<const SID*>(args[0].Ptr)->SubAuthority[1]);
	}
}

#if 0
//
// Generic Value Types
//

TYPED_TEST(LogDataTestX, CopyArgumentsTo_SingleArgument_GetValue) {
	for (const std::tuple<TypeParam, std::string>& data : LogDataTest<TypeParam>::m_data) {
		internal::LogData logData;
		{
			const TypeParam arg = std::get<0>(data);
			logData << arg;
		}

		{
			fmt::dynamic_format_arg_store<fmt::format_context> args;
			logData.CopyArgumentsTo(args);

			if constexpr (std::is_floating_point_v<TypeParam>) {
				if (std::get<1>(data).empty()) {
					args.push_back(std::numeric_limits<TypeParam>::max_digits10);
					const std::string str = fmt::vformat("{:.{}g}", args);

					char sz[1024];
					sprintf_s(sz, "%.*g", std::numeric_limits<TypeParam>::max_digits10, std::get<0>(data));
					EXPECT_EQ(sz, str);
				} else {
					const std::string str = fmt::vformat("{}", args);
					EXPECT_EQ(std::get<1>(data), str);
				}
			} else {
				const std::string str = fmt::vformat("{}", args);
				EXPECT_EQ(std::get<1>(data), str);
			}
		}

		{
			std::vector<EVENT_DATA_DESCRIPTOR> args;
			logData.CopyArgumentsTo(args);

			ASSERT_EQ(1u, args.size());
			if constexpr (is_any_v<TypeParam, const char*, const wchar_t*>) {
				using CharType = std::remove_pointer_t<TypeParam>;
				const std::size_t length = std::char_traits<CharType>::length(std::get<0>(data));
				ASSERT_EQ(length * sizeof(CharType), args[0].Size);
				EXPECT_EQ(std::get<0>(data), std::basic_string_view(reinterpret_cast<TypeParam>(args[0].Ptr), length));
			} else if constexpr (is_any_v<TypeParam, const std::string, const std::wstring, const std::string_view, const std::wstring_view>) {
				ASSERT_EQ(std::get<0>(data).length() * sizeof(TypeParam::value_type), args[0].Size);
				EXPECT_EQ(std::get<0>(data), reinterpret_cast<const TypeParam::value_type*>(args[0].Ptr));
			} else {
				ASSERT_EQ(sizeof(TypeParam), args[0].Size);
				EXPECT_EQ(std::get<0>(data), *reinterpret_cast<const TypeParam*>(args[0].Ptr));
			}
		}
	}
}

#if 0

TEST(LogLine_Test, wcharptr_IsLongValueAfterConversion_PrintUtf8) {
	LogLine logLine = GetLogLine("{:.5}");
	{
		std::wstring arg(256, L'x');
		arg[0] = L'\xE4';
		logLine << arg.c_str() << escape(arg.c_str());
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\xC3\xA4xxx", str);
}
#endif


#if 0
//
// std::align_val_t
//

TEST(LogLine_Test, stdalignvalt_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::align_val_t arg = static_cast<std::align_val_t>(4096);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("4096 4096", str);
}

#endif

#endif

//
// Errors
//

TEST_F(LogDataTest, CopyArgumentsTo_win32error_GetValue) {
	internal::LogData logData;
	{
		logData << make_win32_error(ERROR_ACCESS_DENIED);
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", args);
			EXPECT_EQ("Access is denied. (5)", str);
		});
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(sizeof(DWORD), args[0].Size);
		EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), *reinterpret_cast<const DWORD*>(args[0].Ptr));
	}
}

TEST_F(LogDataTest, CopyArgumentsTo_rpcstatus_GetValue) {
	internal::LogData logData;
	{
		logData << make_rpc_status(RPC_S_ALREADY_REGISTERED);
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", args);
			EXPECT_EQ("The object universal unique identifier (UUID) has already been registered. (1711)", str);
		});
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(sizeof(RPC_STATUS), args[0].Size);
		EXPECT_EQ(static_cast<RPC_STATUS>(RPC_S_ALREADY_REGISTERED), *reinterpret_cast<const RPC_STATUS*>(args[0].Ptr));
	}
}

TEST_F(LogDataTest, CopyArgumentsTo_hresult_GetValue) {
	internal::LogData logData;
	{
		logData << make_hresult(E_INVALIDARG);
	}

	{
		fmt::dynamic_format_arg_store<fmt::format_context> args;
		logData.CopyArgumentsTo(args);

		m4t::WithLocale("en-US", [&args] {
			const std::string str = fmt::vformat("{}", args);
			EXPECT_EQ("The parameter is incorrect. (0x80070057)", str);
		});
	}

	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(sizeof(HRESULT), args[0].Size);
		EXPECT_EQ(E_INVALIDARG, *reinterpret_cast<const HRESULT*>(args[0].Ptr));
	}
}

//
// Copy and Move
//

TEST_F(LogDataTest, CopyMove_StackBuffer_IsSame) {
	internal::LogData logData;
	{
		logData << "Test";
	}

	// first create a copy
	internal::LogData copy(logData);

	// then move that copy to a new instance
	internal::LogData move(std::move(copy));

	// assign the moved-to instance to the next
	internal::LogData assign;
	assign = move;

	// and finally move assign the value
	internal::LogData moveAssign;
	moveAssign = std::move(assign);

	// now check that the result is still the same
	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		moveAssign.CopyArgumentsTo(args);

		ASSERT_EQ(1u, args.size());
		ASSERT_EQ(5u, args[0].Size);  // including terminating null character
		EXPECT_EQ("Test", std::string_view(reinterpret_cast<const char*>(args[0].Ptr), args[0].Size / sizeof(char) - 1));
	}
}

#if 0
TEST(LogLine_Test, CopyMove_StackBufferWithNonTriviallyCopyable_IsSame) {
	LogLine logLine = GetLogLine("{} {} {} {}");
	{
		const char* arg0 = "Test";
		const CustomTypeTrivial customTrivial(7);
		const CustomTypeCopyOnly customCopy;
		const CustomTypeMove customMove;

		logLine << customTrivial << customCopy << customMove << arg0;
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ("(7) (copy #1) (copy #1 move #0) Test", str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +1

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +1

	// optimum is copy +2 and move +2

	// now check that the result is still the same
	EXPECT_EQ("(7) (copy #5) (copy #3 move #2) Test", moveAssign.GetLogMessage());
}
#endif

TEST_F(LogDataTest, CopyMove_HeapBuffer_IsSame) {
	internal::LogData logData;
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

		auto arg24 = make_win32_error(ERROR_ACCESS_DENIED);
		auto arg25 = make_rpc_status(RPC_S_ALREADY_REGISTERED);
		auto arg26 = make_hresult(E_INVALIDARG);

		logData << arg00 << arg01 << arg02 << arg03 << arg04 << arg05
				<< arg06 << arg07 << arg08 << arg09 << arg10 << arg11
				<< arg12 << arg13
				<< ptr14 << ptr15
				<< arg16 << arg17 << arg18 << arg19
				<< arg20 << arg21 << arg22 << arg23.sid
				<< arg24 << arg25 << arg26;
	}

	// first create a copy
	internal::LogData copy(logData);

	// then move that copy to a new instance
	internal::LogData move(std::move(copy));

	// assign the moved-to instance to the next
	internal::LogData assign;
	assign = move;

	// and finally move assign the value
	internal::LogData moveAssign;
	moveAssign = std::move(assign);

	// now check that the result is still the same
	{
		std::vector<EVENT_DATA_DESCRIPTOR> args;
		logData.CopyArgumentsTo(args);

		ASSERT_EQ(27u, args.size());

		ASSERT_EQ(sizeof(bool), args[0].Size);
		EXPECT_EQ(true, *reinterpret_cast<const bool*>(args[0].Ptr));

		ASSERT_EQ(sizeof(char), args[1].Size);
		EXPECT_EQ('c', *reinterpret_cast<const char*>(args[1].Ptr));

		ASSERT_EQ(sizeof(signed char), args[2].Size);
		EXPECT_EQ(-2, *reinterpret_cast<const signed char*>(args[2].Ptr));
		ASSERT_EQ(sizeof(unsigned char), args[3].Size);
		EXPECT_EQ(3u, *reinterpret_cast<const unsigned char*>(args[3].Ptr));

		ASSERT_EQ(sizeof(signed short), args[4].Size);
		EXPECT_EQ(-4, *reinterpret_cast<const signed short*>(args[4].Ptr));
		ASSERT_EQ(sizeof(unsigned short), args[5].Size);
		EXPECT_EQ(5u, *reinterpret_cast<const unsigned short*>(args[5].Ptr));

		ASSERT_EQ(sizeof(signed int), args[6].Size);
		EXPECT_EQ(-6, *reinterpret_cast<const signed int*>(args[6].Ptr));
		ASSERT_EQ(sizeof(unsigned int), args[7].Size);
		EXPECT_EQ(7u, *reinterpret_cast<const unsigned int*>(args[7].Ptr));

		ASSERT_EQ(sizeof(signed long), args[8].Size);
		EXPECT_EQ(-8, *reinterpret_cast<const signed long*>(args[8].Ptr));
		ASSERT_EQ(sizeof(unsigned long), args[9].Size);
		EXPECT_EQ(9u, *reinterpret_cast<const unsigned long*>(args[9].Ptr));

		ASSERT_EQ(sizeof(signed long long), args[10].Size);
		EXPECT_EQ(-10, *reinterpret_cast<const signed long long*>(args[10].Ptr));
		ASSERT_EQ(sizeof(unsigned long long), args[11].Size);
		EXPECT_EQ(11u, *reinterpret_cast<const unsigned long long*>(args[11].Ptr));

		ASSERT_EQ(sizeof(float), args[12].Size);
		EXPECT_EQ(12.12f, *reinterpret_cast<const float*>(args[12].Ptr));
		ASSERT_EQ(sizeof(double), args[13].Size);
		EXPECT_EQ(13.13, *reinterpret_cast<const double*>(args[13].Ptr));

		ASSERT_EQ(sizeof(void*), args[14].Size);
		EXPECT_EQ(reinterpret_cast<const void*>(0x140014), *reinterpret_cast<const void**>(args[14].Ptr));
		ASSERT_EQ(sizeof(void*), args[15].Size);
		EXPECT_EQ(0x0, *reinterpret_cast<const void**>(args[15].Ptr));

		ASSERT_EQ(7u, args[16].Size);  // including terminating null character
		EXPECT_EQ("Test16", std::string_view(reinterpret_cast<const char*>(args[16].Ptr), args[16].Size / sizeof(char) - 1));
		ASSERT_EQ(14u, args[17].Size);  // including terminating null character
		EXPECT_EQ(L"Test17", std::wstring_view(reinterpret_cast<const wchar_t*>(args[17].Ptr), args[17].Size / sizeof(wchar_t) - 1));
		ASSERT_EQ(513u, args[18].Size);  // including terminating null character
		EXPECT_EQ(std::string(512, 'x'), std::string_view(reinterpret_cast<const char*>(args[18].Ptr), args[18].Size / sizeof(char) - 1));
		ASSERT_EQ(514u, args[19].Size);  // including terminating null character
		EXPECT_EQ(std::wstring(256, 'y'), std::wstring_view(reinterpret_cast<const wchar_t*>(args[19].Ptr), args[19].Size / sizeof(wchar_t) - 1));

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
	}
}

#if 0
TEST(LogLine_Test, CopyMove_HeapBufferWithNonTriviallyCopyable_IsSame) {
	LogLine logLine = GetLogLine(
		"{} {} {} "
		"{} {} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {} {:.3} {:.3} "
		"{} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {}");
	{
		const CustomTypeTrivial customTrivial(7);
		const CustomTypeCopyOnly customCopy;
		const CustomTypeMove customMove;

		const bool arg00 = true;
		const bool* ptr00 = &arg00;

		const char arg01 = 'c';

		const signed char arg02 = -2;
		const signed char* ptr02 = &arg02;

		const unsigned char arg03 = 3;
		const unsigned char* ptr03 = nullptr;

		const signed short arg04 = -4;
		const signed short* ptr04 = &arg04;

		const unsigned short arg05 = 5;
		const unsigned short* ptr05 = &arg05;

		const signed int arg06 = -6;
		const signed int* ptr06 = &arg06;

		const unsigned int arg07 = 7;
		const unsigned int* ptr07 = &arg07;

		const signed long arg08 = -8;
		const signed long* ptr08 = &arg08;

		const unsigned long arg09 = 9;
		const unsigned long* ptr09 = &arg09;

		const signed long long arg10 = -10;
		const signed long long* ptr10 = &arg10;

		const unsigned long long arg11 = 11;
		const unsigned long long* ptr11 = &arg11;

		const float arg12 = 12.12f;
		const float* ptr12 = &arg12;

		const double arg13 = 13.13;
		const double* ptr13 = &arg13;

		const long double arg14 = 14.14L;
		const long double* ptr14 = &arg14;

		const void* ptr15 = reinterpret_cast<const void*>(0x150015);

		const nullptr_t ptr16 = nullptr;

		const char* arg17 = "Test17";
		const wchar_t* arg18 = L"Test18";

		const std::string arg19(512, 'x');

		const std::wstring arg20(256, 'y');

		logLine << customTrivial << customCopy << customMove
				<< arg00 << arg01 << arg02 << arg03 << arg04
				<< arg05 << arg06 << arg07 << arg08 << arg09
				<< arg10 << arg11 << arg12 << arg13 << arg14
				<< arg17 << arg18 << arg19 << arg20
				<< ptr00 << ptr02 << ptr03 << ptr04
				<< ptr05 << ptr06 << ptr07 << ptr08 << ptr09
				<< ptr10 << ptr11 << ptr12 << ptr13 << ptr14
				<< ptr15 << ptr16;
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ(
		"(7) (copy #3) (copy #1 move #2) true c -2 3 -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 Test17 Test18 xxx yyy "
		"true -2 (null) -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 0x150015 0x0",
		str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +0 because of heap buffer

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +0 because of heap buffer

	// optimum is copy +2 and move +0

	// now check that the result is still the same
	EXPECT_EQ(
		"(7) (copy #5) (copy #3 move #2) true c -2 3 -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 Test17 Test18 xxx yyy "
		"true -2 (null) -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 0x150015 0x0",
		moveAssign.GetLogMessage());
}


TEST(LogLine_Test, CopyMove_Exceptions_IsSame) {
	LogLine logLine = GetLogLine("{:%l} {:%l} {:%l} {:%l} {:%w} {:%c}");
	{
		try {
			llamalog::Throw(std::exception("e1"), "myfile.cpp", 15, "exfunc", "m1={:.3}", std::string(2, 'x'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::system_error(7, std::system_category(), "e2"), "myfile.cpp", 15, "exfunc", "m2={:.3}", std::string(2, 'y'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::exception("e3"), "myfile.cpp", 15, "exfunc", "m3={:.3}", std::string(512, 'x'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::system_error(7, std::system_category(), "e4"), "myfile.cpp", 15, "exfunc", "m4={:.3}", std::string(512, 'y'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			throw std::exception("e5");
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			throw std::system_error(7, std::system_category(), "e6");
		} catch (std::exception& e) {
			logLine << e;
		}
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ("m1=xx m2=yy m3=xxx m4=yyy e5 7", str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +0 because of heap buffer

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +0 because of heap buffer

	// optimum is copy +2 and move +0

	// now check that the result is still the same
	EXPECT_EQ(str, moveAssign.GetLogMessage());
}

#endif
}  // namespace m3c::test
