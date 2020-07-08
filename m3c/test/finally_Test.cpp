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

#include "m3c/finally.h"

#include <gtest/gtest.h>

#include <utility>

namespace m3c::test {

//
// finally
//

TEST(finally_Test, ctorFromLambda_WithArg_CallCleanup) {
	int i = 3;
	{
		auto f = finally([&]() noexcept {
			++i;
		});
		EXPECT_EQ(3, i);
	}
	EXPECT_EQ(4, i);
}

TEST(finally_Test, ctorMove_WithArg_CallCleanup) {
	int i = 3;
	{
		auto f = std::move(finally([&]() noexcept {
			++i;
		}));
		EXPECT_EQ(3, i);
	}
	EXPECT_EQ(4, i);
}

}  // namespace m3c::test
