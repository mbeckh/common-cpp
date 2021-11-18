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

#include <m3c/LogData.h>
#include <m3c/source_location.h>

#include <windows.h>
#include <evntprov.h>
#include <rpc.h>

#include <concepts>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

//
// Exceptions for common error types
//

namespace m3c {

/// @brief A helper class to transfer system errors.
/// @details Other than `std::system_error` the message is not formatted until the time the `LogLine` is written or `#system_error::what()` is called.
/// @note Please note that the class is NOT derived from `std::system_error`.-
class system_error : public std::runtime_error {
public:
	/// @brief Creates a new instance for the provided error code and category.
	/// @param code The error code.
	/// @param category The error category.
	/// @param message The error message.
	[[nodiscard]] system_error(int code, const std::error_category& category, const std::string& message);
	[[nodiscard]] system_error(int code, const std::error_category& category, _In_opt_z_ const char* __restrict message);

	[[nodiscard]] system_error(const system_error&) noexcept = default;  ///< @defaultconstructor
	[[nodiscard]] system_error(system_error&&) noexcept = default;       ///< @defaultconstructor

	~system_error() noexcept override = default;

public:
	system_error& operator=(const system_error&) noexcept = default;  ///< @defaultoperator
	system_error& operator=(system_error&&) noexcept = default;       ///< @defaultoperator

public:
	/// @brief Get the system error code (as in `std::system_error::code()`).
	/// @result The error code.
	[[nodiscard]] const std::error_code& code() const noexcept {
		return m_code;
	}

	/// @brief Create the formatted error message.
	/// @return The formatted error message.
	[[nodiscard]] _Ret_z_ const char* what() const noexcept override;

	/// @brief Return the (unformatted) message provided in the constructor.
	/// @return The error message from the constructor.
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
	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	[[nodiscard]] explicit windows_error(_In_range_(!=, 0) const DWORD errorCode = GetLastError())
	    : windows_error(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception with the error code returned by `GetLastError`.
	/// @param message An error message.
	[[nodiscard]] explicit windows_error(_In_opt_z_ const char* const message)
	    : windows_error(GetLastError(), message) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned by `GetLastError`.
	/// @param message An error message.
	[[nodiscard]] windows_error(_In_range_(!=, 0) const DWORD errorCode, _In_opt_z_ const char* const message)
	    : m3c::system_error(static_cast<int>(errorCode), std::system_category(), message) {
		// empty
	}

	[[nodiscard]] windows_error(const windows_error&) noexcept = default;  ///< @defaultconstructor
	[[nodiscard]] windows_error(windows_error&&) noexcept = default;       ///< @defaultconstructor

	~windows_error() noexcept override = default;

public:
	windows_error& operator=(const windows_error&) noexcept = default;  ///< @defaultoperator
	windows_error& operator=(windows_error&&) noexcept = default;       ///< @defaultoperator
};


/// @brief An exception thrown for errors returned from RPC functions.
class rpc_error : public m3c::system_error {
public:
	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	[[nodiscard]] explicit rpc_error(_In_range_(!=, 0) const RPC_STATUS errorCode)
	    : rpc_error(errorCode, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param errorCode The windows error code returned from RPC functions.
	/// @param message An error message.
	[[nodiscard]] rpc_error(_In_range_(!=, 0) const RPC_STATUS errorCode, _In_opt_z_ const char* const message)
	    : m3c::system_error(errorCode, std::system_category(), message) {
		// empty
	}

	[[nodiscard]] rpc_error(const rpc_error&) noexcept = default;  ///< @defaultconstructor
	[[nodiscard]] rpc_error(rpc_error&&) noexcept = default;       ///< @defaultconstructor

	~rpc_error() noexcept override = default;

public:
	rpc_error& operator=(const rpc_error&) noexcept = default;  ///< @defaultoperator
	rpc_error& operator=(rpc_error&&) noexcept = default;       ///< @defaultoperator
};


/// @brief An exception thrown for COM errors.
class com_error : public m3c::system_error {
public:
	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	[[nodiscard]] explicit com_error(_In_range_(<, 0) const HRESULT hr)
	    : com_error(hr, nullptr) {
		// empty
	}

