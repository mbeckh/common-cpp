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

#include <m3c/type_traits.h>

#include <rpc.h>
#include <sal.h>

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace m3c {

/// @brief A generic template for a RAII type for memory managed using `RpcStringFree`.
/// @tparam T The character type of the string.
template <AnyOf<char, wchar_t> T>
class basic_rpc_string final {
public:
	/// @brief The character type of the string.
	using char_type = T;
	/// @brief The RPC character type of the string.
	using rpc_str_type = std::conditional_t<std::is_same_v<char_type, char>, RPC_CSTR, RPC_WSTR>;

public:
	/// @brief Creates a new empty instance.
	[[nodiscard]] constexpr basic_rpc_string() noexcept = default;

	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr explicit basic_rpc_string(std::nullptr_t) noexcept {
		// empty
	}

	[[nodiscard]] basic_rpc_string(const basic_rpc_string&) = delete;

	/// @brief Transfers ownership.
	/// @param str Another `basic_rpc_string`.
	[[nodiscard]] basic_rpc_string(basic_rpc_string&& str) noexcept;

	/// @brief Releases any memory using `RpcStringFree`.
	~basic_rpc_string() noexcept;

public:
	basic_rpc_string& operator=(const basic_rpc_string&) = delete;

	/// @brief Transfers ownership.
	/// @param p Another `basic_rpc_string`.
	/// @return This instance.
	basic_rpc_string& operator=(basic_rpc_string&& p) noexcept;

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	basic_rpc_string& operator=(std::nullptr_t);

	/// @brief Provides access to the storage location for functions returning pointers as out parameters.
	/// @details The function frees the currently held memory before returning the address.
	/// When a value is assigned to the return value of this function, ownership is transferred to this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ rpc_str_type* operator&();

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return !!m_ptr;
	}

public:
	/// @brief Use the RPC string in place of the raw type.
	/// @return The native pointer.
	[[nodiscard]] constexpr _Ret_maybenull_ rpc_str_type get() const noexcept {
		return m_ptr;
	}

	/// @brief Access the internal string as a native pointer.
	/// @details If no RPC string is stored, an empty string is returned.
	/// @return The native `char` or `wchar_t` pointer.
	[[nodiscard]] constexpr _Ret_z_ const char_type* c_str() const noexcept {
		if (m_ptr) {
			[[likely]];
			return reinterpret_cast<const char_type*>(m_ptr);
		}
		return SelectString<char_type>("", L"");
	}

	/// @brief Get the number of characters in this string.
	/// @return The number of characters (not including the terminating null character).
	[[nodiscard]] constexpr std::size_t size() const noexcept {
		if (m_ptr) {
			[[likely]];
			return std::char_traits<char_type>::length(c_str());
		}
		return 0;
	}

	/// @brief Releases ownership of the pointer.
	/// @note The responsibility for deleting the object is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] constexpr _Ret_maybenull_ rpc_str_type release() noexcept {
		rpc_str_type const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param str The other `basic_rpc_string`.
	constexpr void swap(basic_rpc_string& str) noexcept {
		std::swap(m_ptr, str.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] constexpr std::size_t hash() const noexcept {
		return std::hash<void*>{}(m_ptr);
	}

private:
	/// @brief Release the memory using `RpcStringFreeA` or `RpcStringFreeW`.
	/// @return A `RPC_STATUS` code signaling success or failure.
	[[nodiscard]] RPC_STATUS destroy() noexcept;

private:
	rpc_str_type m_ptr = nullptr;  ///< @brief The native pointer.
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
[[nodiscard]] constexpr bool operator==(const basic_rpc_string<T>& str, const basic_rpc_string<T>& oth) noexcept {
	return str.get() == oth.get();
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @param p A native pointer.
/// @return `true` if @p str holds the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator==(const basic_rpc_string<T>& str, typename basic_rpc_string<T>::rpc_str_type p) noexcept {
	return str.get() == p;
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param p A native pointer.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str holds the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator==(typename basic_rpc_string<T>::rpc_str_type p, const basic_rpc_string<T>& str) noexcept {
	return str.get() == p;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is not set.
template <typename T>
[[nodiscard]] constexpr bool operator==(const basic_rpc_string<T>& str, std::nullptr_t) noexcept {
	return !str;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is not set.
template <typename T>
[[nodiscard]] constexpr bool operator==(std::nullptr_t, const basic_rpc_string<T>& str) noexcept {
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
[[nodiscard]] constexpr bool operator!=(const basic_rpc_string<T>& str, const basic_rpc_string<T>& oth) noexcept {
	return str.get() != oth.get();
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @param p A native pointer.
/// @return `true` if @p str does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const basic_rpc_string<T>& str, typename basic_rpc_string<T>::rpc_str_type p) noexcept {
	return str.get() != p;
}

/// @brief Allows comparison of `basic_rpc_string` with native pointers.
/// @tparam T The type of the RPC string.
/// @param p A native pointer.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str does not hold the same pointer as @p p.
template <typename T>
[[nodiscard]] constexpr bool operator!=(typename basic_rpc_string<T>::rpc_str_type p, const basic_rpc_string<T>& str) noexcept {
	return str.get() != p;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is set.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const basic_rpc_string<T>& str, std::nullptr_t) noexcept {
	return !!str;
}

/// @brief Allows comparison of `basic_rpc_string` with `nullptr`.
/// @tparam T The type of the RPC string.
/// @param str A `basic_rpc_string` object.
/// @return `true` if @p str is set.
template <typename T>
[[nodiscard]] constexpr bool operator!=(std::nullptr_t, const basic_rpc_string<T>& str) noexcept {
	return !!str;
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param str A `basic_rpc_string` object.
/// @param oth Another `basic_rpc_string` object.
template <typename T>
constexpr void swap(basic_rpc_string<T>& str, basic_rpc_string<T>& oth) noexcept {
	str.swap(oth);
}

template <>
RPC_STATUS basic_rpc_string<char>::destroy() noexcept;
template <>
RPC_STATUS basic_rpc_string<wchar_t>::destroy() noexcept;

extern template class basic_rpc_string<char>;
extern template class basic_rpc_string<wchar_t>;

/// @brief A typedef for `char`-based RPC strings.
using rpc_string = basic_rpc_string<char>;

/// @brief A typedef for wide character RPC strings.
using rpc_wstring = basic_rpc_string<wchar_t>;

}  // namespace m3c


/// @brief Specialization of std::hash.
template <typename T>
struct std::hash<m3c::basic_rpc_string<T>> {
	[[nodiscard]] constexpr std::size_t operator()(const m3c::basic_rpc_string<T>& str) const noexcept {
		return str.hash();
	}
};
