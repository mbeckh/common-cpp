/*
Copyright 2020 Michael Beckh

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

#include "m3c/lazy_string.h"

#include "m3c/Log.h"
#include "m3c/exception.h"
#include "m3c/type_traits.h"

#include <m4t/LogListener.h>
#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <evntprov.h>

#include <compare>
#include <cwchar>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace m3c::test {
namespace {

namespace t = testing;

constexpr wchar_t kEmpty[] = L"";
constexpr wchar_t kInline[] = L"abcdefghijklmnopqrstuvwxyz";
constexpr wchar_t kString[] = L"0123456789012345678901234567890123456789";

constexpr wchar_t kInline2[] = L"yxwvutsrqponmlkjihgfedcba";
constexpr wchar_t kString2[] = L"876543210987654321098765432109876543210";

constexpr wchar_t kMinInline[] = L"00";
constexpr wchar_t kMaxInline[] = L"zz";

constexpr wchar_t kMinString[] = L"0012345678901234567890123456789012345678901234567890123456789";
constexpr wchar_t kMaxString[] = L"zz2345678901234567890123456789012345678901234567890123456789";

class lazy_string_Test : public t::Test {
public:
	using CharT = wchar_t;

	using StringT = lazy_wstring<32>;
	using AlwaysInlineT = lazy_wstring<50>;
	using AlwaysStringT = lazy_wstring<20>;

	using StringViewT = std::wstring_view;
};

class lazy_string_DataDrivenTest : public lazy_string_Test
    , public t::WithParamInterface<const lazy_string_Test::CharT*> {
protected:
	[[nodiscard]] static const CharT* GetValue() {
		return GetParam();
	}
	[[nodiscard]] static std::size_t GetSize() {
		return std::wcslen(GetParam());
	}
};

template <typename CharT>
class lazy_string_CharT_Test : public t::Test {
};

using CharTypes = t::Types<char, wchar_t>;

struct CharTNames {
	template <typename T>
	static std::string GetName(int /* index */) noexcept {
		if constexpr (std::is_same_v<T, char>) {
			return "0_char";
		} else if constexpr (std::is_same_v<T, wchar_t>) {
			return "1_wchar_t";
		} else {
			static_assert_no_clang(false, "unknown type");
		}
	}
};

TYPED_TEST_SUITE(lazy_string_CharT_Test, CharTypes, CharTNames);

INSTANTIATE_TEST_SUITE_P(lazy_string_Test, lazy_string_DataDrivenTest, t::Values(kEmpty, kInline, kString), [](const t::TestParamInfo<lazy_string_DataDrivenTest::ParamType>& param) {
	return FMT_FORMAT("{:03}_{}", param.index, param.param == kEmpty ? "Empty" : (param.param == kInline ? "Inline" : "String"));
});

TEST_F(lazy_string_Test, ctor_DefaultChar_IsEmpty) {
	const basic_lazy_string<10, char> lazyString;

	EXPECT_STREQ("", lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());
}