	/// @brief Creates the exception.
	/// @param hr The `HRESULT` value.
	/// @param message An error message.
	[[nodiscard]] com_error(_In_range_(<, 0) const HRESULT hr, _In_opt_z_ const char* const message)
	    : m3c::system_error(hr, std::system_category(), message) {
		// empty
	}

	[[nodiscard]] com_error(const com_error&) noexcept = default;  ///< @defaultconstructor
	[[nodiscard]] com_error(com_error&&) noexcept = default;       ///< @defaultconstructor

	~com_error() noexcept override = default;

public:
	com_error& operator=(const com_error&) noexcept = default;  ///< @defaultoperator
	com_error& operator=(com_error&&) noexcept = default;       ///< @defaultoperator
};

/// @brief An helper class to easily distinguish invalid argument errors from other COM errors in exception handlers.
class com_invalid_argument_error : public com_error {
public:
	[[nodiscard]] com_invalid_argument_error()
	    : com_invalid_argument_error(nullptr) {
		// empty
	}

	/// @brief Creates the exception with `HRESULT` value of `E_INVALIDARG`.
	/// @param arg The name of the invalid argument.
	[[nodiscard]] explicit com_invalid_argument_error(_In_opt_z_ const char* const arg)
	    : com_invalid_argument_error(E_INVALIDARG, arg) {
		// empty
	}

	[[nodiscard]] explicit com_invalid_argument_error(_In_range_(<, 0) const HRESULT hr)
	    : com_invalid_argument_error(hr, nullptr) {
		// empty
	}

	[[nodiscard]] com_invalid_argument_error(_In_range_(<, 0) const HRESULT hr, _In_opt_z_ const char* const arg)
	    : com_error(hr, arg) {
		// empty
	}

	[[nodiscard]] com_invalid_argument_error(const com_invalid_argument_error&) noexcept = default;  ///< @defaultconstructor
	[[nodiscard]] com_invalid_argument_error(com_invalid_argument_error&&) noexcept = default;       ///< @defaultconstructor

	~com_invalid_argument_error() noexcept override = default;

public:
	com_invalid_argument_error& operator=(const com_invalid_argument_error&) noexcept = default;  ///< @defaultoperator
	com_invalid_argument_error& operator=(com_invalid_argument_error&&) noexcept = default;       ///< @defaultoperator
};


//
// Exception context
//

namespace internal {

/// @brief The type of the `evt::Default` singleton.
struct Default final {
	// empty
};

}  // namespace internal

namespace evt {

/// @brief A singleton to add context but no custom error information to exceptions.
inline constexpr internal::Default Default;  // NOLINT(readability-identifier-naming): No prefix for consistency with other events.

}  // namespace evt


namespace internal {

/// @brief Location context for exceptions.
/// @details The class make `std::source_location` of the caller available to the overloaded operators.
class DefaultContext {
public:
	/// @brief Create a new context. The unused parameter is required for automatic type creation.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr DefaultContext(Default /* unused */, const std::source_location& sourceLocation = std::source_location::current()) noexcept  // NOLINT(google-explicit-constructor): Implicit conversion is intentional.
	    : m_sourceLocation(sourceLocation) {
		// empty
	}

protected:
	/// @brief Constructor used by sub class.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr explicit DefaultContext(const std::source_location& sourceLocation) noexcept
	    : m_sourceLocation(sourceLocation) {
		// empty
	}

public:
	DefaultContext(const DefaultContext&) = delete;
	DefaultContext(DefaultContext&&) = delete;

	constexpr ~DefaultContext() noexcept = default;

public:
	DefaultContext& operator=(const DefaultContext&) = delete;
	DefaultContext& operator=(DefaultContext&&) = delete;

public:
	/// @brief Get the source location of the caller.
	/// @return The source location.
	[[nodiscard]] constexpr const std::source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

private:
	const std::source_location m_sourceLocation;  ///< @brief The source location.
};


/// @brief Location and event context for exceptions.
/// @details The class make `std::source_location` of the caller available to the overloaded operators.
class ExceptionContext : public DefaultContext {
public:
	/// @brief Create a new context.
	/// @param event The event descriptor for the exception.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr ExceptionContext(const EVENT_DESCRIPTOR& event, const std::source_location& sourceLocation = std::source_location::current()) noexcept  // NOLINT(google-explicit-constructor): Implicit conversion is intentional.
	    : DefaultContext(sourceLocation)
	    , m_event(event) {
		// empty
	}

