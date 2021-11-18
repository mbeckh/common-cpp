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

/// @file

#include "m3c/com_heap_ptr.h"

#include "m3c/exception.h"

#include "m3c.events.h"

#include <objbase.h>

#include <cstdint>
#include <new>

namespace m3c::internal {

_Ret_notnull_ void* com_heap_ptr_base::allocate(const std::size_t count, const std::size_t size) {
	void* const ptr = CoTaskMemAlloc(count * size);
	if (!ptr) {
		[[unlikely]];
		throw std::bad_alloc() + evt::ComAlloc << static_cast<std::uint64_t>(count) << static_cast<std::uint64_t>(size);
	}
	return ptr;
}

_Ret_notnull_ void* com_heap_ptr_base::reallocate(_In_opt_ void* const ptr, const std::size_t count, const std::size_t size) {
	void* const pResult = CoTaskMemRealloc(ptr, count * size);
	if (!pResult) {
		[[unlikely]];
		throw std::bad_alloc() + evt::ComAlloc << static_cast<std::uint64_t>(count) << static_cast<std::uint64_t>(size);
	}
	return pResult;
}

void com_heap_ptr_base::deallocate(_In_opt_ void* const ptr) noexcept {
	CoTaskMemFree(ptr);
}

}  // namespace m3c::internal
