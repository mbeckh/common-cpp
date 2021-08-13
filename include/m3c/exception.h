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

#include "Log.h"
#include "LogData.h"
#include "source_location.h"

#include <windows.h>
#include <evntprov.h>
#include <rpc.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace m3c {

namespace internal {

struct Default final {
};

}  // namespace internal

namespace evt {

constexpr internal::Default Default;

}  // namespace evt

#if 0
/// @brief The singleton for windows errors returned from `GetLastError`.
/// @return The singleton for this category.
const std::error_category& win32_category() noexcept;

/// @brief The singleton for RPC errors returned as `RPC_STATUS`.
/// @return The singleton for this category.
const std::error_category& rpc_category() noexcept;

/// @brief The singleton for `HRESULT` errors.
/// @return The singleton for this category.
const std::error_category& com_category() noexcept;
#endif

/// @brief A helper class to transfer system errors.
/// @details Other than `std::system_error` the message is not formatted until the time the `LogLine` is written or `#system_error::what()` is called.
class system_error : public std::runtime_error {  // NOLINT(readability-identifier-naming): Interface is compatible to std::system_error.
public:
	system_error(const system_error&) noexcept = default;  ///< @defaultconstructor
	system_error(system_error&&) noexcept = default;       ///< @defaultconstructor

	/// @brief Creates a new instance for the provided error code and category.
	/// @param code The error code.
	/// @param category The error category.
	/// @param message The error message.
	system_error(int code, const std::error_category& category, const std::string& message);
	system_error(int code, const std::error_category& category, _In_opt_z_ const char* __restrict message);
	~system_error() noexcept = default;

public:
	system_error& operator=(const system_error&) noexcept = default;  ///< @defaultoperator
	system_error& operator=(system_error&&) noexcept = default;       ///< @defaultoperator

public:
	/// @brief Get the system error code (as in `std::system_error::code()`).
	/// @result The error code.
	[[nodiscard]] const std::error_code& code() const noexcept {  // NOLINT(readability-identifier-naming): Like code() from std::system_error.
		return m_code;
	}

	/// @brief Create the formatted error message.
	/// @return The formatted error message.
	[[nodiscard]] _Ret_z_ const char* what() const noexcept override;

	[[nodiscard]] _Ret_z_ const char* message() const noexcept {
		return __super::what();
	}

private:
	std::error_code m_code;                  ///< @brief The error code
	mutable std::shared_ptr<char[]> m_what;  ///< @brief The error message. @details Uses a `std::shared_ptr` because exceptions must be copyable.
};


/// @brief An exception thrown for errors returned by `GetLastError`.
class windows_error : public m3c::system_error {
public:
	windows_error(const windows_error&) noexcept = default;
	windows_error(windows_error&&) noexcept = default;

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	explicit windows_error(const DWORD errorCode)
		: windows_error(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	/// @param message An error message.
	windows_error(const DWORD errorCode, _In_opt_z_ const char* const message)
		: m3c::system_error(errorCode, std::system_category(), message) {
		// empty
	}

	~windows_error() noexcept = default;

public:
	windows_error& operator=(const windows_error&) noexcept = default;  ///< @defaultoperator
	windows_error& operator=(windows_error&&) noexcept = default;       ///< @defaultoperator
};


/// @brief An exception thrown for errors returned from RPC functions.
class rpc_error : public m3c::system_error {
public:
	rpc_error(const rpc_error&) noexcept = default;
	rpc_error(rpc_error&&) noexcept = default;

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	explicit rpc_error(const RPC_STATUS errorCode)
		: rpc_error(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	/// @param message An error message.
	rpc_error(const RPC_STATUS errorCode, _In_opt_z_ const char* const message)
		: m3c::system_error(errorCode, std::system_category(), message) {
		// empty
	}

	~rpc_error() noexcept = default;

public:
	rpc_error& operator=(const rpc_error&) noexcept = default;  ///< @defaultoperator
	rpc_error& operator=(rpc_error&&) noexcept = default;       ///< @defaultoperator
};


/// @brief An exception thrown for COM errors.
class com_error : public m3c::system_error {
public:
	com_error(const com_error&) noexcept = default;
	com_error(com_error&&) noexcept = default;

	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	explicit com_error(const HRESULT hr)
		: com_error(hr, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	/// @param message An error message.
	com_error(const HRESULT hr, _In_opt_z_ const char* const message)
		: m3c::system_error(hr, std::system_category(), message) {
		// empty
	}

	~com_error() noexcept = default;

public:
	com_error& operator=(const com_error&) noexcept = default;  ///< @defaultoperator
	com_error& operator=(com_error&&) noexcept = default;       ///< @defaultoperator
};

class com_invalid_argument_error : public com_error {
public:
	com_invalid_argument_error()
		: com_invalid_argument_error(nullptr) {
		// empty
	}