	ExceptionContext(const ExceptionContext&) = delete;
	ExceptionContext(ExceptionContext&&) = delete;

	constexpr ~ExceptionContext() noexcept = default;

public:
	ExceptionContext& operator=(const ExceptionContext&) = delete;
	ExceptionContext& operator=(ExceptionContext&&) = delete;

public:
	/// @brief Get the event descriptor.
	/// @return The event descriptor.
	[[nodiscard]] constexpr const EVENT_DESCRIPTOR& GetEvent() const noexcept {
		return m_event;
	}

private:
	const EVENT_DESCRIPTOR m_event;  ///< @brief The event descriptor.
};


//
// Exception with context
//

/// @brief A mixin class to carry additional logging context for exceptions.
class __declspec(novtable) BaseException {
protected:
	/// @brief Create a new object with context information.
	/// @param context A default exception context.
	[[nodiscard]] constexpr explicit BaseException(const DefaultContext& context) noexcept  // NOLINT(cppcoreguidelines-pro-type-member-init): m_event initialized in body.
	    : m_sourceLocation(context.GetSourceLocation()) {
		m_event.Id = 0;  // EventDescSetId(&m_event, 0) is not constexpr
	}

	/// @brief Create a new object with context information.
	/// @param context An exception context.
	[[nodiscard]] constexpr explicit BaseException(const ExceptionContext& context) noexcept
	    : m_sourceLocation(context.GetSourceLocation())
	    , m_event(context.GetEvent()) {
	}

	/// @brief Defaulted non-throwing copy constructor required for exceptions.
	[[nodiscard]] BaseException(const BaseException&) noexcept = default;

	/// @brief Defaulted non-throwing move constructor required for exceptions.
	[[nodiscard]] BaseException(BaseException&&) noexcept = default;

public:
	virtual ~BaseException() noexcept = 0;

public:
	/// @brief Defaulted non-throwing copy operator required for exceptions.
	BaseException& operator=(const BaseException&) noexcept = default;

	/// @brief Defaulted non-throwing move operator required for exceptions.
	BaseException& operator=(BaseException&&) noexcept = default;

public:
	/// @brief Allow access to the source location from the context.
	/// @return The source location.
	[[nodiscard]] constexpr const std::source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

	/// @brief Get the event descriptor from the context.
	/// @return The event descriptor.
	[[nodiscard]] constexpr const EVENT_DESCRIPTOR& GetEvent() const noexcept {
		return m_event;
	}

