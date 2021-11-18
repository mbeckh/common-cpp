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

#include "m3c/LogArgs.h"
#include "m3c/LogData.h"

#include <fmt/format.h>

#include <windows.h>

#include <cstddef>
#include <functional>
#include <utility>

namespace m3c {

namespace internal {

/// @brief Strategy to close regular handles.
struct HandleCloser final {
	HandleCloser() = delete;

	/// @brief Close the handle throwing an exception in case of errors.
	/// @param hNative The handle to close.
	static void Close(HANDLE hNative);

	/// @brief Closes the handle, silently ignoring any exceptions.
	/// @param hNative The handle to close.
	static void CloseSilently(HANDLE hNative) noexcept;
};


/// @brief Strategy to close handles which require a call to `FindClose`.
struct FindCloser final {
	FindCloser() = delete;

	/// @brief Close the handle throwing an exception in case of errors.
	/// @param hNative The handle to close.
	static void Close(HANDLE hNative);

	/// @brief Closes the handle, silently ignoring any exceptions.
	/// @param hNative The handle to close.
	static void CloseSilently(HANDLE hNative) noexcept;
};


/// @brief A RAII type for windows `HANDLE` values.
/// @tparam Closer The type of the strategy to close the handle.
template <typename Closer>
class BaseHandle final {
public:
	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr BaseHandle() noexcept = default;

	/// @brief Transfer ownership of an existing handle.
	/// @param hNative The native `HANDLE`.
	[[nodiscard]] constexpr BaseHandle(HANDLE hNative) noexcept  // NOLINT(google-explicit-constructor): Type should act as a drop-in replacement.
	    : m_hNative(hNative) {
		// empty
	}

	BaseHandle(const BaseHandle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	[[nodiscard]] constexpr BaseHandle(BaseHandle&& handle) noexcept
	    : m_hNative(handle.release()) {
		// empty
	}

	/// @brief Calls `CloseHandle`.
	~BaseHandle() noexcept {
		if (m_hNative != kInvalid) {
			Closer::CloseSilently(m_hNative);
		}
	}

public:
	BaseHandle& operator=(const BaseHandle&) = delete;

	/// @brief Transfers ownership.
	/// @param handle Another `BaseHandle`.
	/// @return This instance.
	constexpr BaseHandle& operator=(BaseHandle&& handle) noexcept {
		if (m_hNative != kInvalid) {
			Closer::CloseSilently(m_hNative);
		}
		m_hNative = handle.release();
		return *this;
	}

	/// @brief Resets the instance to hold a different value.
	/// @param hNative A native `HANDLE`.
	/// @return This instance.
	constexpr BaseHandle& operator=(HANDLE hNative) noexcept {
		if (m_hNative != kInvalid) {
			Closer::CloseSilently(m_hNative);
		}
		m_hNative = hNative;
		return *this;
	}

	/// @brief Check if this instance currently manages a valid handle.
	/// @return `true` if the native handle does not equal `INVALID_HANDLE_VALUE`, else `false`.
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return m_hNative != kInvalid;
	}

	/// @brief Use this instance in place of the native `HANDLE`.
	/// @return The native `HANDLE`.
	[[nodiscard]] constexpr operator HANDLE() const noexcept {  // NOLINT(google-explicit-constructor): Type should act as a drop-in replacement.
		return m_hNative;
	}

	/// @brief Use the value of the `HANDLE` as an exception argument.
	/// @param logData The output target.
	void operator>>(_Inout_ LogData& logData) const {
		logData << reinterpret_cast<const void* const&>(m_hNative);
	}

	/// @brief Use value of the `HANDLE` as an event argument.
	/// @tparam A The type of the log arguments.
	/// @param args The output target.
	template <LogArgs A>
	constexpr void operator>>(_Inout_ A& args) const {
		args << reinterpret_cast<const void* const&>(m_hNative);
	}

public:
	/// @brief Use the `BaseHandle` in place of the raw `HANDLE`.
	/// @return The native handle.
	[[nodiscard]] constexpr HANDLE get() const noexcept {
		return m_hNative;
	}

	/// @brief Close the handle.
	constexpr void close() {
		if (m_hNative != kInvalid) {
			Closer::Close(m_hNative);
			m_hNative = kInvalid;
		}
	}

