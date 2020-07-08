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

namespace m3c {

namespace internal {

struct handle_closer final {
	constexpr handle_closer() noexcept = default;

	bool operator()(const HANDLE hNative) const noexcept {
		return CloseHandle(hNative);
	}
};

struct find_closer final {
	constexpr find_closer() noexcept = default;

	bool operator()(const HANDLE hNative) const noexcept {
		return FindClose(hNative);
	}
};

/// @brief A RAII type for windows `HANDLE` values.
/// @tparam Closer The type of the functor to close the handle.
template <typename Closer>
class base_handle final {
public:
	/// @brief Creates an empty instance.
	constexpr base_handle() noexcept = default;

	base_handle(const base_handle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	base_handle(base_handle&& handle) noexcept
		: m_hNative(handle.release()) {
		// empty
	}

	/// @brief Transfer ownership of an existing handle.
	/// @param hNative The native `HANDLE`.
	constexpr base_handle(const HANDLE hNative) noexcept  // NOLINT(google-explicit-constructor): Type should act as a drop-in replacement.
		: m_hNative(hNative) {
		// empty
	}

	/// @brief Calls `CloseHandle`.
	~base_handle() noexcept {
		if (m_hNative != INVALID_HANDLE_VALUE && !m_closer(m_hNative)) {
			LOG_ERROR("Close: {}", lg::LastError());
		}
	}

public:
	base_handle& operator=(const base_handle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	/// @return This instance.
	base_handle& operator=(base_handle&& handle) noexcept {
		if (m_hNative != INVALID_HANDLE_VALUE && !m_closer(m_hNative)) {
			LOG_ERROR("Close: {}", lg::LastError());
		}
		m_hNative = handle.release();
		return *this;
	}

	/// @brief Resets the instance to hold a different value.
	/// @param hNative A native `HANDLE`.
	/// @return This instance.
	base_handle& operator=(HANDLE hNative) noexcept {
		if (m_hNative != INVALID_HANDLE_VALUE && !m_closer(m_hNative)) {
			LOG_ERROR("Close: {}", lg::LastError());
		}
		m_hNative = hNative;
		return *this;
	}

	/// @brief Check if this instance currently manages a valid handle.
	/// @return `true` if the native handle does not equal `INVALID_HANDLE_VALUE`, else `false`.
	[[nodiscard]] explicit operator bool() const noexcept {
		return m_hNative != INVALID_HANDLE_VALUE;
	}

	/// @brief Use this instance in place of the native `HANDLE`.
	/// @return The native `HANDLE`.
	[[nodiscard]] operator HANDLE() const noexcept {  // NOLINT(google-explicit-constructor): Type should act as a drop-in replacement.
		return m_hNative;
	}

public:
	/// @brief Use the `Handle` in place of the raw `HANDLE`.
	/// @return The native handle.
	[[nodiscard]] HANDLE get() noexcept {
		return m_hNative;
	}

	/// @brief Use the `Handle` in place of the raw `HANDLE`.
	/// @return The native handle.
	[[nodiscard]] HANDLE get() const noexcept {
		return m_hNative;
	}

	/// @brief Close the handle.
	void close() {
		if (m_hNative != INVALID_HANDLE_VALUE) {
			if (!m_closer(m_hNative)) {
				LLAMALOG_THROW(m3c::windows_exception(GetLastError(), "Close"));
			}
			m_hNative = INVALID_HANDLE_VALUE;
		}
	}

	/// @brief Release ownership of a handle.
	/// @note The responsibility for releasing the handle is transferred to the caller.
	/// @return The native handle.
	[[nodiscard]] HANDLE release() noexcept {
		const HANDLE hNative = m_hNative;
		m_hNative = INVALID_HANDLE_VALUE;
		return hNative;
	}

	/// @brief Swap two objects.
	/// @param handle The other `BaseHandle`.
	void swap(base_handle& handle) noexcept {
		std::swap(m_hNative, handle.m_hNative);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the native handle.
	[[nodiscard]] std::size_t hash() const noexcept {
		return std::hash<HANDLE>{}(m_hNative);
	}

private:
	HANDLE m_hNative = INVALID_HANDLE_VALUE;  ///< @brief The native handle.
	Closer m_closer;
};

extern template base_handle<handle_closer>;
extern template base_handle<find_closer>;

//
// operator==
//

/// @brief Allows comparison of two `BaseHandle` instances.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p oth.
template <typename Closer>
[[nodiscard]] inline bool operator==(const base_handle<Closer>& handle, const base_handle<Closer>& oth) noexcept {
	return handle.get() == oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator==(const base_handle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() == hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator==(const HANDLE hNative, const base_handle<Closer>& handle) noexcept {
	return handle.get() == hNative;
}


//
// operator!=
//

/// @brief Allows comparison of two `BaseHandle` instances.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
/// @return `true` if @p handle is not the same handle as @p oth.
template <typename Closer>
[[nodiscard]] inline bool operator!=(const base_handle<Closer>& handle, const base_handle<Closer>& oth) noexcept {
	return handle.get() != oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator!=(const base_handle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() != hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator!=(const HANDLE hNative, const base_handle<Closer>& handle) noexcept {
	return handle.get() != hNative;
}

/// @brief Swap function.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `Handle` object.
/// @param oth Another `Handle` object.
template <typename Closer>
inline void swap(base_handle<Closer>& handle, base_handle<Closer>& oth) noexcept {
	handle.swap(oth);
}

}  // namespace internal

/// @brief A RAII type for windows `HANDLE` values.
using handle = internal::base_handle<internal::handle_closer>;

/// @brief A RAII type for windows `HANDLE` values used in the calls `FindFirstFile` etc.
using find_handle = internal::base_handle<internal::find_closer>;

}  // namespace m3c

/// @brief Specialization of std::hash.
/// @tparam Closer The type of the functor to close the handle.
template <typename Closer>
struct std::hash<m3c::internal::base_handle<Closer>> {
	[[nodiscard]] size_t operator()(const m3c::internal::base_handle<Closer>& handle) const noexcept {
		return handle.hash();
	}
};
