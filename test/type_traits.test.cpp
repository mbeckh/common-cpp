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

#include "m3c/type_traits.h"

#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace m3c::test {
namespace {

//
// SelectString
//
TEST(type_traits_Test, SelectString) {
	static constexpr char kChar[] = "123";
	static constexpr wchar_t kWideChar[] = L"456";

	static_assert(kChar == SelectString<char>(kChar, kWideChar));
	static_assert(kWideChar == SelectString<wchar_t>(kChar, kWideChar));
}


//
// is_any_of / AnyOf
//

static_assert(is_any_of<char, int, char, short>::value);
static_assert(!is_any_of<char, int, char*, short>::value);

static_assert(is_any_of_v<char, int, char, short>);
static_assert(!is_any_of_v<char, int, char*, short>);

static_assert(AnyOf<char, int, char, short>);
static_assert(!AnyOf<char, int, char*, short>);


//
// is_pointer_to / PointerTo
//

static_assert(is_pointer_to<char*, char>::value);
static_assert(is_pointer_to<const char*, char>::value);
static_assert(is_pointer_to<volatile char*, char>::value);

static_assert(is_pointer_to_v<char*, char>);
static_assert(is_pointer_to_v<const char*, char>);
static_assert(is_pointer_to_v<volatile char*, char>);

static_assert(PointerTo<char*, char>);
static_assert(PointerTo<const char*, char>);
static_assert(PointerTo<volatile char*, char>);


//
// is_specialization_of / SpecializationOf
//

static_assert(is_specialization_of<std::string, std::basic_string>::value);
static_assert(!is_specialization_of<std::string_view, std::basic_string>::value);

static_assert(is_specialization_of_v<std::string, std::basic_string>);
static_assert(!is_specialization_of_v<std::string_view, std::basic_string>);

static_assert(SpecializationOf<std::string, std::basic_string>);
static_assert(!SpecializationOf<std::string_view, std::basic_string>);


//
// is_span / Span
//

static_assert(is_span<std::span<char, 4>>::value);
static_assert(is_span<std::span<char>>::value);
static_assert(!is_span<std::string>::value);

static_assert(is_span_v<std::span<char, 4>>);
static_assert(is_span_v<std::span<char>>);
static_assert(!is_span_v<std::string>);

static_assert(Span<std::span<char, 4>>);
static_assert(Span<std::span<char>>);
static_assert(!Span<std::string>);


//
// is_unique_ptr_to / UniquePtrTo
//

static_assert(is_unique_ptr_to<std::unique_ptr<int>>::value);
static_assert(is_unique_ptr_to<std::unique_ptr<int[]>>::value);
static_assert(!is_unique_ptr_to<int>::value);
static_assert(!is_unique_ptr_to<std::shared_ptr<int>>::value);

static_assert(is_unique_ptr_to_v<std::unique_ptr<int>>);
static_assert(is_unique_ptr_to_v<std::unique_ptr<int[]>>);
static_assert(!is_unique_ptr_to_v<int>);
static_assert(!is_unique_ptr_to_v<std::shared_ptr<int>>);

static_assert(UniquePtrTo<std::unique_ptr<int>>);
static_assert(UniquePtrTo<std::unique_ptr<int[]>>);
static_assert(!UniquePtrTo<int>);
static_assert(!UniquePtrTo<std::shared_ptr<int>>);

}  // namespace
}  // namespace m3c::test