	com_invalid_argument_error(const com_invalid_argument_error&) noexcept = default;
	com_invalid_argument_error(com_invalid_argument_error&&) noexcept = default;

	/// @brief Creates the exception with `HRESULT` value of `E_INVALIDARG`.
	/// @param arg The name of the invalid argument.
	explicit com_invalid_argument_error(_In_opt_z_ const char* const arg)
		: com_invalid_argument_error(E_INVALIDARG, arg) {
		// empty
	}

	explicit com_invalid_argument_error(const HRESULT hr)
		: com_invalid_argument_error(hr, nullptr) {
		// empty
	}

	com_invalid_argument_error(const HRESULT hr, _In_opt_z_ const char* const arg)
		: com_error(hr, arg) {
		// empty
	}

	~com_invalid_argument_error() noexcept = default;

public:
	com_invalid_argument_error& operator=(const com_invalid_argument_error&) noexcept = default;  ///< @defaultoperator
	com_invalid_argument_error& operator=(com_invalid_argument_error&&) noexcept = default;       ///< @defaultoperator
};

namespace internal {
#if 0

/// @brief Conversion function if the global log level is set to `DEBUG` or lower.
/// @remarks The function is always compiled to be available for testing.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at the respective level.
/// @return The HRESULT as described for `#ExceptionToHRESULT`.
HRESULT ExceptionToHRESULT_DEBUG(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*thunk)(_In_opt_ void*, Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* log) noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief Conversion function if the global log level is set higher than `DEBUG` and up to `ERROR`.
/// @remarks The function is always compiled to be available for testing.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at level `llamalog::Priority::kError`.
/// @return The HRESULT as described for `#ExceptionToHRESULT`.
HRESULT ExceptionToHRESULT_ERROR(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*thunk)(_In_opt_ void*, Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* log) noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

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
	auto log = [message, args...](const Priority priority, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
		//llamalog::Log(priority, file, line, function, message, std::forward<T>(args)..., e);
	};
	auto thunk = [](_In_ void* const p, const Priority priority, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
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
		//llamalog::Log(Priority::kError, file, line, function, message, std::forward<T>(args)..., e);
	};
	auto thunk = [](_In_ void* const p, Priority /* priority */, _In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function, const std::exception& e) {
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

#endif

class DefaultContext {
public:
	[[nodiscard]] constexpr DefaultContext(Default /* unused */, const source_location& sourceLocation = source_location::current()) noexcept
		: m_sourceLocation(sourceLocation) {
	}

protected:
	[[nodiscard]] constexpr DefaultContext(const source_location& sourceLocation) noexcept
		: m_sourceLocation(sourceLocation) {
	}

public:
	[[nodiscard]] constexpr const source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

private:
	const source_location m_sourceLocation;
};

class ExceptionContext : public DefaultContext {
public:
	[[nodiscard]] constexpr ExceptionContext(const EVENT_DESCRIPTOR& event, const source_location& sourceLocation = source_location::current()) noexcept
		: m_event(event)
		, DefaultContext(sourceLocation) {
	}

public:
	[[nodiscard]] constexpr const EVENT_DESCRIPTOR& GetEvent() const noexcept {
		return m_event;
	}

private:
	const EVENT_DESCRIPTOR m_event;
};

/// @brief A helper class to carry additional logging context for exceptions.
class __declspec(novtable) BaseException {
protected:
	BaseException(DefaultContext&& context) noexcept;
	BaseException(ExceptionContext&& context) noexcept;
#if 0
	/// @brief Creates a new instance.
	/// @param file The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param function The function where the exception happened.
	/// @param message An additional logging message which MAY use {fmt} pattern syntax.
	BaseException(_In_z_ const char* __restrict file, const EVENT_DESCRIPTOR& event) noexcept;

	BaseException(const BaseException&) = default;      ///< @defaultconstructor
	BaseException(BaseException&&) noexcept = default;  ///< @defaultconstructor
	~BaseException() noexcept = default;

public:
	BaseException& operator=(const BaseException&) = default;      ///< @defaultoperator
	BaseException& operator=(BaseException&&) noexcept = default;  ///< @defaultoperator

#endif
public:
	[[nodiscard]] const EVENT_DESCRIPTOR& GetEvent() const noexcept {
		return m_event;
	}

	/// @brief Allow access to the `LogLine` by base classes.
	/// @return The log line.
	[[nodiscard]] const LogData& GetLogData() const noexcept {
		return m_logData;
	}

	[[nodiscard]] const source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

protected:
	/// @brief Allow access to the `LogLine` by base classes.
	/// @return The log line.
	[[nodiscard]] LogData& GetLogData() noexcept {
		return m_logData;
	}

private:
	LogData m_logData;  ///< @brief Additional information for logging.
	source_location m_sourceLocation;
	EVENT_DESCRIPTOR m_event;
};

#if 0
class __declspec(novtable) EventException : public BaseException {
protected:
	EventException(ExceptionContext&& context) noexcept;

public:
	[[nodiscard]] const EVENT_DESCRIPTOR& GetEvent() const noexcept {
		return m_event;
	}

private:
	EVENT_DESCRIPTOR m_event;
};
#endif

#if 0
/// @brief The actual exception class thrown by `#Throw`.
/// @tparam E The type of the exception.
template <typename E>
class ExceptionDetailOld final : public E
	, public BaseException {
public:
	/// @brief Creates a new exception that carries additional logging context.
	/// @tparam T The type of the arguments for the message.
	/// @param exception The actual exception thrown from the code.
	/// @param file The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param function The function where the exception happened.
	/// @param message An additional logging message.
	/// @param args Arguments for the logging message.
	template <typename... T>
	ExceptionDetailOld(E&& exception, ExceptionContext&& context, T&&... args)
		: E(std::forward<E>(exception))
		, BaseException(std::move(context)) {
#pragma warning(suppress : 4834)  // value MAY be discarded if there are no arguments
		(GetLogData() << ... << std::forward<T>(args));  // NOLINT(clang-diagnostic-unused-result): Value MAY be discarded if there are no arguments.
	}
	ExceptionDetailOld(ExceptionDetailOld&) = default;            ///< @defaultconstructor
	ExceptionDetailOld(ExceptionDetailOld&&) noexcept = default;  ///< @defaultconstructor
	~ExceptionDetailOld() noexcept = default;

public:
	ExceptionDetailOld& operator=(const ExceptionDetailOld&) = default;      ///< @defaultoperator
	ExceptionDetailOld& operator=(ExceptionDetailOld&&) noexcept = default;  ///< @defaultoperator
};
#endif

template <typename E>
class ExceptionDetail : public E
	, public BaseException {
public:
	ExceptionDetail(E&& exception, DefaultContext&& context) noexcept(noexcept(E(std::move(exception))))
		: BaseException(std::move(context))
		, E(std::move(exception)) {
	}

	ExceptionDetail(E&& exception, ExceptionContext&& context) noexcept(noexcept(E(std::move(exception))))
		: BaseException(std::move(context))
		, E(std::move(exception)) {
	}

public:
	template <typename T>
	ExceptionDetail& operator<<(T&& arg) {
		GetLogData() << std::forward<T>(arg);
		return *this;
	}
};

HRESULT ExceptionToHRESULT(Priority priority, Thunk<Priority, HRESULT>&& log) noexcept;


}  // namespace internal

template <typename E>
concept Exception = std::derived_from<E, std::exception>;

template <Exception E>
internal::ExceptionDetail<E> operator+(E&& exception, internal::ExceptionContext&& context) noexcept(noexcept(internal::ExceptionDetail<E>(std::move(exception), std::move(context)))) {
	return internal::ExceptionDetail<E>(std::move(exception), std::move(context));
}

template <Exception E>
internal::ExceptionDetail<E> operator+(E&& exception, internal::DefaultContext&& context) noexcept(noexcept(internal::ExceptionDetail<E>(std::move(exception), std::move(context)))) {
	return internal::ExceptionDetail<E>(std::move(exception), std::move(context));
}


/// @brief Throws an exception adding logging context.
/// @remarks The only purpose of this function is to allow template argument deduction of `internal::ExceptionDetail`.
/// @tparam E The type of the exception.
/// @tparam T The type of the arguments for the message.
/// @param exception The actual exception thrown from the code.
/// @param file The source code file where the exception happened.
/// @param line The source code line where the exception happened.
/// @param function The function where the exception happened.
// template <typename E>
//[[noreturn]] void Throw(E&& exception, internal::ExceptionContext&& context) {
//	throw internal::ExceptionDetail<E>(std::forward<E>(exception), std::move(context));
// }

/// @brief Throws an exception adding logging context.
/// @remarks The only purpose of this function is to allow template argument deduction of `internal::ExceptionDetail`.
/// @tparam E The type of the exception.
/// @tparam T The type of the arguments for the message.
/// @param exception The actual exception thrown from the code.
/// @param file The source code file where the exception happened.
/// @param line The source code line where the exception happened.
/// @param function The function where the exception happened.
/// @param message An additional logging message which MAY use {fmt} pattern syntax.
/// @param args Arguments for the logging message.
// template <typename E, typename... T>
//[[noreturn]] void ThrowOld(E&& exception, internal::ExceptionContext&& context, T&&... args) {
//	throw internal::ExceptionDetailOld<E>(std::forward<E>(exception), std::move(context), std::forward<T>(args)...);
// }

namespace internal {

template <typename... Args>
[[noreturn]] void throw_com_exception(const HRESULT hr, ExceptionContext&& context, Args&&... args) {
	throw((m3c::com_error(hr) + std::move(context)) << ... << std::forward<Args>(args));
}

}  // namespace internal

#define M3C_COM_HR(hr_, context_, ...)                                                  \
	do {                                                                                \
		const HRESULT hr_evaluated_ = (hr_);                                            \
		if (FAILED(hr_evaluated_)) [[unlikely]] {                                       \
			m3c::internal::throw_com_exception(hr_evaluated_, (context_), __VA_ARGS__); \
		}                                                                               \
	} while (false)

template <typename... Args>
HRESULT ExceptionToHRESULT(const Priority priority, internal::EventContext<const EVENT_DESCRIPTOR&>&& context, Args&&... args) noexcept {
	auto log = [&context, &args...](const Priority priority, const HRESULT hr) noexcept {
		Log::Exception(priority, std::move(context), std::forward<Args>(args)..., make_hresult(hr));
	};
	return internal::ExceptionToHRESULT(priority, {[](void* fn, const Priority priority, const HRESULT hr) noexcept {
													   (*static_cast<decltype(log)*>(fn))(priority, hr);
												   },
	                                               &log});
}

template <typename... Args>
HRESULT ExceptionToHRESULT(const Priority priority, internal::EventContext<const char*>&& context, Args&&... args) noexcept {
	auto log = [&context, &args...](const Priority priority, const HRESULT hr) noexcept {
		Log::Exception(priority, std::move(context), std::forward<Args>(args)..., make_hresult(hr));
	};
	return internal::ExceptionToHRESULT(priority, {[](void* fn, const Priority priority, const HRESULT hr) noexcept {
													   (*static_cast<decltype(log)*>(fn))(priority, hr);
												   },
	                                               &log});
}

#if 0
template <typename... Args>
auto CheckHR(Args&&... args) -> decltype(HRESULTToException(std::forward<Args>(args)...)) {
	return HRESULTToException(std::forward<Args>(args)...);
}
template <typename... Args>
auto AsHRESULT(Args&&... args) noexcept -> decltype(ExceptionToHRESULT(std::forward<Args>(args)...)) {
	return ExceptionToHRESULT(std::forward<Args>(args)...);
}
#endif

#if 0
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

#endif

}  // namespace m3c

#if 0
//
// Macros
//

/// @brief Throw a new exception with additional logging context.
/// @details The variable arguments MAY provide a literal message string and optional arguments.
/// @param exception_ The exception to throw.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_THROW(exception_, ...)                                           \
	do {                                                                          \
		constexpr const char* __restrict file_ = llamalog::GetFilename(__FILE__); \
		llamalog::Throw(exception_, file_, __LINE__, __func__, __VA_ARGS__);      \
	} while (0)

/// @brief Alias for LLAMALOG_THROW
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define THROW LLAMALOG_THROW

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

#endif