	/// @brief Allow access to the `LogData` of the context.
	/// @return The log data.
	[[nodiscard]] constexpr const LogData& GetLogData() const noexcept {
		return m_logData;
	}

protected:
	/// @brief Allow access to the `LogData` of the context for appending.
	/// @return The log data.
	[[nodiscard]] constexpr LogData& GetLogData() noexcept {
		return m_logData;
	}

private:
	LogData m_logData;                      ///< @brief Additional information for logging.
	std::source_location m_sourceLocation;  ///< @brief The source location where the exception happened.
	EVENT_DESCRIPTOR m_event;               ///< @brief The event to log.
};


/// @brief The actual exception class thrown when adding context.
/// @tparam E The type of the exception.
template <typename E>
class ExceptionDetail : public E
    , public BaseException {
public:
	/// @brief Create a new exception from exception and context.
	/// @param exception The exception object.
	/// @param context Additional default context with source location.
	[[nodiscard]] constexpr ExceptionDetail(E&& exception, const DefaultContext& context) noexcept(noexcept(E(std::forward<E>(exception))))
	    : E(std::forward<E>(exception))
	    , BaseException(context) {
	}

	/// @brief Create a new exception from exception and context.
	/// @param exception The exception object.
	/// @param context Additional context with source location and event data.
	[[nodiscard]] constexpr ExceptionDetail(E&& exception, const ExceptionContext& context) noexcept(noexcept(E(std::forward<E>(exception))))
	    : E(std::forward<E>(exception))
	    , BaseException(context) {
	}

	/// @brief Defaulted non-throwing copy constructor required for exceptions.
	[[nodiscard]] constexpr ExceptionDetail(const ExceptionDetail&) noexcept = default;

	/// @brief Defaulted non-throwing move constructor required for exceptions.
	[[nodiscard]] constexpr ExceptionDetail(ExceptionDetail&&) noexcept = default;

	constexpr ~ExceptionDetail() noexcept override = default;

public:
	/// @brief Defaulted non-throwing copy operator required for exceptions.
	constexpr ExceptionDetail& operator=(const ExceptionDetail&) noexcept = default;

	/// @brief Defaulted non-throwing move operator required for exceptions.
	constexpr ExceptionDetail& operator=(ExceptionDetail&&) noexcept = default;

	/// @brief Add arguments to the exception context.
	/// @details @p arg is forwarded to a `LogData` object. The method is required to retain the type of the exception.
	/// @tparam T The type of the argument.
	/// @param arg The argument.
	/// @return This instance.
	template <typename T>
	constexpr ExceptionDetail& operator<<(T&& arg) {
		GetLogData() << std::forward<T>(arg);
		return *this;
	}
};

}  // namespace internal


/// @brief `true` for any type derived from `std::exception`.
/// @tparam E A type.
template <typename E>
concept Exception = std::derived_from<E, std::exception>;

}  // namespace m3c


/// @brief Create an exception enhanced with context information for source location.
/// @details The operator MUST be in global scope to allow automatic instantiation.
/// @tparam E The type of the exception.
/// @param exception The exception object.
/// @param context Context information for source location.
/// @return A newly created exception with additional context.
template <m3c::Exception E>
[[nodiscard]] constexpr m3c::internal::ExceptionDetail<E> operator+(E&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<E>(std::forward<E>(exception), context))) {
	return m3c::internal::ExceptionDetail<E>(std::forward<E>(exception), context);
}

/// @brief Create an exception enhanced with context information for source location and events.
/// @details The operator MUST be in global scope to allow automatic instantiation of `m3c::internal::ExceptionContext` from `EVENT_DESCRIPTOR`.
/// @tparam E The type of the exception.
/// @param exception The exception object.
/// @param context Context information for source location and event data.
/// @return A newly created exception with additional context.
template <m3c::Exception E>
[[nodiscard]] constexpr m3c::internal::ExceptionDetail<E> operator+(E&& exception, m3c::internal::ExceptionContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<E>(std::forward<E>(exception), context))) {
	return m3c::internal::ExceptionDetail<E>(std::forward<E>(exception), context);
}


namespace m3c::internal {

/// @brief Throws a `com_exception` with additional context.
/// @details Required as a function to evaluate the variable arguments properly (unpack operator call, prevent double evaluation).
/// @tparam Args The argument pack.
/// @param hr The `HRESULT` value.
/// @param context Context with source location and event information.
/// @param args Arguments to add to the context.
template <typename... Args>
[[noreturn]] inline void throw_com_exception(_In_range_(<, 0) const HRESULT hr, ExceptionContext&& context, Args&&... args) {
	throw((m3c::com_error(hr) + std::move(context)) << ... << std::forward<Args>(args));
}

}  // namespace m3c::internal


/// @brief Checks a `HRESULT` code and throws a `m3c::com_exception` in case of failure.
/// @param hr_ The `HRESULT` code.
/// @param context_ Additional context for the exception followed by optional arguments.
#define M3C_COM_HR(hr_, context_, ...)                                                  \
	do {                                                                                \
		const HRESULT hr_evaluated_ = (hr_);                                            \
		if (FAILED(hr_evaluated_)) {                                                    \
			[[unlikely]];                                                               \
			m3c::internal::throw_com_exception(hr_evaluated_, (context_), __VA_ARGS__); \
		}                                                                               \
	} while (false)
