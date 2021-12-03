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

#include "m3c/Log.h"
#include "m3c/finally.h"

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <objbase.h>

#include <string>

namespace t = testing;

constinit const GUID m3c::Log::kGuid = m3c::Test_Provider;

namespace m3c::log {

void Print(const m3c::Priority priority, const std::string& message) {
	Log::PrintDefault(priority, message);
}

}  // namespace m3c::log

int main(int argc, char** argv) {
	const HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		m3c::Log::Critical("CoInitialize: {}", m3c::hresult(hr));
		return 1;
	}
	const auto f = m3c::finally([]() noexcept {
		CoUninitialize();
	});

	t::InitGoogleMock(&argc, argv);
	const int result = RUN_ALL_TESTS();

	return result;
}