TEST_F(lazy_string_Test, ctor_Default_IsEmpty) {
	const StringT lazyString;

	EXPECT_STREQ(kEmpty, lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_Copy_Value) {
	const StringT src(GetValue());
	const StringT lazyString(src);  // NOLINT(performance-unnecessary-copy-initialization): Whole point of the test is to check copy operation.

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_CopyToDifferentInline_Value) {
	const StringT src(GetValue());
	const AlwaysInlineT lazyString(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_CopyToDifferentString_Value) {
	const StringT src(GetValue());
	const AlwaysStringT lazyString(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_Move_Value) {
	StringT src(GetValue());
	const StringT lazyString(std::move(src));

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_MoveToDifferentInline_Value) {
	StringT src(GetValue());
	const AlwaysInlineT lazyString(std::move(src));

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_MoveFromDifferentInline_Value) {
	AlwaysInlineT src(GetValue());
	const StringT lazyString(std::move(src));

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_MoveToDifferentString_Value) {
	StringT src(GetValue());
	const AlwaysStringT lazyString(std::move(src));

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_MoveFromDifferentString_Value) {
	AlwaysStringT src(GetValue());
	const StringT lazyString(std::move(src));

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_Pointer_Value) {
	const StringT lazyString(GetValue());

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_PointerAndLength_Value) {
	const StringT lazyString(GetValue(), GetSize());

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_F(lazy_string_Test, ctor_PointerAndLengthInline_Value) {
	const StringT lazyString(kInline, 5);

	EXPECT_STREQ(L"abcde", lazyString.c_str());
	EXPECT_EQ(5, lazyString.size());
}

TEST_F(lazy_string_Test, ctor_PointerAndLengthString_Value) {
	const StringT lazyString(kString, 35);

	EXPECT_STREQ(L"01234567890123456789012345678901234", lazyString.c_str());
	EXPECT_EQ(35, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_StringCopy_Value) {
	const std::wstring src(GetValue());
	const StringT lazyString(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_StringMove_Value) {
	std::wstring src(GetValue());
	const StringT lazyString(std::move(src));

	m4t::EnableMovedFromCheck(src);
	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(kEmpty, src.c_str());
	EXPECT_EQ(0, src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_StringViewDefaultCopy_Value) {
	const StringViewT src;
	const StringT lazyString(src);

	EXPECT_STREQ(L"", lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());

	EXPECT_EQ(L"", src);
	EXPECT_EQ(0, src.size());
}

TEST_P(lazy_string_DataDrivenTest, ctor_StringViewValueCopy_Value) {
	const StringViewT src(GetValue());
	const StringT lazyString(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_EQ(GetValue(), src);
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToEmpty_Value) {
	const StringT src(GetValue());
	StringT lazyString(kEmpty);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToInline_Value) {
	const StringT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToInlineFromDifferentInline_Value) {
	const AlwaysInlineT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToInlineFromDifferentString_Value) {
	const AlwaysStringT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToString_Value) {
	const StringT src(GetValue());
	StringT lazyString(kString2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToStringFromDifferentInline_Value) {
	const AlwaysInlineT src(GetValue());
	StringT lazyString(kString2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToStringFromDifferentString_Value) {
	const AlwaysStringT src(GetValue());
	StringT lazyString(kString2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToDifferentInlineAsInline_Value) {
	const StringT src(GetValue());
	AlwaysInlineT lazyString(kInline2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_ToDifferentStringAsString_Value) {
	const StringT src(GetValue());
	AlwaysStringT lazyString(kString2);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_Pointer_Value) {
	const std::wstring src(GetValue());
	StringT lazyString;

	lazyString = src.c_str();

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, assign_PointerAndLengthToEmpty_Value) {
	const std::wstring src(GetValue());
	StringT lazyString;

	lazyString.assign(src.c_str(), GetSize());

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, assign_PointerAndLengthToInline_Value) {
	const std::wstring src(GetValue());
	StringT lazyString(kInline);

	lazyString.assign(src.c_str(), GetSize());

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, assign_PointerAndLengthToString_Value) {
	const std::wstring src(GetValue());
	StringT lazyString(kString);

	lazyString.assign(src.c_str(), GetSize());

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringToEmpty_Value) {
	const std::wstring src(GetValue());
	StringT lazyString;

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringToInline_Value) {
	const std::wstring src(GetValue());
	StringT lazyString(kInline);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringToString_Value) {
	const std::wstring src(GetValue());
	StringT lazyString(kString);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_STREQ(GetValue(), src.c_str());
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewDefaultToEmpty_Value) {
	const StringViewT src;
	StringT lazyString;

	lazyString = src;

	EXPECT_STREQ(L"", lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());

	EXPECT_EQ(L"", src);
	EXPECT_EQ(0, src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewValueToEmpty_Value) {
	const StringViewT src(GetValue());
	StringT lazyString;

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_EQ(GetValue(), src);
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewDefaultToInline_Value) {
	const StringViewT src;
	StringT lazyString(kInline);

	lazyString = src;

	EXPECT_STREQ(L"", lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());

	EXPECT_EQ(L"", src);
	EXPECT_EQ(0, src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewValueToInline_Value) {
	const StringViewT src(GetValue());
	StringT lazyString(kInline);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_EQ(GetValue(), src);
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewDefaultToString_Value) {
	const StringViewT src;
	StringT lazyString(kString);

	lazyString = src;

	EXPECT_STREQ(L"", lazyString.c_str());
	EXPECT_EQ(0, lazyString.size());

	EXPECT_EQ(L"", src);
	EXPECT_EQ(0, src.size());
}

TEST_P(lazy_string_DataDrivenTest, opAssign_StringViewValueToString_Value) {
	const StringViewT src(GetValue());
	StringT lazyString(kString);

	lazyString = src;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());

	EXPECT_EQ(GetValue(), src);
	EXPECT_EQ(GetSize(), src.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToEmpty_Value) {
	StringT src(GetValue());
	StringT lazyString(kEmpty);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToInline_Value) {
	StringT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToInlineFromDifferentInline_Value) {
	AlwaysInlineT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToInlineFromDifferentString_Value) {
	AlwaysStringT src(GetValue());
	StringT lazyString(kInline2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToString_Value) {
	StringT src(GetValue());
	StringT lazyString(kString2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToStringFromDifferentInline_Value) {
	AlwaysInlineT src(GetValue());
	StringT lazyString(kString2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToStringFromDifferentString_Value) {
	AlwaysStringT src(GetValue());
	StringT lazyString(kString2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToDifferentInlineAsInline_Value) {
	StringT src(GetValue());
	AlwaysInlineT lazyString(kInline2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_ToDifferentStringAsString_Value) {
	StringT src(GetValue());
	AlwaysStringT lazyString(kString2);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_StringToEmpty_Value) {
	std::wstring src(GetValue());
	StringT lazyString;

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_StringToInline_Value) {
	std::wstring src(GetValue());
	StringT lazyString(kInline);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opMove_StringToString_Value) {
	std::wstring src(GetValue());
	StringT lazyString(kString);

	lazyString = std::move(src);

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_LazyString_Value) {
	const AlwaysInlineT str;
	StringT lazyString(GetValue());

	lazyString += str;

	EXPECT_EQ(GetValue() + std::wstring(str.sv()), lazyString.c_str());
	EXPECT_EQ(GetSize() + str.size(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_Char_Value) {
	StringT lazyString(GetValue());

	lazyString += L'x';

	EXPECT_EQ(GetValue() + std::wstring(1, L'x'), lazyString.c_str());
	EXPECT_EQ(GetSize() + 1, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_PointerEmpty_Value) {
	StringT lazyString(GetValue());

	lazyString += L"";

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_PointerShort_Value) {
	StringT lazyString(GetValue());

	lazyString += L"ab";

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 2, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_PointerLong_Value) {
	StringT lazyString(GetValue());

	lazyString += L"0123456789";

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 10, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, append_PointerAndLength_Value) {
	StringT lazyString(GetValue());

	lazyString.append(L"0123456789", 5);

	EXPECT_EQ(GetValue() + std::wstring(L"01234"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 5, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringEmpty_Value) {
	const std::wstring str;
	StringT lazyString(GetValue());

	lazyString += str;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringShort_Value) {
	const std::wstring str(L"ab");
	StringT lazyString(GetValue());

	lazyString += str;

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 2, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringLong_Value) {
	const std::wstring str(L"0123456789");
	StringT lazyString(GetValue());

	lazyString += str;

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 10, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringViewDefault_Value) {
	const StringViewT stringView;
	StringT lazyString(GetValue());

	lazyString += stringView;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringViewEmpty_Value) {
	const StringViewT stringView(L"x", 0);  // NOLINT(bugprone-string-constructor): Intention is to create a string_view with length 0.
	StringT lazyString(GetValue());

	lazyString += stringView;

	EXPECT_STREQ(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringViewShort_Value) {
	const StringViewT stringView(L"abx", 2);
	StringT lazyString(GetValue());

	lazyString += stringView;

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 2, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opAppend_StringViewLong_Value) {
	const StringViewT stringView(L"0123456789x", 10);
	StringT lazyString(GetValue());

	lazyString += stringView;

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), lazyString.c_str());
	EXPECT_EQ(GetSize() + 10, lazyString.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_LazyString_Concatenate) {
	const AlwaysInlineT str;
	const StringT lazyString(GetValue());

	const StringT result = lazyString + str;

	EXPECT_EQ(GetValue() + std::wstring(str.sv()), result.c_str());
	EXPECT_EQ(GetSize() + str.size(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_Char_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = lazyString + L'x';

	EXPECT_EQ(GetValue() + std::wstring(1, L'x'), result.c_str());
	EXPECT_EQ(GetSize() + 1, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToChar_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = L'x' + lazyString;

	EXPECT_EQ(std::wstring(1, L'x') + GetValue(), result.c_str());
	EXPECT_EQ(1 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_PointerEmpty_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = lazyString + L"";

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_PointerShort_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = lazyString + L"ab";

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), result.c_str());
	EXPECT_EQ(GetSize() + 2, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_PointerLong_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = lazyString + L"0123456789";

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), result.c_str());
	EXPECT_EQ(GetSize() + 10, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToPointerEmpty_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = L"" + lazyString;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToPointerShort_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = L"ab" + lazyString;

	EXPECT_EQ(std::wstring(L"ab") + GetValue(), result.c_str());
	EXPECT_EQ(2 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToPointerLong_Concatenate) {
	const StringT lazyString(GetValue());

	const StringT result = L"0123456789" + lazyString;

	EXPECT_EQ(std::wstring(L"0123456789") + GetValue(), result.c_str());
	EXPECT_EQ(10 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringEmpty_Concatenate) {
	const std::wstring str;
	const StringT lazyString(GetValue());

	const StringT result = lazyString + str;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringShort_Concatenate) {
	const std::wstring str(L"ab");
	const StringT lazyString(GetValue());

	const StringT result = lazyString + str;

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), result.c_str());
	EXPECT_EQ(GetSize() + 2, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringLong_Concatenate) {
	const std::wstring str(L"0123456789");
	const StringT lazyString(GetValue());

	const StringT result = lazyString + str;

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), result.c_str());
	EXPECT_EQ(GetSize() + 10, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringEmpty_Concatenate) {
	const std::wstring str;
	const StringT lazyString(GetValue());

	const StringT result = str + lazyString;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringShort_Concatenate) {
	const std::wstring str(L"ab");
	const StringT lazyString(GetValue());

	const StringT result = str + lazyString;

	EXPECT_EQ(std::wstring(L"ab") + GetValue(), result.c_str());
	EXPECT_EQ(2 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringLong_Concatenate) {
	const std::wstring str(L"0123456789");
	const StringT lazyString(GetValue());

	const StringT result = str + lazyString;

	EXPECT_EQ(std::wstring(L"0123456789") + GetValue(), result.c_str());
	EXPECT_EQ(10 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringViewDefault_Concatenate) {
	const StringViewT stringView;
	const StringT lazyString(GetValue());

	const StringT result = lazyString + stringView;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringViewEmpty_Concatenate) {
	const StringViewT stringView(L"x", 0);  // NOLINT(bugprone-string-constructor): Intention is to create a string_view with length 0.
	const StringT lazyString(GetValue());

	const StringT result = lazyString + stringView;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringViewShort_Concatenate) {
	const StringViewT stringView(L"abx", 2);
	const StringT lazyString(GetValue());

	const StringT result = lazyString + stringView;

	EXPECT_EQ(GetValue() + std::wstring(L"ab"), result.c_str());
	EXPECT_EQ(GetSize() + 2, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_StringViewLong_Concatenate) {
	const StringViewT stringView(L"0123456789x", 10);
	const StringT lazyString(GetValue());

	const StringT result = lazyString + stringView;

	EXPECT_EQ(GetValue() + std::wstring(L"0123456789"), result.c_str());
	EXPECT_EQ(GetSize() + 10, result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringViewDefault_Concatenate) {
	const StringViewT stringView;
	const StringT lazyString(GetValue());

	const StringT result = stringView + lazyString;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringViewEmpty_Concatenate) {
	const StringViewT stringView(L"x", 0);  // NOLINT(bugprone-string-constructor): Intention is to create a string_view with length 0.
	const StringT lazyString(GetValue());

	const StringT result = stringView + lazyString;

	EXPECT_STREQ(GetValue(), result.c_str());
	EXPECT_EQ(GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringViewShort_Concatenate) {
	const StringViewT stringView(L"abx", 2);
	const StringT lazyString(GetValue());

	const StringT result = stringView + lazyString;

	EXPECT_EQ(std::wstring(L"ab") + GetValue(), result.c_str());
	EXPECT_EQ(2 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opConcat_ToStringViewLong_Concatenate) {
	const StringViewT stringView(L"0123456789x", 10);
	const StringT lazyString(GetValue());

	const StringT result = stringView + lazyString;

	EXPECT_EQ(std::wstring(L"0123456789") + GetValue(), result.c_str());
	EXPECT_EQ(10 + GetSize(), result.size());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_Identity_IsEqual) {
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> lazyString == 0);
	EXPECT_TRUE(lazyString == lazyString);
	EXPECT_FALSE(lazyString != lazyString);
	EXPECT_FALSE(lazyString < lazyString);
	EXPECT_TRUE(lazyString <= lazyString);
	EXPECT_FALSE(lazyString > lazyString);
	EXPECT_TRUE(lazyString >= lazyString);

	EXPECT_EQ(lazyString.hash(), lazyString.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_Same_IsEqual) {
	const StringT str(GetValue());
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str == 0);
	EXPECT_TRUE(lazyString == str);
	EXPECT_FALSE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);

	EXPECT_EQ(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_LessThanEmpty_IsNotLessThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringT str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_LessThanInline_IsLessThan) {
	const StringT str(kMaxInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_LessThanString_IsLessThan) {
	const StringT str(kMaxString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_GreatherThanEmpty_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringT str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_GreatherThanInline_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringT str(kMinInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_GreatherThanString_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringT str(kMinString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);

	EXPECT_NE(lazyString.hash(), str.hash());
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerSame_IsEqual) {
	const wchar_t* const str = GetValue();
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str == 0);
	EXPECT_TRUE(lazyString == str);
	EXPECT_FALSE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerLessThanEmpty_IsNotLessThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const wchar_t* const str = kEmpty;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerLessThanInline_IsLessThan) {
	const wchar_t* const str = kMaxInline;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerLessThanString_IsLessThan) {
	const wchar_t* const str = kMaxString;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerGreatherThanEmpty_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const wchar_t* const str = kEmpty;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerGreatherThanInline_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const wchar_t* const str = kMinInline;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_PointerGreatherThanString_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const wchar_t* const str = kMinString;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringSame_IsEqual) {
	const std::wstring str(GetValue());
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str == 0);
	EXPECT_TRUE(lazyString == str);
	EXPECT_FALSE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringLessThanEmpty_IsNotLessThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const std::wstring str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringLessThanInline_IsLessThan) {
	const std::wstring str(kMaxInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringLessThanString_IsLessThan) {
	const std::wstring str(kMaxString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringGreatherThanEmpty_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const std::wstring str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringGreatherThanInline_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const std::wstring str(kMinInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringGreatherThanString_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const std::wstring str(kMinString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewSame_IsEqual) {
	const StringViewT str(GetValue());
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str == 0);
	EXPECT_TRUE(lazyString == str);
	EXPECT_FALSE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_EmptyAndStringViewDefault_IsEqual) {
	if (GetValue() != kEmpty) {
		return;
	}

	const StringViewT str;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str == 0);
	EXPECT_TRUE(lazyString == str);
	EXPECT_FALSE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewDefault_IsNotLessThan) {
	if (GetValue() == kEmpty) {
		return;
	}

	const StringViewT str;
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewLessThanEmpty_IsNotLessThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringViewT str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewLessThanInline_IsLessThan) {
	const StringViewT str(kMaxInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewLessThanString_IsLessThan) {
	const StringViewT str(kMaxString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str < 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_TRUE(lazyString < str);
	EXPECT_TRUE(lazyString <= str);
	EXPECT_FALSE(lazyString > str);
	EXPECT_FALSE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewGreatherThanEmpty_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringViewT str(kEmpty);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewGreatherThanInline_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringViewT str(kMinInline);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, opCompare_StringViewGreatherThanString_IsGreatherThan) {
	if (GetValue() == kEmpty) {
		return;
	}
	const StringViewT str(kMinString);
	const StringT lazyString(GetValue());

	EXPECT_TRUE(lazyString <=> str > 0);
	EXPECT_FALSE(lazyString == str);
	EXPECT_TRUE(lazyString != str);
	EXPECT_FALSE(lazyString < str);
	EXPECT_FALSE(lazyString <= str);
	EXPECT_TRUE(lazyString > str);
	EXPECT_TRUE(lazyString >= str);
}

TEST_P(lazy_string_DataDrivenTest, str_Value_ReturnStringView) {
	const StringT lazyString(GetValue());

	const StringViewT sv = lazyString.sv();

	EXPECT_EQ(lazyString.size(), sv.size());
	EXPECT_EQ(lazyString.c_str(), sv.data());
	EXPECT_EQ(GetValue(), sv);
}

TEST_P(lazy_string_DataDrivenTest, cstr_Value_ReturnPointer) {
	const StringT lazyString(GetValue());

	const CharT* cstr = lazyString.c_str();

	EXPECT_STREQ(GetValue(), cstr);
}

TEST_P(lazy_string_DataDrivenTest, data_Value_ReturnPointer) {
	StringT lazyString(GetValue());

	const CharT* const data = lazyString.data();

	EXPECT_STREQ(GetValue(), data);
	EXPECT_EQ(lazyString.c_str(), data);
}

TEST_P(lazy_string_DataDrivenTest, data_Update_ChangeValue) {
	if (GetValue() == kEmpty) {
		return;
	}
	StringT lazyString(GetValue());

	CharT* data = lazyString.data();
	*data = L'x';

	EXPECT_STRNE(GetValue(), lazyString.c_str());
	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_EQ(L'x', lazyString.sv()[0]);
}

TEST_P(lazy_string_DataDrivenTest, empty_Value_ReturnEmpty) {
	const StringT lazyString(GetValue());

	const bool empty = lazyString.empty();

	EXPECT_EQ(GetValue() == kEmpty, empty);
}

TEST_P(lazy_string_DataDrivenTest, clear_call_IsEmpty) {
	StringT lazyString(GetValue());

	lazyString.clear();

	EXPECT_EQ(0, lazyString.size());
	EXPECT_STREQ(kEmpty, lazyString.c_str());
}

TEST_P(lazy_string_DataDrivenTest, resize_ShrinkToEmpty_Shrink) {
	StringT lazyString(GetValue());

	lazyString.resize(0);

	EXPECT_EQ(0, lazyString.size());
	EXPECT_STREQ(kEmpty, lazyString.c_str());
}

TEST_P(lazy_string_DataDrivenTest, resize_ShrinkToInline_Shrink) {
	if (GetValue() == kEmpty) {
		return;
	}
	StringT lazyString(GetValue());

	lazyString.resize(2);

	EXPECT_EQ(2, lazyString.size());
	EXPECT_EQ(std::wstring(GetValue(), 2), lazyString.c_str());
}

TEST_P(lazy_string_DataDrivenTest, resize_ShrinkToString_Shrink) {
	if (GetValue() == kEmpty || GetValue() == kInline) {
		return;
	}
	StringT lazyString(GetValue());

	lazyString.resize(35);

	EXPECT_EQ(35, lazyString.size());
	EXPECT_EQ(std::wstring(GetValue(), 35), lazyString.c_str());
}

TEST_P(lazy_string_DataDrivenTest, resize_GrowToInline_Grow) {
	if (GetValue() == kString) {
		return;
	}
	StringT lazyString(GetValue());

	lazyString.resize(30);

	EXPECT_EQ(30, lazyString.size());
	EXPECT_THAT(lazyString.c_str(), t::StartsWith(GetValue()));
}

TEST_P(lazy_string_DataDrivenTest, resize_GrowToString_Grow) {
	StringT lazyString(GetValue());

	lazyString.resize(100);

	EXPECT_EQ(100, lazyString.size());
	EXPECT_THAT(lazyString.c_str(), t::StartsWith(GetValue()));
}

TEST_P(lazy_string_DataDrivenTest, swap_Empty_IsEmpty) {
	StringT str(GetValue());
	StringT lazyString(kEmpty);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(0, str.size());
	EXPECT_STREQ(kEmpty, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_Inline_IsInline) {
	StringT str(GetValue());
	StringT lazyString(kInline2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kInline2), str.size());
	EXPECT_STREQ(kInline2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_InlineInDifferentInline_IsInline) {
	StringT str(GetValue());
	AlwaysInlineT lazyString(kInline2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kInline2), str.size());
	EXPECT_STREQ(kInline2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_InlineInDifferentString_IsInline) {
	StringT str(GetValue());
	AlwaysStringT lazyString(kInline2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kInline2), str.size());
	EXPECT_STREQ(kInline2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_InlineToDifferentInline_IsInline) {
	AlwaysInlineT str(GetValue());
	StringT lazyString(kInline2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kInline2), str.size());
	EXPECT_STREQ(kInline2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_InlineToDifferentString_IsString) {
	AlwaysStringT str(GetValue());
	StringT lazyString(kInline2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kInline2), str.size());
	EXPECT_STREQ(kInline2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_String_IsString) {
	StringT str(GetValue());
	StringT lazyString(kString2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(sizeof(kString2) / sizeof(*kString2) - 1, str.size());
	EXPECT_STREQ(kString2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_StringInDifferentInline_IsString) {
	StringT str(GetValue());
	AlwaysInlineT lazyString(kString2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kString2), str.size());
	EXPECT_STREQ(kString2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_StringInDifferentString_IsString) {
	StringT str(GetValue());
	AlwaysStringT lazyString(kString2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kString2), str.size());
	EXPECT_STREQ(kString2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_StringToDifferentInline_IsString) {
	AlwaysInlineT str(GetValue());
	StringT lazyString(kString2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kString2), str.size());
	EXPECT_STREQ(kString2, str.c_str());
}

TEST_P(lazy_string_DataDrivenTest, swap_StringToDifferentString_IsString) {
	AlwaysStringT str(GetValue());
	StringT lazyString(kString2);

	lazyString.swap(str);

	EXPECT_EQ(GetSize(), lazyString.size());
	EXPECT_STREQ(GetValue(), lazyString.c_str());

	EXPECT_EQ(std::wcslen(kString2), str.size());
	EXPECT_STREQ(kString2, str.c_str());
}


//
// Formatting
//

TYPED_TEST(lazy_string_CharT_Test, format_Empty_Print) {
	constexpr TypeParam kValue[] = {'\0'};

	basic_lazy_string<16, TypeParam> arg(kValue);
	EXPECT_EQ("", fmt::to_string(arg));
}

TYPED_TEST(lazy_string_CharT_Test, format_EmptyCentered_PrintCentered) {
	constexpr TypeParam kValue[] = {'\0'};

	basic_lazy_string<16, TypeParam> arg(kValue);
	EXPECT_EQ("   ", FMT_FORMAT("{:^3}", arg));
}

TYPED_TEST(lazy_string_CharT_Test, format_Default_Print) {
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};

	basic_lazy_string<16, TypeParam> arg(kValue);
	EXPECT_EQ("Test", fmt::to_string(arg));
}

TYPED_TEST(lazy_string_CharT_Test, format_DefaultW_Print) {
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};

	basic_lazy_string<16, TypeParam> arg(kValue);
	EXPECT_EQ(L"Test", fmt::to_wstring(arg));
}

TYPED_TEST(lazy_string_CharT_Test, format_Centered_PrintCentered) {
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};

	basic_lazy_string<16, TypeParam> arg(kValue);
	EXPECT_EQ(" Test ", FMT_FORMAT("{:^6}", arg));
}


//
// Logging
//

namespace {

template <typename T>
constexpr const char* kLogPattern = nullptr;

template <>
constexpr const char* kLogPattern<char> = "char[]={}\t";
template <>
constexpr const char* kLogPattern<wchar_t> = "wchar_t[]={}\t";

template <typename T>
constexpr const EVENT_DESCRIPTOR& kLogEvent = EVENT_DESCRIPTOR{};

template <>
constexpr const EVENT_DESCRIPTOR& kLogEvent<char> = evt::Test_LogString;
template <>
constexpr const EVENT_DESCRIPTOR& kLogEvent<wchar_t> = evt::Test_LogWideString;

}  // namespace

TYPED_TEST(lazy_string_CharT_Test, LogString_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	basic_lazy_string<16, TypeParam> arg;

	EXPECT_CALL(log, Debug(t::_, "\t"));

	Log::Info("{}{}", arg, '\t');
}

TYPED_TEST(lazy_string_CharT_Test, LogString_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};
	basic_lazy_string<16, TypeParam> arg(kValue);

	EXPECT_CALL(log, Debug(t::_, "Test\t"));

	Log::Info("{}{}", arg, '\t');
}

TYPED_TEST(lazy_string_CharT_Test, LogEvent_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	constexpr TypeParam kEmptyString[] = {'\0'};
	basic_lazy_string<16, TypeParam> arg;

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT(kLogPattern<TypeParam>, "")));
	EXPECT_CALL(log, Event(kLogEvent<TypeParam>.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(kEmptyString), m4t::PointerAs<TypeParam>(t::StrEq(kEmptyString))));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(kLogEvent<TypeParam>, arg, '\t');
}

TYPED_TEST(lazy_string_CharT_Test, LogEvent_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};
	basic_lazy_string<16, TypeParam> arg(kValue);

	EXPECT_CALL(log, Debug(t::_, FMT_FORMAT(kLogPattern<TypeParam>, "Test")));
	EXPECT_CALL(log, Event(kLogEvent<TypeParam>.Id, t::_, t::_, 2));
	EXPECT_CALL(log, EventArg(0, sizeof(kValue), m4t::PointerAs<TypeParam>(t::StrEq(kValue))));
	EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t')));

	Log::Info(kLogEvent<TypeParam>, arg, '\t');
}

TYPED_TEST(lazy_string_CharT_Test, LogException_Empty_LogEmpty) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	constexpr TypeParam kEmptyString[] = {'\0'};

	try {
		basic_lazy_string<16, TypeParam> arg;

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, FMT_FORMAT(kLogPattern<TypeParam>, ""))).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(kLogEvent<TypeParam>.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(kEmptyString), m4t::PointerAs<TypeParam>(t::StrEq(kEmptyString)))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + kLogEvent<TypeParam> << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

TYPED_TEST(lazy_string_CharT_Test, LogException_Value_LogValue) {
	m4t::LogListener log(m4t::LogListenerMode::kStrictAll);
	constexpr TypeParam kValue[] = {'T', 'e', 's', 't', '\0'};

	try {
		basic_lazy_string<16, TypeParam> arg(kValue);

		t::Sequence seqString;
		t::Sequence seqEvent;
		EXPECT_CALL(log, Debug(t::_, FMT_FORMAT(kLogPattern<TypeParam>, "Test"))).InSequence(seqString);
		EXPECT_CALL(log, Debug(t::_, "~Log~")).InSequence(seqString);
		EXPECT_CALL(log, Event(kLogEvent<TypeParam>.Id, t::_, t::_, 2)).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(0, sizeof(kValue), m4t::PointerAs<TypeParam>(t::StrEq(kValue)))).InSequence(seqEvent);
		EXPECT_CALL(log, EventArg(1, sizeof(char), m4t::PointeeAs<char>('\t'))).InSequence(seqEvent);
		EXPECT_CALL(log, Event(evt::Test_LogException.Id, t::_, t::_, 0)).InSequence(seqEvent);

		throw std::exception() + kLogEvent<TypeParam> << arg << '\t';
	} catch (...) {
		Log::InfoException(evt::Test_LogException);
	}
}

}  // namespace
}  // namespace m3c::test
