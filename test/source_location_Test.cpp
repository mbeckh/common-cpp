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

#include "m3c/source_location.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace m3c::test {

TEST(source_location_Test, file) {
	constexpr source_location kSourceLocation = source_location::current();
	constexpr const char* kFile = kSourceLocation.file_name();
	EXPECT_STREQ(__FILE__, kFile);
}

TEST(source_location_Test, line) {
	constexpr source_location kSourceLocation = source_location::current();
	constexpr std::uint_least32_t kLine = kSourceLocation.line();
	EXPECT_EQ(__LINE__, kLine + 2);
}

TEST(source_location_Test, column) {
	constexpr source_location kSourceLocation = source_location::current();
	constexpr std::uint_least32_t kColumn = kSourceLocation.column();
	EXPECT_LT(4, kColumn);
}

TEST(source_location_Test, function) {
	constexpr source_location kSourceLocation = source_location::current();
	constexpr const char* kFunction = kSourceLocation.function_name();
	EXPECT_STREQ(__func__, kFunction);
}


}  // namespace m3c::test
