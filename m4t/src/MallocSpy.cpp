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

/// @file

#include "m4t/MallocSpy.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <utility>

namespace m4t {

SIZE_T __stdcall MallocSpy::PreAlloc(_In_ const SIZE_T cbRequest) noexcept {
	return cbRequest;
}

void* __stdcall MallocSpy::PostAlloc(_In_ void* const pActual) noexcept {
	std::scoped_lock<decltype(m_mutex)> lock(m_mutex);
	m_allocated.insert(pActual);

	return pActual;
}

void* __stdcall MallocSpy::PreFree(_In_ void* const pRequest, _In_ const BOOL /* fSpyed */) noexcept {
	std::scoped_lock<decltype(m_mutex)> lock(m_mutex);
	m_allocated.erase(pRequest);
	m_deleted.insert(pRequest);

	return pRequest;
}

void __stdcall MallocSpy::PostFree(_In_ const BOOL /* fSpyed */) noexcept {
	// empty
}

SIZE_T __stdcall MallocSpy::PreRealloc(_In_ void* const pRequest, _In_ const SIZE_T cbRequest, _Outptr_ void** const ppNewRequest, _In_ BOOL /* fSpyed */) noexcept {
	std::scoped_lock<decltype(m_mutex)> lock(m_mutex);
	m_allocated.erase(pRequest);
	m_deleted.insert(pRequest);

	*ppNewRequest = pRequest;
	return cbRequest;
}

void* __stdcall MallocSpy::PostRealloc(_In_ void* const pActual, _In_ BOOL /* fSpyed */) noexcept {
	std::scoped_lock<decltype(m_mutex)> lock(m_mutex);
	m_allocated.insert(pActual);

	return pActual;
}

void* __stdcall MallocSpy::PreGetSize(_In_ void* const pRequest, _In_ BOOL /* fSpyed */) noexcept {
	return pRequest;
}

SIZE_T __stdcall MallocSpy::PostGetSize(_In_ const SIZE_T cbActual, _In_ BOOL /* fSpyed */) noexcept {
	return cbActual;
}

void* __stdcall MallocSpy::PreDidAlloc(_In_ void* const pRequest, _In_ BOOL /* fSpyed */) noexcept {
	return pRequest;
}

int __stdcall MallocSpy::PostDidAlloc(_In_ void* /* pRequest */, _In_ BOOL /* fSpyed */, _In_ const int fActual) noexcept {
	return fActual;
}

void __stdcall MallocSpy::PreHeapMinimize() noexcept {
	// empty
}

void __stdcall MallocSpy::PostHeapMinimize() noexcept {
	// empty
}

bool MallocSpy::IsAllocated(const void* const p) const noexcept {
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	return m_allocated.count(p) == 1;
}

bool MallocSpy::IsDeleted(const void* p) const noexcept {
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	return m_deleted.count(p) == 1;
}

std::size_t MallocSpy::GetAllocatedCount() const noexcept {
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	return m_allocated.size();
}

std::size_t MallocSpy::GetDeletedCount() const noexcept {
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	return m_deleted.size();
}

}  // namespace m4t
