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

#include "testevents.h"

#include "m3c/Log.h"
#include "m3c/finally.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
//#include <llamalog/LogLine.h>
//#include <llamalog/LogWriter.h>
//#include <llamalog/llamalog.h>
//#include <m4t/memory.h>

#include <windows.h>
#include <objbase.h>

#include <memory>
#include <new>
#include <utility>

namespace t = testing;

int main(int argc, char** argv) {
	// std::unique_ptr<lg::DebugWriter> writer = std::make_unique<lg::DebugWriter>(lg::Priority::kTrace);
	// lg::Initialize(std::move(writer));
	constexpr GUID kGUID = {0x7f241d8a, 0xf704, 0x4399, {0x8f, 0x1b, 0x91, 0xd1, 0xb2, 0x2e, 0xa3, 0xe2}};
	m3c::Log::Register(kGUID);

	const HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		m3c::Log::Critical(m3c::evt::CoInitialize, hr);
		return 1;
	}
	auto f = m3c::finally([]() noexcept {
		CoUninitialize();
	});

	t::InitGoogleMock(&argc, argv);
	const int result = RUN_ALL_TESTS();

	// lg::Shutdown();
	return result;
}
