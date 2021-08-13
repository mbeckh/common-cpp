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

#include "m3c/mutex.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace m3c::test {

TEST(mutex, scoped_lock) {
	mutex mtx;
	condition_variable cond;

	scoped_lock lock(mtx);
	EXPECT_FALSE(cond.wait_for(lock, std::chrono::milliseconds(2)));
}

TEST(mutex, shared_lock) {
	mutex mtx;
	condition_variable cond;

	shared_lock lock(mtx);
	EXPECT_FALSE(cond.wait_for(lock, std::chrono::milliseconds(2)));
}

}  // namespace m3c::test
