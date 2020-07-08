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

#include <rpc.h>
#include <windows.h>

#include <cstdint>
#include <exception>
#include <system_error>
#include <utility>

namespace m3c {

/// @brief The singleton for windows errors returned from `GetLastError`.
/// @return The singleton for this category.
const std::error_category& win32_category() noexcept;

/// @brief The singleton for RPC errors returned as `RPC_STATUS`.
/// @return The singleton for this category.
const std::error_category& rpc_category() noexcept;

/// @brief The singleton for `HRESULT` errors.
/// @return The singleton for this category.
const std::error_category& com_category() noexcept;


/// @brief An exception thrown for errors returned by `GetLastError`.
class windows_exception : public lg::system_error {
public:
	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	explicit windows_exception(const DWORD errorCode)
		: windows_exception(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	/// @param message An error message.
	windows_exception(const DWORD errorCode, _In_opt_z_ const char* const message)
		: lg::system_error(errorCode, win32_category(), message) {
		// empty
	}
};


/// @brief An exception thrown for errors returned from RPC functions.
class rpc_exception : public lg::system_error {
public:
	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	explicit rpc_exception(const RPC_STATUS errorCode)
		: rpc_exception(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	/// @param message An error message.
	rpc_exception(const RPC_STATUS errorCode, _In_opt_z_ const char* const message)
		: lg::system_error(errorCode, rpc_category(), message) {
		// empty
	}
};


/// @brief An exception thrown for COM errors.
class com_exception : public lg::system_error {
public:
	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	explicit com_exception(const HRESULT hr)
		: com_exception(hr, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	/// @param message An error message.
	com_exception(const HRESULT hr, _In_opt_z_ const char* const message)
		: lg::system_error(hr, com_category(), message) {
		// empty
	}
};

class com_invalid_argument_exception : public com_exception {
public:
	com_invalid_argument_exception()
		: com_invalid_argument_exception(nullptr) {
		// empty
	}

	/// @brief Creates the exception with `HRESULT` value of `E_INVALIDARG`.
	/// @param arg The name of the invalid argument.
	explicit com_invalid_argument_exception(_In_opt_z_ const char* const arg)
		: com_invalid_argument_exception(E_INVALIDARG, arg) {
		// empty
	}

	explicit com_invalid_argument_exception(const HRESULT hr)
		: com_invalid_argument_exception(hr, nullptr) {
		// empty
	}

