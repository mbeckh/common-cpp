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

#include <sal.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace m3c {

/// @brief Similar to `std::unique_ptr` but allows setting pointer value by out parameter.
/// @tparam T The native type of the pointer.
template <class T>
class unique_ptr final {
public:
	/// @brief Creates an empty instance.
	constexpr unique_ptr() noexcept = default;

	unique_ptr(const unique_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param ptr Another `unique_ptr`.
	unique_ptr(unique_ptr&& ptr) noexcept
		: m_ptr(ptr.release()) {
		// empty
	}

	/// @brief Creates an empty instance.
	constexpr explicit unique_ptr(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Transfer ownership of an existing pointer.
	/// @param p The native pointer.
	constexpr explicit unique_ptr(_In_opt_ T* const p) noexcept
		: m_ptr(p) {
		// empty
	}

	/// @brief Deletes the owner object.
	~unique_ptr() noexcept {
		delete m_ptr;
	}

public:
	unique_ptr& operator=(const unique_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param p Another `unique_ptr`.
	/// @return This instance.
	unique_ptr& operator=(unique_ptr&& p) noexcept {
		delete m_ptr;
		m_ptr = p.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	unique_ptr& operator=(std::nullptr_t) noexcept {
		delete m_ptr;
		m_ptr = nullptr;
		return *this;
	}

	/// @brief Allows the smart pointer to act as the object.
	/// @return The native pointer to the interface.
	[[nodiscard]] _Ret_maybenull_ T* operator->() noexcept {
		return m_ptr;
	}

	/// @brief Allows the smart pointer to act as the object.
	/// @return The native pointer to the interface.
	[[nodiscard]] _Ret_maybenull_ const T* operator->() const noexcept {
		return m_ptr;
	}

	/// @brief Allows the smart pointer to act as the object.
	/// @details The behavior is undefined if no object is set.
	/// @return The managed object.
	[[nodiscard]] T& operator*() noexcept {
		return *m_ptr;
	}

	/// @brief Allows the smart pointer to act as the object.
	/// @details The behavior is undefined if no object is set.
	/// @return The managed object.
	[[nodiscard]] const T& operator*() const noexcept {
		return *m_ptr;
	}

	/// @brief Provides access to the storage location for functions returning pointers as out parameters.
	/// @details The function frees the currently held memory before returning the address.
	/// When a value is assigned to the return value of this function, ownership is transferred to this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ T** operator&() noexcept {
		delete m_ptr;
		m_ptr = nullptr;
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] explicit operator bool() const noexcept {
		return m_ptr;
	}

public:
	/// @brief Use the `unique_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* get() noexcept {
		return m_ptr;
	}

	/// @brief Use the `unique_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ const T* get() const noexcept {
		return m_ptr;
	}

	/// @brief Transfers ownership.
	/// @param p A pointer.
	void reset(_In_opt_ T* const p = nullptr) noexcept {
		if (m_ptr != p) {
			delete m_ptr;
			m_ptr = p;
		}
	}

	/// @brief Releases ownership of the pointer.
	/// @warning The responsibility for deleting the object is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param ptr The other `unique_ptr`.
	void swap(unique_ptr& ptr) noexcept {
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

/// @brief Allows comparison of two `unique_ptr` instances.
/// @tparam T The type of the first managed native pointer.
/// @tparam U The type of the second managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @param oth Another `unique_ptr` object.
/// @return `true` if @p ptr points to the same address as @p oth.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const unique_ptr<T>& ptr, const unique_ptr<U>& oth) noexcept {
	return ptr.get() == oth.get();
}

/// @brief Allows comparison of `unique_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `unique_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const unique_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `unique_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const U* const p, const unique_ptr<T>& ptr) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `unique_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] inline bool operator==(const unique_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !ptr;
}

/// @brief Allows comparison of `unique_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] inline bool operator==(std::nullptr_t, const unique_ptr<T>& ptr) noexcept {
	return !ptr;
}


//
// operator!=
//

/// @brief Allows comparison of two `unique_ptr` instances.
/// @tparam T The type of the first managed native pointer.
/// @tparam U The type of the second managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @param oth Another `unique_ptr` object.
/// @return `true` if @p ptr does not point to the same address as @p oth.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const unique_ptr<T>& ptr, const unique_ptr<U>& oth) noexcept {
	return ptr.get() != oth.get();
}

