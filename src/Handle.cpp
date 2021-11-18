/*
Copyright 2020-2021 Michael Beckh

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

/// @file

#include "m3c/Handle.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include "m3c.events.h"

namespace m3c::internal {

void HandleCloser::Close(HANDLE hNative) {
	if (!CloseHandle(hNative)) {
		[[unlikely]];
		throw windows_error() + evt::HandleLeak_E;
	}
}

void HandleCloser::CloseSilently(HANDLE hNative) noexcept {
	if (!CloseHandle(hNative)) {
		[[unlikely]];
		Log::Error(evt::HandleLeak_E, last_error());
	}
}

void FindCloser::Close(HANDLE hNative) {
	if (!FindClose(hNative)) {
		[[unlikely]];
		throw windows_error() + evt::HandleLeak_E;
	}
}

void FindCloser::CloseSilently(HANDLE hNative) noexcept {
	if (!FindClose(hNative)) {
		[[unlikely]];
		Log::Error(evt::HandleLeak_E, last_error());
	}
}

template class BaseHandle<HandleCloser>;
template class BaseHandle<FindCloser>;

}  // namespace m3c::internal
