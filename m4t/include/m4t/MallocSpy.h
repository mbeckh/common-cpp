/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file

#pragma once

#include <m3c/ComObject.h>

#include <objidl.h>
#include <windows.h>

#include <cstddef>
#include <shared_mutex>
#include <unordered_set>

namespace m4t {

/// @brief Implementation of `IMallocSpy` for use in testing.
class MallocSpy : public m3c::ComObject<IMallocSpy> {
public:
	MallocSpy() = default;
	// allow creation on the stack
	~MallocSpy() = default;

public:  // IMallocSpy
	SIZE_T __stdcall PreAlloc(_In_ SIZE_T cbRequest) noexcept override;
	void* __stdcall PostAlloc(_In_ void* pActual) noexcept override;
	void* __stdcall PreFree(_In_ void* pRequest, _In_ BOOL fSpyed) noexcept override;
	void __stdcall PostFree(_In_ BOOL fSpyed) noexcept override;
	SIZE_T __stdcall PreRealloc(_In_ void* pRequest, _In_ SIZE_T cbRequest, _Outptr_ void** ppNewRequest, _In_ BOOL fSpyed) noexcept override;
	void* __stdcall PostRealloc(_In_ void* pActual, _In_ BOOL fSpyed) noexcept override;
	void* __stdcall PreGetSize(_In_ void* pRequest, _In_ BOOL fSpyed) noexcept override;
	SIZE_T __stdcall PostGetSize(_In_ SIZE_T cbActual, _In_ BOOL fSpyed) noexcept override;
	void* __stdcall PreDidAlloc(_In_ void* pRequest, _In_ BOOL fSpyed) noexcept override;
	int __stdcall PostDidAlloc(_In_ void* pRequest, _In_ BOOL fSpyed, _In_ int fActual) noexcept override;
	void __stdcall PreHeapMinimize() noexcept override;
	void __stdcall PostHeapMinimize() noexcept override;

public:
	bool IsAllocated(const void* p) const noexcept;
	bool IsDeleted(const void* p) const noexcept;
	std::size_t GetAllocatedCount() const noexcept;
	std::size_t GetDeletedCount() const noexcept;

private:
	mutable std::shared_mutex m_mutex;
	std::unordered_set<const void*> m_allocated;
	std::unordered_set<const void*> m_deleted;
};

}  // namespace m4t