	com_invalid_argument_exception(const HRESULT hr, _In_opt_z_ const char* const arg)
		: com_exception(hr, arg) {
		// empty
	}
};

namespace internal {

/// @brief Conversion function if the global log level is set to `DEBUG` or lower.
/// @remarks The function is always compiled to be available for testing.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at the respective level.
/// @return The HRESULT as described for `#ExceptionToHRESULT`.
HRESULT ExceptionToHRESULT_DEBUG(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*thunk)(_In_opt_ void*, llamalog::Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* log) noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief Conversion function if the global log level is set higher than `DEBUG` and up to `ERROR`.
/// @remarks The function is always compiled to be available for testing.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at level `llamalog::Priority::kError`.
/// @return The HRESULT as described for `#ExceptionToHRESULT`.
HRESULT ExceptionToHRESULT_ERROR(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*thunk)(_In_opt_ void*, llamalog::Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* log) noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief Conversion function if the global log level is set to `FATAL`.
/// @remarks The function is always compiled to be available for testing.
/// @return The HRESULT as described for `#ExceptionToHRESULT`.
HRESULT ExceptionToHRESULT_FATAL() noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.


/// @brief Helper for `#ExceptionToHRESULT` to make testing easier.
/// @details @internal The functions `#CallExceptionToHRESULT_DEBUG`, `#CallExceptionToHRESULT_ERROR` and
/// `#CallExceptionToHRESULT_FATAL` MUST share the same signature for use in `#ExceptionToHRESULT`.
/// @tparam T The types of the message arguments.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param message The pattern for the log message. The exception is added as the **last** argument.
/// @param args Arguments to append to log messages.
/// @return The `HRESULT` value.
/// @copyright The function uses a trick that allows calling a binding lambda to use a function pointer. It is published
/// by Joaquín M López Muñoz at http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html.
template <typename... T>
[[nodiscard]] HRESULT CallExceptionToHRESULT_DEBUG(_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, _In_z_ const char* const message, T&&... args) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	auto log = [message, args...](const llamalog::Priority priority, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
		llamalog::Log(priority, file, line, function, message, std::forward<T>(args)..., e);
	};
	auto thunk = [](_In_ void* const p, const llamalog::Priority priority, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
		(*static_cast<decltype(log)*>(p))(priority, file, line, function, e);
	};
	return ExceptionToHRESULT_DEBUG(file, line, function, thunk, &log);
}

/// @brief Helper for `#ExceptionToHRESULT` to make testing easier.
/// @details @internal The functions `#CallExceptionToHRESULT_DEBUG`, `#CallExceptionToHRESULT_ERROR` and
/// `#CallExceptionToHRESULT_FATAL` MUST share the same signature for use in `#ExceptionToHRESULT`.
/// @tparam T The types of the message arguments.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param message The pattern for the log message. The exception is added as the **last** argument.
/// @param args Arguments to append to log messages.
/// @return The `HRESULT` value.
/// @copyright The function uses a trick that allows calling a binding lambda to use a function pointer. It is published
/// by Joaquín M López Muñoz at http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html.
template <typename... T>
[[nodiscard]] HRESULT CallExceptionToHRESULT_ERROR(_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, _In_z_ const char* const message, T&&... args) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	auto log = [message, args...](_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
		llamalog::Log(llamalog::Priority::kError, file, line, function, message, std::forward<T>(args)..., e);
	};
	auto thunk = [](_In_ void* const p, llamalog::Priority /* priority */, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
		(*static_cast<decltype(log)*>(p))(file, line, function, e);
	};
	return ExceptionToHRESULT_ERROR(file, line, function, thunk, &log);
}

/// @brief Helper for `#ExceptionToHRESULT` to make testing easier.
/// @details @internal The functions `#CallExceptionToHRESULT_DEBUG`, `#CallExceptionToHRESULT_ERROR` and
/// `#CallExceptionToHRESULT_FATAL` MUST share the same signature for use in `#ExceptionToHRESULT`.
/// @tparam T The types of the message arguments.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param message The pattern for the log message. The exception is added as the **last** argument.
/// @param args Arguments to append to log messages.
/// @return The `HRESULT` value.
template <typename... T>
[[nodiscard]] HRESULT CallExceptionToHRESULT_FATAL([[maybe_unused]] _In_z_ const char* const file, [[maybe_unused]] const std::uint32_t line, [[maybe_unused]] _In_z_ const char* const function, [[maybe_unused]] _In_z_ const char* const message, [[maybe_unused]] T&&... args) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return ExceptionToHRESULT_FATAL();
}

}  // namespace internal

/// @brief Convert any exception to a `HRESULT`.
/// @details The `HRESULT` is converted from `#SystemError::code()` if available, is set to `E_OUTOFMEMORY` for
/// `std::bad_alloc` and `E_FAIL` for any other type of exception.
/// @note The function MUST be called from within a catch block.
/// @tparam T The types of the message arguments.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param message The pattern for the log message. The exception is added as the **last** argument.
/// @param args Arguments to append to log messages.
/// @return The `HRESULT` value.
template <typename... T>
[[nodiscard]] HRESULT ExceptionToHRESULT(_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, _In_z_ const char* const message, T&&... args) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
		internal::CallExceptionToHRESULT_DEBUG
#elif defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO)
		internal::CallExceptionToHRESULT_ERROR
#else
		internal::CallExceptionToHRESULT_FATAL
#endif
		(file, line, function, message, std::forward<T>(args)...);
}

/// @brief Convert any exception to a `HRESULT`.
/// @details The `HRESULT` is converted from `#SystemError::code()` if available, is set to `E_OUTOFMEMORY` for
/// `std::bad_alloc` and `E_FAIL` for any other type of exception.
/// @note The function MUST be called from within a catch block.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @return The `HRESULT` value.
[[nodiscard]] inline HRESULT ExceptionToHRESULT(_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	return ExceptionToHRESULT(file, line, function, "{}");
}

}  // namespace m3c


//
// Macros
//

/// @brief Throw `com_exception` in case of COM failures.
/// @param result_ The `HRSESULT` value to check.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define M3C_HRESULT_TO_EXCEPTION(result_, ...)                \
	if (const HRESULT hr_ = result_; FAILED(hr_)) {           \
		LLAMALOG_THROW(m3c::com_exception(hr_), __VA_ARGS__); \
	}

/// @brief Alias for `#M3C_HRESULT_TO_EXCEPTION'.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define HRESULT_TO_EXCEPTION M3C_HRESULT_TO_EXCEPTION

/// @brief Alias for `#M3C_HRESULT_TO_EXCEPTION'.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define COM_HR M3C_HRESULT_TO_EXCEPTION

/// @brief Calls m3c::ExceptionToHRESULT with the current values for `__FILE__`, `__LINE__` and `__func__`.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define M3C_EXCEPTION_TO_HRESULT(...)                                          \
	[&](const char* const function) {                                          \
		constexpr const char* file = llamalog::GetFilename(__FILE__);          \
		return m3c::ExceptionToHRESULT(file, __LINE__, function, __VA_ARGS__); \
	}(__func__)

/// @brief Alias for `#M3C_EXCEPTION_TO_HRESULT`.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define EXCEPTION_TO_HRESULT M3C_EXCEPTION_TO_HRESULT

/// @brief Alias for `#M3C_EXCEPTION_TO_HRESULT`.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define AS_HRESULT M3C_EXCEPTION_TO_HRESULT