	/// @brief Release ownership of a handle.
	/// @note The responsibility for releasing the handle is transferred to the caller.
	/// @return The native handle.
	[[nodiscard]] constexpr HANDLE release() noexcept {
		HANDLE hNative = m_hNative;
		m_hNative = kInvalid;
		return hNative;
	}

	/// @brief Swap two objects.
	/// @param handle The other `BaseHandle`.
	constexpr void swap(BaseHandle& handle) noexcept {
		std::swap(m_hNative, handle.m_hNative);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the native handle.
	[[nodiscard]] constexpr std::size_t hash() const noexcept {
		return std::hash<HANDLE>{}(m_hNative);
	}

private:
	/// @brief The invalid handle value.
	static inline const HANDLE kInvalid = INVALID_HANDLE_VALUE;  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast, performance-no-int-to-ptr): Constant definition uses cast.

	/// @brief The native handle.
	HANDLE m_hNative = kInvalid;
};

extern template class BaseHandle<HandleCloser>;
extern template class BaseHandle<FindCloser>;

//
// operator==
//

/// @brief Allows comparison of two `BaseHandle` instances.
/// @tparam Closer The type of the strategy to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p oth.
template <typename Closer>
[[nodiscard]] constexpr bool operator==(const BaseHandle<Closer>& handle, const BaseHandle<Closer>& oth) noexcept {
	return handle.get() == oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the strategy to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] constexpr bool operator==(const BaseHandle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() == hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the strategy to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] constexpr bool operator==(const HANDLE hNative, const BaseHandle<Closer>& handle) noexcept {
	return handle.get() == hNative;
}


//
// operator!=
//

/// @brief Allows comparison of two `BaseHandle` instances.
/// @tparam Closer The type of the strategy to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
/// @return `true` if @p handle is not the same handle as @p oth.
template <typename Closer>
[[nodiscard]] constexpr bool operator!=(const BaseHandle<Closer>& handle, const BaseHandle<Closer>& oth) noexcept {
	return handle.get() != oth.get();
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the strategy to close the handle.
/// @param handle A `BaseHandle` object.
/// @param hNative A native handle.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] constexpr bool operator!=(const BaseHandle<Closer>& handle, const HANDLE hNative) noexcept {
	return handle.get() != hNative;
}

/// @brief Allows comparison of `BaseHandle` with native `HANDLE`.
/// @tparam Closer The type of the strategy to close the handle.
/// @param hNative A native handle.
/// @param handle A `BaseHandle` object.
/// @return `true` if @p handle is not the same handle as @p hNative.
template <typename Closer>
[[nodiscard]] constexpr bool operator!=(const HANDLE hNative, const BaseHandle<Closer>& handle) noexcept {
	return handle.get() != hNative;
}

/// @brief Swap function.
/// @tparam Closer The type of the strategy to close the handle.
/// @param handle A `BaseHandle` object.
/// @param oth Another `BaseHandle` object.
template <typename Closer>
constexpr void swap(BaseHandle<Closer>& handle, BaseHandle<Closer>& oth) noexcept {
	handle.swap(oth);
}

}  // namespace internal


/// @brief A RAII type for windows `HANDLE` values.
using Handle = internal::BaseHandle<internal::HandleCloser>;

/// @brief A RAII type for windows `HANDLE` values used in the calls `FindFirstFile` etc.
using FindHandle = internal::BaseHandle<internal::FindCloser>;

// assert no size overhead
static_assert(sizeof(Handle) == sizeof(HANDLE));

// assert no size overhead
static_assert(sizeof(FindHandle) == sizeof(HANDLE));

}  // namespace m3c


/// @brief Specialization of std::hash.
/// @tparam Closer The type of the strategy to close the handle.
template <typename Closer>
struct std::hash<m3c::internal::BaseHandle<Closer>> {
	[[nodiscard]] constexpr std::size_t operator()(const m3c::internal::BaseHandle<Closer>& handle) const noexcept {
		return handle.hash();
	}
};

/// @brief Specialization of `fmt::formatter` for a `BaseHandle`.
/// @tparam Closer The type of the strategy to close the handle.
/// @tparam CharT The character type of the string.
template <typename Closer, typename CharT>
struct fmt::formatter<m3c::internal::BaseHandle<Closer>, CharT> : public fmt::formatter<const void*, CharT> {
	/// @brief Format the handle.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A handle.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::internal::BaseHandle<Closer>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(static_cast<const void*>(arg.get()), ctx);
	}
};
