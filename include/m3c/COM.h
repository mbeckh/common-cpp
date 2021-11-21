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
#pragma once

#include <windows.h>

namespace m3c {

namespace internal {
class AbstractClassFactory;  // NOLINT(cppcoreguidelines-virtual-class-destructor): see AbstractClassFactory.
class AbstractComObject;
}  // namespace internal


/// @brief Access internal counters for COM.
/// @details The data is typically only required in main functions of COM DLLs.
class COM {
public:
	COM() = delete;
	COM(const COM&) = delete;
	COM(COM&&) = delete;
	~COM() = delete;

public:
	COM& operator=(const COM&) = delete;
	COM& operator=(COM&&) = delete;

public:
	/// @brief Get the number of locks currently held by all `ClassFactory` objects.
	/// @return The number of locks.
	[[nodiscard]] static ULONG GetLockCount() noexcept {
		return s_lockCount;
	}

	/// @brief Get the number of currently instantiated COM objects.
	/// @return The number of currently instantiated COM objects.
	[[nodiscard]] static ULONG GetObjectCount() noexcept {
		return s_objectCount;
	}

private:
	// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): Global variable to keep interface of AbstractClassFactory clean.
	static inline volatile ULONG s_lockCount;    ///< @brief The number of class factory locks currently held.
	static inline volatile ULONG s_objectCount;  ///< @brief The global object count.
	// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

	friend class internal::AbstractClassFactory;
	friend class internal::AbstractComObject;
};

}  // namespace m3c
