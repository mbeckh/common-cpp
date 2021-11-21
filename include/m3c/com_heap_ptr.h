/*
Copyright 2019-2021 Michael Beckh

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

#include <m3c/LogArgs.h>
#include <m3c/LogData.h>

#include <fmt/format.h>

#include <sal.h>

#include <cstddef>
#include <functional>
#include <memory>

namespace m3c {

namespace internal {

/// @brief Base class with common static helper methods.
class com_heap_ptr_base {
protected:
	constexpr com_heap_ptr_base() noexcept = default;
	constexpr com_heap_ptr_base(const com_heap_ptr_base&) noexcept = default;
	constexpr com_heap_ptr_base(com_heap_ptr_base&&) noexcept = default;
	constexpr ~com_heap_ptr_base() noexcept = default;

protected:
	constexpr com_heap_ptr_base& operator=(const com_heap_ptr_base&) noexcept = default;
	constexpr com_heap_ptr_base& operator=(com_heap_ptr_base&&) noexcept = default;

protected:
	/// @brief Wraps call to `CoTaskMemAlloc`
	/// @param count The number of object to allocate.
	/// @param size The size of each object.
	[[nodiscard]] static _Ret_notnull_ void* allocate(std::size_t count, std::size_t size);

	/// @brief Wraps call to `CoTaskMemRealloc`
	/// @param ptr A pointer to a memory block.
	/// @param count The number of object to allocate.
	/// @param size The size of each object.
	[[nodiscard]] static _Ret_notnull_ void* reallocate(_In_opt_ void* ptr, std::size_t count, std::size_t size);

	/// @brief Wraps call to `CoTaskMemFree`
	/// @param ptr A pointer to a memory block.
	static void deallocate(_In_opt_ void* ptr) noexcept;
};

}  // namespace internal


/// @brief A RAII type for memory managed using `CoTaskMemAlloc` and `CoTaskMemFree`.
/// @tparam T The native type of the pointer.
template <typename T>
class com_heap_ptr final : private internal::com_heap_ptr_base {
public:
	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr com_heap_ptr() noexcept = default;

	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr explicit com_heap_ptr(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Transfer ownership of an existing pointer.
	/// @param p The native pointer.
	[[nodiscard]] constexpr explicit com_heap_ptr(_In_opt_ T* const p) noexcept
	    : m_ptr(p) {
		// empty
	}

	/// @brief Allocate and own a new memory block.
	/// @param count The size of the memory block in number of @p T elements.
	[[nodiscard]] explicit com_heap_ptr(const std::size_t count)
	    : m_ptr(static_cast<T*>(allocate(count, sizeof(T)))) {
		// empty
	}

	[[nodiscard]] com_heap_ptr(const com_heap_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param ptr Another `com_heap_ptr`.
	[[nodiscard]] constexpr com_heap_ptr(com_heap_ptr&& ptr) noexcept
	    : m_ptr(ptr.release()) {
		// empty
	}

	/// @brief Calls `CoTaskMemFree`.
	~com_heap_ptr() noexcept {
		deallocate(m_ptr);
	}

public:
	com_heap_ptr& operator=(const com_heap_ptr&) = delete;

	/// @brief Transfers ownership.
	/// @param ptr Another `com_heap_ptr`.
	/// @return This instance.
	com_heap_ptr& operator=(com_heap_ptr&& ptr) noexcept {
		deallocate(m_ptr);
		m_ptr = ptr.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	com_heap_ptr& operator=(std::nullptr_t) noexcept {
		deallocate(m_ptr);
		m_ptr = nullptr;
		return *this;
	}

	/// @brief Provides access to the storage location for functions returning pointers as out parameters.
	/// @details The function frees the currently held memory before returning the address.
	/// When a value is assigned to the return value of this function, ownership is transferred to this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ T** operator&() noexcept {
		deallocate(m_ptr);
		m_ptr = nullptr;
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return !!m_ptr;
	}

	/// @brief Use the address of the managed pointer as an exception argument.
	/// @param logData The output target.
	void operator>>(_Inout_ LogData& logData) const {
		// ensure cast to void* for logging of pointer value
		logData << reinterpret_cast<const void* const&>(m_ptr);
	}

	/// @brief Use the address of the managed pointer as an event argument.
	/// @tparam A The type of the log arguments.
	/// @param args The output target.
	template <LogArgs A>
	constexpr void operator>>(_Inout_ A& args) const {
		args << reinterpret_cast<const void* const&>(m_ptr);
	}

public:
	/// @brief Use the `com_heap_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* get() noexcept {
		return m_ptr;
	}

	/// @brief Use the `com_heap_ptr` in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] constexpr _Ret_maybenull_ const T* get() const noexcept {
		return m_ptr;
	}

	/// @brief Release ownership of a pointer.
	/// @note The responsibility for releasing the memory is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] constexpr _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Change the size of the allocated memory block.
	/// @param count The new size of the memory block in number of @a T elements.
	void realloc(const std::size_t count) {
		m_ptr = static_cast<T*>(reallocate(m_ptr, count, sizeof(T)));
	}

	/// @brief Swap two objects.
	/// @param ptr The other `com_heap_ptr`.
	constexpr void swap(com_heap_ptr& ptr) noexcept {
		std::swap(m_ptr, ptr.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] constexpr std::size_t hash() const noexcept {
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
[[nodiscard]] constexpr bool operator==(const com_heap_ptr<T>& ptr, const com_heap_ptr<T>& oth) noexcept {
	return ptr.get() == oth.get();
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator==(const com_heap_ptr<T>& ptr, const T* const p) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param p A native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr holds the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator==(const T* const p, const com_heap_ptr<T>& ptr) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] constexpr bool operator==(const com_heap_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !ptr;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is not set.
template <typename T>
[[nodiscard]] constexpr bool operator==(std::nullptr_t, const com_heap_ptr<T>& ptr) noexcept {
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
[[nodiscard]] constexpr bool operator!=(const com_heap_ptr<T>& ptr, const com_heap_ptr<T>& oth) noexcept {
	return ptr.get() != oth.get();
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const com_heap_ptr<T>& ptr, const T* const p) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_heap_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @param p A native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const T* const p, const com_heap_ptr<T>& ptr) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const com_heap_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !!ptr;
}

/// @brief Allows comparison of `com_heap_ptr` with `nullptr`.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @return `true` if @p ptr is set.
template <typename T>
[[nodiscard]] constexpr bool operator!=(std::nullptr_t, const com_heap_ptr<T>& ptr) noexcept {
	return !!ptr;
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_heap_ptr` object.
/// @param oth Another `com_heap_ptr` object.
template <typename T>
constexpr void swap(m3c::com_heap_ptr<T>& ptr, m3c::com_heap_ptr<T>& oth) noexcept {
	ptr.swap(oth);
}

}  // namespace m3c

/// @brief Specialization of std::hash.
/// @tparam T The type of the managed native pointer.
template <typename T>
struct std::hash<m3c::com_heap_ptr<T>> {
	[[nodiscard]] constexpr std::size_t operator()(const m3c::com_heap_ptr<T>& ptr) const noexcept {
		return ptr.hash();
	}
};

/// @brief Specialization of `fmt::formatter` for a `m3c::com_heap_ptr`.
/// @tparam T The type managed by the `m3c::com_heap_ptr`.
/// @tparam CharT The character type of the formatter.
template <typename T, typename CharT>
struct fmt::formatter<m3c::com_heap_ptr<T>, CharT> : fmt::formatter<const void*, CharT> {
	/// @brief Format the `m3c::com_heap_ptr`.
	/// @param arg A `m3c::com_heap_ptr`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::com_heap_ptr<T>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(arg.get(), ctx);
	}
};
