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
#pragma once

#include <llamalog/llamalog.h>

#include <objbase.h>
#include <sal.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <new>

namespace m3c {

/// @brief A RAII type for memory managed using `CoTaskMemAlloc` and `CoTaskMemFree`.
/// @tparam T The native type of the pointer.
template <typename T>
class com_heap_ptr final {
public:
	/// @brief Creates an empty instance.
	constexpr com_heap_ptr() noexcept = default;

	com_heap_ptr(const com_heap_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param ptr Another `com_heap_ptr`.
	com_heap_ptr(com_heap_ptr&& ptr) noexcept
		: m_ptr(ptr.release()) {
		// empty
	}

	/// @brief Creates an empty instance.
	constexpr explicit com_heap_ptr(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Transfer ownership of an existing pointer.
	/// @param p The native pointer.
	constexpr explicit com_heap_ptr(_In_opt_ T* const p) noexcept
		: m_ptr(p) {
		// empty
	}

	/// @brief Allocate and own a new memory block.
	/// @param count The size of the memory block in number of @p T elements.
	explicit com_heap_ptr(const std::size_t count)
		: m_ptr(reinterpret_cast<T*>(CoTaskMemAlloc(count * sizeof(T)))) {
		if (!m_ptr) {
			LLAMALOG_THROW(std::bad_alloc(), "CoTaskMemAlloc: {} * {} bytes", count, sizeof(T));
		}
	}

	/// @brief Calls `CoTaskMemFree`.
	~com_heap_ptr() noexcept {
		CoTaskMemFree(m_ptr);
	}

public:
	com_heap_ptr& operator=(const com_heap_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param ptr Another `com_heap_ptr`.
	/// @return This instance.
	com_heap_ptr& operator=(com_heap_ptr&& ptr) noexcept {
		CoTaskMemFree(m_ptr);
		m_ptr = ptr.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	com_heap_ptr& operator=(std::nullptr_t) noexcept {
		CoTaskMemFree(m_ptr);
		m_ptr = nullptr;
		return *this;
	}

	/// @brief Provides access to the storage location for functions returning pointers as out parameters.
	/// @details The function frees the currently held memory before returning the address.
	/// When a value is assigned to the return value of this function, ownership is transferred to this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ T** operator&() noexcept {
		CoTaskMemFree(m_ptr);
		m_ptr = nullptr;
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] explicit operator bool() const noexcept {
		return m_ptr;
	}

public:
	/// @brief Use the `com_heap_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* get() noexcept {
		return m_ptr;
	}

	/// @brief Use the `com_heap_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ const T* get() const noexcept {
		return m_ptr;
	}

	/// @brief Release ownership of a pointer.
	/// @note The responsibility for releasing the memory is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Change the size of the allocated memory block.
	/// @param count The new size of the memory block in number of @a T elements.
	void realloc(const std::size_t count) {
		T* const pNew = reinterpret_cast<T*>(CoTaskMemRealloc(m_ptr, count * sizeof(T)));
		if (!pNew) {
			LLAMALOG_THROW(std::bad_alloc(), "CoTaskMemRealloc: {} * {} bytes", count, sizeof(T));
		}
		m_ptr = pNew;
	}

	/// @brief Swap two objects.
	/// @param ptr The other `com_heap_ptr`.
	void swap(com_heap_ptr& ptr) noexcept {
		std::swap(m_ptr, ptr.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] std::size_t hash() const noexcept {
		return std::hash<T*>{}(m_ptr);
	}

private:
	T* m_ptr = nullptr;  ///< @brief The native pointer.
};


//
// operator==
//

/// @brief Allows comparison of two `com_heap_ptr` instances.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param oth Another `com_heap_ptr` object.
/// @return `true` if @p ptr points to the same address as @p oth.
template <typename T>
[[nodiscard]] inline bool operator==(const com_heap_ptr<T>& ptr, const com_heap_ptr<T>& oth) noexcept {
	return ptr.get() == oth.get();
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator==(const com_heap_ptr<T>& ptr, const T* const p) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param p A native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator==(const T* const p, const com_heap_ptr<T>& ptr) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] inline bool operator==(const com_heap_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !ptr;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] inline bool operator==(std::nullptr_t, const com_heap_ptr<T>& ptr) noexcept {
	return !ptr;
}


//
// operator!=
//

/// @brief Allows comparison of two `com_heap_ptr` instances.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param oth Another `com_heap_ptr` object.
/// @return `true` if @p ptr does not point to the same address as @p oth.
template <typename T>
[[nodiscard]] inline bool operator!=(const com_heap_ptr<T>& ptr, const com_heap_ptr<T>& oth) noexcept {
	return ptr.get() != oth.get();
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator!=(const com_heap_ptr<T>& ptr, const T* const p) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param p A native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator!=(const T* const p, const com_heap_ptr<T>& ptr) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] inline bool operator!=(const com_heap_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !!ptr;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] inline bool operator!=(std::nullptr_t, const com_heap_ptr<T>& ptr) noexcept {
	return !!ptr;
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param oth Another `com_heap_ptr` object.
template <typename T>
inline void swap(m3c::com_heap_ptr<T>& ptr, m3c::com_heap_ptr<T>& oth) noexcept {
	ptr.swap(oth);
}

}  // namespace m3c

/// @brief Specialization of std::hash.
/// @tparam T The type of the managed native pointer.
template <typename T>
struct std::hash<m3c::com_heap_ptr<T>> {
	[[nodiscard]] size_t operator()(const m3c::com_heap_ptr<T>& ptr) const noexcept {
		return ptr.hash();
	}
};