/// @brief Allows comparison of `unique_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `unique_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const unique_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `unique_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const U* const p, const unique_ptr<T>& ptr) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `unique_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] inline bool operator!=(const unique_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !!ptr;
}

/// @brief Allows comparison of `unique_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] inline bool operator!=(std::nullptr_t, const unique_ptr<T>& ptr) noexcept {
	return !!ptr;
}


/// @brief Specialization of the `unique_ptr` for arrays.
/// @tparam T The type.
template <class T>
class unique_ptr<T[]> final {
public:
	constexpr unique_ptr() noexcept = default;

	unique_ptr(const unique_ptr&) = delete;

	unique_ptr(unique_ptr&& ptr) noexcept
		: m_ptr(ptr.release()) {
		// empty
	}

	constexpr unique_ptr(std::nullptr_t) noexcept {
		// empty
	}

	explicit unique_ptr(_In_opt_ T* const p) noexcept
		: m_ptr(p) {
		//empty
	}

	~unique_ptr() noexcept {
		delete[] m_ptr;
	}

public:
	unique_ptr& operator=(const unique_ptr&) = delete;

	unique_ptr& operator=(unique_ptr&& ptr) noexcept {
		delete[] m_ptr;
		m_ptr = ptr.release();
		return *this;
	}

	unique_ptr& operator=(std::nullptr_t) noexcept {
		delete[] m_ptr;
		m_ptr = nullptr;
		return *this;
	}

	[[nodiscard]] _Ret_notnull_ T** operator&() noexcept {
		delete[] m_ptr;
		m_ptr = nullptr;
		return std::addressof(m_ptr);
	}

	[[nodiscard]] T& operator[](const std::size_t index) noexcept {
		return m_ptr[index];
	}

	[[nodiscard]] const T& operator[](const std::size_t index) const noexcept {
		return m_ptr[index];
	}

	[[nodiscard]] explicit operator bool() const noexcept {
		return m_ptr;
	}

public:
	[[nodiscard]] _Ret_maybenull_ T* get() noexcept {
		return m_ptr;
	}

	[[nodiscard]] _Ret_maybenull_ const T* get() const noexcept {
		return m_ptr;
	}

	void reset(_In_opt_ T* p = nullptr) noexcept {
		if (m_ptr != p) {
			delete[] m_ptr;
			m_ptr = p;
		}
	}

	[[nodiscard]] _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param ptr The other `unique_ptr`.
	void swap(unique_ptr& ptr) noexcept {
		std::swap(m_ptr, ptr.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] std::size_t hash() const noexcept {
		return std::hash<void*>{}(m_ptr);
	}

private:
	T* m_ptr = nullptr;
};


/// @brief Creates a new `unique_ptr` for plain objects.
/// @remarks The sole purpose of the function is to allow template argument deduction.
/// @tparam T The type of the pointer.
/// @tparam Args The types of the arguments for the constructor of @p T.
/// @param args The arguments for the constructor of @p T.
template <typename T, typename... Args>
[[nodiscard]] inline typename std::enable_if_t<!std::is_array_v<T>, unique_ptr<T>> make_unique(Args&&... args) {
	return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/// @brief Creates a new `unique_ptr` for an array.
/// @remarks The sole purpose of the function is to allow template argument deduction.
/// @tparam T The type of the pointer.
/// @param size The size of the array in number of elements of @p T.
template <typename T>
[[nodiscard]] inline typename std::enable_if_t<std::is_array_v<T>, unique_ptr<T>> make_unique(const size_t size) {
	return unique_ptr<T>(new std::remove_extent_t<T>[size]);
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `unique_ptr` object.
/// @param oth Another `unique_ptr` object.
template <typename T>
inline void swap(m3c::unique_ptr<T>& ptr, m3c::unique_ptr<T>& oth) noexcept {
	ptr.swap(oth);
}

}  // namespace m3c

/// @brief Specialization of std::hash.
template <typename T>
struct std::hash<m3c::unique_ptr<T>> {
	[[nodiscard]] std::size_t operator()(const m3c::unique_ptr<T>& ptr) const noexcept {
		return ptr.hash();
	}
};
