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

#include <m3c/exception.h>

#include <llamalog/llamalog.h>

#include <rpc.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace m3c {
namespace internal {

/// @brief A generic template for a RAII type for memory managed using `RpcStringFree`.
/// @tparam T The type of the managed object, either `RPC_CSTR` or `RPC_WSTR`.
template <typename T>
class basic_rpc_string final {
public:
	/// @brief Creates a new empty instance.
	constexpr basic_rpc_string() noexcept = default;

	basic_rpc_string(const basic_rpc_string&) = delete;

	/// @brief Transfers ownership.
	/// @param str Another `basic_rpc_string`.
	basic_rpc_string(basic_rpc_string&& str) noexcept
		: m_ptr(str.release()) {
		// empty
	}

	/// @brief Creates an empty instance.
	constexpr basic_rpc_string(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Releases any memory using `RpcStringFree`.
	inline ~basic_rpc_string() noexcept {
		const RPC_STATUS status = destroy();
		if (status != RPC_S_OK) {
			// destructor SHOULD NOT throw
			LOG_ERROR("RpcStringFree: {}", lg::error_code{status});
		}
	}

public:
	basic_rpc_string& operator=(const basic_rpc_string&) = delete;

	/// @brief Transfers ownership.
	/// @param p Another `basic_rpc_string`.
	/// @return This instance.
	basic_rpc_string& operator=(basic_rpc_string&& p) noexcept {
		const RPC_STATUS status = destroy();
		if (status != RPC_S_OK) {
			// move operator SHOULD NOT throw
			LOG_ERROR("RpcStringFree: {}", lg::error_code{status});
		}
		m_ptr = p.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	basic_rpc_string& operator=(std::nullptr_t) {
		const RPC_STATUS status = destroy();
		if (status != RPC_S_OK) {
			LLAMALOG_THROW(rpc_exception(status), "RpcStringFree: {}", lg::error_code{status});
		}
		m_ptr = nullptr;
		return *this;
	}

	/// @brief Provides access to the storage location for functions returning pointers as out parameters.
	/// @details The function frees the currently held memory before returning the address.
	/// When a value is assigned to the return value of this function, ownership is transferred to this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ T* operator&() {
		const RPC_STATUS status = destroy();
		if (status != RPC_S_OK) {
			LLAMALOG_THROW(rpc_exception(status), "RpcStringFree: {}", lg::error_code{status});
		}
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] explicit operator bool() const noexcept {
		return m_ptr;
	}

public:
	/// @brief Use the RPC string in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T get() const noexcept {
		return m_ptr;
	}

	/// @brief Access the internal string as a native pointer.
	/// @return The native `char` or `wchar_t` pointer.
	[[nodiscard]] _Ret_maybenull_ const std::conditional_t<std::is_same_v<T, RPC_CSTR>, char, wchar_t>* as_native() const noexcept {
		return reinterpret_cast<const std::conditional_t<std::is_same_v<T, RPC_CSTR>, char, wchar_t>*>(m_ptr);
	}

	/// @brief Releases ownership of the pointer.
	/// @note The responsibility for deleting the object is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T release() noexcept {
		T const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param str The other `basic_rpc_string`.
	void swap(basic_rpc_string& str) noexcept {
		std::swap(m_ptr, str.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] std::size_t hash() const noexcept {
		return std::hash<void*>{}(m_ptr);
	}

private:
	/// @brief Release the memory using `RpcStringFreeA` or `RpcStringFreeW`.
	/// @return A `RPC_STATUS` code signaling success or failure.
	[[nodiscard]] RPC_STATUS destroy() noexcept {
		if constexpr (std::is_same_v<T, RPC_CSTR>) {
			return RpcStringFreeA(&m_ptr);
		} else if constexpr (std::is_same_v<T, RPC_WSTR>) {
			return RpcStringFreeW(&m_ptr);
		} else {
			__assume(0);
		}
	}

private:
	T m_ptr = nullptr;  ///< @brief The native pointer.
};


//
// operator==
//

/// @brief Allows comparison of two `basic_rpc_string` instances.
/// @tparam T The type of the RPC strings.
/// @param str A `basic_rpc_string` object.
/// @param oth Another `basic_rpc_string` object.
/// @return `true` if @p str points to the same address as @p oth.
template <typename T>
[[nodiscard]] inline bool operator==(const basic_rpc_string<T>& str, const basic_rpc_string<T>& oth) {
	return str.get() == oth.get();
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @param p A native pointer.
/// @return `true` if @p str holds the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator==(const basic_rpc_string<T>& str, T p) {
	return str.get() == p;
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param p A native pointer.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str holds the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator==(T p, const basic_rpc_string<T>& str) {
	return str.get() == p;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is not set.
template <typename T>
[[nodiscard]] inline bool operator==(const basic_rpc_string<T>& str, std::nullptr_t) {
	return !str;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is not set.
template <typename T>
[[nodiscard]] inline bool operator==(std::nullptr_t, const basic_rpc_string<T>& str) {
	return !str;
}


//
// operator!=
//

/// @brief Allows comparison of two `basic_rpc_string` instances.
/// @tparam T The type of the RPC strings.
/// @param str A `basic_rpc_string` object.
/// @param oth Another `basic_rpc_string` object.
/// @return `true` if @p str does not point to the same address as @p oth.
template <typename T>
[[nodiscard]] inline bool operator!=(const basic_rpc_string<T>& str, const basic_rpc_string<T>& oth) {
	return str.get() != oth.get();
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @param p A native pointer.
/// @return `true` if @p str does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator!=(const basic_rpc_string<T>& str, T p) {
	return str.get() != p;
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param p A native pointer.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] inline bool operator!=(T p, const basic_rpc_string<T>& str) {
	return str.get() != p;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is set.
template <typename T>
[[nodiscard]] inline bool operator!=(const basic_rpc_string<T>& str, std::nullptr_t) {
	return !!str;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is set.
template <typename T>
[[nodiscard]] inline bool operator!=(std::nullptr_t, const basic_rpc_string<T>& str) {
	return !!str;
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param str A `basic_rpc_string` object.
/// @param oth Another `basic_rpc_string` object.
template <typename T>
inline void swap(m3c::internal::basic_rpc_string<T>& str, m3c::internal::basic_rpc_string<T>& oth) noexcept {
	str.swap(oth);
}

}  // namespace internal

/// @brief A typedef for `char`-based RPC strings.
using rpc_string = internal::basic_rpc_string<RPC_CSTR>;

/// @brief A typedef for wide character RPC strings.
using rpc_wstring = internal::basic_rpc_string<RPC_WSTR>;

}  // namespace m3c

/// @brief Specialization of std::hash.
template <typename T>
struct std::hash<m3c::internal::basic_rpc_string<T>> {
	[[nodiscard]] std::size_t operator()(const m3c::internal::basic_rpc_string<T>& str) const noexcept {
		return str.hash();
	}
};
