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

/// @file

#include "m4t/memory.h"

#include "m4t/m4t.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_set>

namespace m4t {

namespace {

std::atomic_bool g_tracking(false);
std::recursive_mutex g_mutex;
std::unordered_set<const void*> g_tracked;
std::unordered_set<const void*> g_deleted;

}  // namespace

void memory_start_tracking(const void* ptr) noexcept {
	g_tracking.store(true, std::memory_order_release);

	std::lock_guard<decltype(g_mutex)> lock(g_mutex);
	g_tracked.insert(ptr);
}

void memory_stop_tracking() noexcept {
	g_tracking.store(false, std::memory_order_release);

	std::lock_guard<decltype(g_mutex)> lock(g_mutex);
	g_tracked.clear();
}

bool memory_is_deleted(const void* const ptr) noexcept {
	std::lock_guard<decltype(g_mutex)> lock(g_mutex);
	return g_deleted.count(ptr) == 1;
}

namespace internal {

/// @brief Defines the necessary functions for using `EXPECT_UNINITIALIZED` and `EXPECT_DELETED`.
/// @details The macro MUST be called in global scope.
void* memory_new(const std::size_t count) {
	void* const ptr = std::malloc(count);
	if (!ptr) {
		throw std::bad_alloc();
	}

	// for compatibility with MSVC
	std::memset(ptr, 0xCD, count);
	return ptr;
}

void memory_delete(void* ptr) noexcept {
	if (!ptr) {
		return;
	}

	if (g_tracking.load(std::memory_order_acquire)) {
		std::lock_guard<decltype(g_mutex)> lock(g_mutex);
		if (g_tracked.count(ptr) == 1) {
			g_deleted.insert(ptr);
		}
	}

	std::free(ptr);
}

}  // namespace internal
}  // namespace m4t
