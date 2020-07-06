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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <llamalog/LogWriter.h>
#include <llamalog/llamalog.h>

#include <objbase.h>

#include <memory>
#include <utility>

namespace t = testing;

int main(int argc, char** argv) {
	std::unique_ptr<lg::DebugWriter> writer = std::make_unique<lg::DebugWriter>(lg::Priority::kTrace);
	lg::Initialize(std::move(writer));

	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		LOG_FATAL("CoInitialize: {}", lg::error_code(hr));
	}
	auto f = m3c::finally([]() noexcept {
		CoUninitialize();
	});

	t::InitGoogleMock(&argc, argv);
	const int result = RUN_ALL_TESTS();

	lg::Shutdown();
	return result;
}
