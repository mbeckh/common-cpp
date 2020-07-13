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

#include <windows.h>

#include <cstddef>
#include <functional>

namespace m3c {

namespace internal {

/// @brief Strategy to close regular handles.
struct HandleCloser final {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	constexpr HandleCloser() noexcept = default;

	/// @brief Close the handle.
	/// @param hNative The handle to close.
	/// @return `true` if the operation was successful, else `false`.
	bool operator()(const HANDLE hNative) const noexcept {
		return CloseHandle(hNative);
	}
};

/// @brief Strategy to close handles which require a call to `FindClose`.
struct FindCloser final {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	constexpr FindCloser() noexcept = default;

	/// @brief Close the handle.
	/// @param hNative The handle to close.
	/// @return `true` if the operation was successful, else `false`.
	bool operator()(const HANDLE hNative) const noexcept {
		return FindClose(hNative);
	}
};

/// @brief A RAII type for windows `HANDLE` values.
/// @tparam Closer The type of the functor to close the handle.
template <typename Closer>
class BaseHandle final {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
public:
	/// @brief Creates an empty instance.
	constexpr BaseHandle() noexcept = default;

	BaseHandle(const BaseHandle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	BaseHandle(BaseHandle&& handle) noexcept
		: m_hNative(handle.release()) {
		// empty
	}

	/// @brief Transfer ownership of an existing handle.
	/// @param hNative The native `HANDLE`.
	constexpr BaseHandle(const HANDLE hNative) noexcept  // NOLINT(google-explicit-constructor): Type should act as a drop-in replacement.
		: m_hNative(hNative) {
		// empty
	}

	/// @brief Calls `CloseHandle`.
	~BaseHandle() noexcept {
		if (m_hNative != INVALID_HANDLE_VALUE && !m_closer(m_hNative)) {
			LOG_ERROR("Close: {}", lg::LastError());
		}
	}

public:
	BaseHandle& operator=(const BaseHandle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	/// @return This instance.
	BaseHandle& operator=(BaseHandle&& handle) noexcept {
		if (m_hNative != INVALID_HANDLE_VALUE && !m_closer(m_hNative)) {
			LOG_ERROR("Close: {}", lg::LastError());
		}
		m_hNative = handle.release();
		return *this;
	}

	/// @brief Resets the instance to hold a different value.
	/// @param hNative A native `HANDLE`.
	/// @return This instance.
	BaseHandle& operator=(HANDLE hNative) noexcept {
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
	void swap(BaseHandle& handle) noexcept {
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

extern template class BaseHandle<HandleCloser>;
extern template class BaseHandle<FindCloser>;

//
// operator==
//

/// @brief Allows comparison of two `BaseHandle` instances.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p oth.
template <typename Closer>
[[nodiscard]] inline bool operator==(const BaseHandle<Closer>& handle, const BaseHandle<Closer>& oth) noexcept {
	return handle.get() == oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator==(const BaseHandle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() == hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator==(const HANDLE hNative, const BaseHandle<Closer>& handle) noexcept {
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
[[nodiscard]] inline bool operator!=(const BaseHandle<Closer>& handle, const BaseHandle<Closer>& oth) noexcept {
	return handle.get() != oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator!=(const BaseHandle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() != hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the functor to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] inline bool operator!=(const HANDLE hNative, const BaseHandle<Closer>& handle) noexcept {
	return handle.get() != hNative;
}

/// @brief Swap function.
/// @tparam Closer The type of the functor to close the handle.
/// @param handle A `Handle` object.
/// @param oth Another `Handle` object.
template <typename Closer>
inline void swap(BaseHandle<Closer>& handle, BaseHandle<Closer>& oth) noexcept {
	handle.swap(oth);
}

}  // namespace internal

/// @brief A RAII type for windows `HANDLE` values.
using Handle = internal::BaseHandle<internal::HandleCloser>;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief A RAII type for windows `HANDLE` values used in the calls `FindFirstFile` etc.
using FindHandle = internal::BaseHandle<internal::FindCloser>;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

}  // namespace m3c

/// @brief Specialization of std::hash.
/// @tparam Closer The type of the functor to close the handle.
template <typename Closer>
struct std::hash<m3c::internal::BaseHandle<Closer>> {
	[[nodiscard]] size_t operator()(const m3c::internal::BaseHandle<Closer>& handle) const noexcept {
		return handle.hash();
	}
};
