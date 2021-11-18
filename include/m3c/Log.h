/*
Copyright 2021 Michael Beckh

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
#include <m3c/format.h>  // IWYU pragma: export
#include <m3c/source_location.h>
#include <m3c/type_traits.h>

#include <windows.h>
#include <evntprov.h>
#include <winmeta.h>

#include <string>
#include <type_traits>
#include <utility>

namespace m3c {

/// @brief Enum for different log priorities.
enum class Priority : UCHAR {
	kNone = 0,
	kCritical = WINEVENT_LEVEL_CRITICAL,  ///< A condition leading to program abort.
	kError = WINEVENT_LEVEL_ERROR,        ///< A recoverable error.
	kWarning = WINEVENT_LEVEL_WARNING,    ///< A condition which should be inspected.
	kInfo = WINEVENT_LEVEL_INFO,          ///< Informational message which should be logged.
	kVerbose = WINEVENT_LEVEL_VERBOSE,    ///< More detailed informational message.
	kDebug = 150,                         ///< Output useful for debugging.
	kTrace = 200                          ///< Output useful for inspecting program flow during debugging.
};

namespace log {

/// @brief Customization point to Print a log message.
/// @details This method MUST be overridden when print logging is enabled.
/// @param priority The log priority.
/// @param message The formatted log message.
void Print(Priority priority, const std::string& message);

}  // namespace log


namespace internal {

/// @brief A type of log message.
/// @tparam T The actual type of the log message.
template <typename T>
concept LogMessage = AnyOf<T, const EVENT_DESCRIPTOR&, const char*>;


/// @brief Log context with message and source location.
/// @remarks Used to automatically supply the source location to log functions with variable number of arguments.
/// @tparam M The type of the log message.
template <LogMessage M>
class LogContext {
public:
	/// @brief `true` for log messages of event type, `false` for string messages.
	static constexpr bool kIsEventDescriptor = std::is_same_v<M, const EVENT_DESCRIPTOR&>;

public:
	/// @brief Create a log context from an event descriptor or a C-string.
	/// @details The string is not copied, therefore this function MUST only be used as an immediate argument to a logging call.
	/// @param message The log message.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr LogContext(const M message, const std::source_location sourceLocation = std::source_location::current()) noexcept  // NOLINT(google-explicit-constructor): Implicit conversion is intentional.
	    : m_sourceLocation(sourceLocation)
	    , m_message(message) {
	}

	/// @brief Create a log context from a `std::string`.
	/// @details The string is not copied, therefore this function MUST only be used as an immediate argument to a logging call.
	/// @param message The log message.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr LogContext(const std::string& message, const std::source_location sourceLocation = std::source_location::current()) noexcept requires(!kIsEventDescriptor)  // NOLINT(google-explicit-constructor): Implicit conversion is intentional.
	    : m_sourceLocation(sourceLocation)
	    , m_message(message.c_str()) {
	}

	/// @brief Create a log context from a `std::string`.
	/// @details The string is not copied, therefore this function MUST only be used as an immediate argument to a logging call.
	/// @param message The log message.
	/// @param sourceLocation The source location.
	[[nodiscard]] constexpr LogContext(std::string&& message, const std::source_location sourceLocation = std::source_location::current()) noexcept requires(!kIsEventDescriptor)  // NOLINT(google-explicit-constructor): Implicit conversion is intentional.
	    : m_sourceLocation(sourceLocation)
	    , m_message(message.c_str()) {
	}

	LogContext(const LogContext&) = delete;
	LogContext(LogContext&&) = delete;

	constexpr ~LogContext() noexcept = default;

public:
	LogContext& operator=(const LogContext&) = delete;
	LogContext& operator=(LogContext&&) = delete;

public:
	/// @brief Get the source location.
	/// @return The source location.
	[[nodiscard]] constexpr const std::source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

	/// @brief Get the log message.
	/// @remarks GetMessage Is a function of the Windows API automatically changed to either GetMessageA or GetMessageW.
	/// @return The log message as either a const reference or a const pointer.
	[[nodiscard]] constexpr M GetLogMessage() const noexcept {
		return m_message;
	}

private:
	const std::source_location m_sourceLocation;  ///< @brief The source location.
	const M m_message;                            ///< @brief The log messsage.
};


/// @brief A wrapper for calling a capturing lambda closure using a function pointer.
/// @tparam kNoexcept `true` if the closure is noexcept.
/// @tparam R The return value of the closure.
/// @tparam Args The args of the closure.
/// @copyright The class uses a trick that allows calling a capturing lambda using a function pointer. It is published
/// by Joaquín M López Muñoz at http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html.
template <bool kNoexcept, typename R, typename... Args>
class Closure final {
public:
	/// @brief Creates an object for calling @p closure later.
	/// @details The closure is stored by reference.
	/// @tparam C The type of the closure expression.
	/// @param closure The closure expression which is stored by reference.
	template <typename C>
	Closure(C&& closure) noexcept  // NOLINT(google-explicit-constructor): Automatic conversion in function call.
	    : m_thunk([](const void* const closure, Args... args) noexcept(kNoexcept) -> R {
		    return (*static_cast<const std::remove_reference_t<C>*>(closure))(args...);
	    })
	    , m_closure(&closure) {
		static_assert(noexcept(closure) || !kNoexcept, "throwing closure not allowed for non-throwing Closure object");
	}

	Closure(const Closure&) = delete;
	Closure(Closure&&) = delete;
	~Closure() = default;

	Closure& operator=(const Closure&) = delete;
	Closure& operator=(Closure&&) = delete;

	/// @brief Calls the lambda with the arguments @p args.
	/// @param args The arguments.
	/// @return The result of calling the lambda.
	R operator()(Args... args) const noexcept(kNoexcept) {
		return m_thunk(m_closure, args...);
	}

private:
	/// @brief The function called in `operator()`.
	R(*const m_thunk)
	(const void*, Args...) noexcept(kNoexcept);

	/// @brief The capturing closure.
	const void* const m_closure;
};

template <typename R, typename... Args>
using ClosureExcept = Closure<false, R, Args...>;

template <typename R, typename... Args>
using ClosureNoexcept = Closure<true, R, Args...>;


/// @brief Get the string message for the event id.
/// @details The string contains placeholders for `fmt::format`
/// @param eventId The event id.
/// @return The message string.
[[nodiscard]] std::string GetEventMessagePattern(USHORT eventId);

}  // namespace internal


/// @brief A logger for logging to the Windows event log, debug output and `stderr`.
class Log {
public:
	/// @brief Creates a new logger.
	[[nodiscard]] Log() noexcept;
	Log(const Log&) = delete;
	Log(Log&&) = delete;
	~Log() noexcept;

public:
	Log& operator=(const Log&) = delete;
	Log& operator=(Log&&) = delete;

public:
#pragma push_macro("MAIN_METHOD")
#pragma push_macro("LEVEL_METHOD")
#pragma push_macro("LEVEL_METHOD_ONCE")
#pragma push_macro("RESULT_METHOD")
#pragma push_macro("FOR_EACH_PRIORITY")
#pragma push_macro("FOR_EACH_PRIORITY_ONCE")
#pragma push_macro("FOR_EACH_CASE")
#pragma push_macro("FOR_EACH_CASE_ONCE")
#pragma push_macro("FOR_EACH_TYPE")
#undef MAIN_METHOD
#undef LEVEL_METHOD
#undef LEVEL_METHOD_ONCE
#undef RESULT_METHOD
#undef FOR_EACH_PRIORITY
#undef FOR_EACH_PRIORITY_ONCE
#undef FOR_EACH_CASE
#undef FOR_EACH_CASE_ONCE
#undef FOR_EACH_TYPE

/// @brief Creates functions `Log::Message` and `Log::Exception`.
/// @param type_ The type of the log message.
/// @param message_ Either `Message` or empty.
/// @param exception_ Either `Exception` or empty.
#define MAIN_METHOD(type_, message_, exception_)                                                                                      \
	template <typename... Args>                                                                                                       \
	static void message_##exception_(const Priority priority, const internal::LogContext<type_>&& context, Args&&... args) noexcept { \
		if (priority <= kLevel) {                                                                                                     \
			GetInstance().Log##message_##exception_(priority, context, args...);                                                      \
		}                                                                                                                             \
	}

/// @brief Creates functions `Log::<Priority>` and `Log::<Priority>Exception`.
/// @param priority_ The logging priority.
/// @param type_ The type of the log message.
/// @param message_ Either `Message` or empty.
/// @param exception_ Either `Exception` or empty.
#define LEVEL_METHOD(priority_, type_, message_, exception_)                                                  \
	template <typename... Args>                                                                               \
	static void priority_##exception_(const internal::LogContext<type_>&& context, Args&&... args) noexcept { \
		if (Priority::k##priority_ <= kLevel) {                                                               \
			GetInstance().Log##message_##exception_(Priority::k##priority_, context, args...);                \
		}                                                                                                     \
	}

/// @brief Creates functions `Log::<Priority>Once` and `Log::<Priority>ExceptionOnce`.
/// @param priority_ The logging priority.
/// @param type_ The type of the log message (always `const EVENT_DESCRIPTOR&`, but helps supplying empty @p message_ and @p exception_ parameters).
/// @param message_ Either `Message` or empty.
/// @param exception_ Either `Exception` or empty.
#define LEVEL_METHOD_ONCE(priority_, type_, message_, exception_)                                                   \
	template <typename... Args>                                                                                     \
	static void priority_##exception_##Once(const internal::LogContext<type_>&& context, Args&&... args) noexcept { \
		static_assert(internal::LogContext<type_>::kIsEventDescriptor, "not an event descriptor");                  \
		if (Priority::k##priority_ <= kLevel) {                                                                     \
			USHORT* const pEventId = LogOnce(context.GetLogMessage().Id);                                           \
			if (pEventId) {                                                                                         \
				[[likely]];                                                                                         \
				GetInstance().Log##message_##exception_(Priority::k##priority_, context, args...);                  \
				*pEventId = 0;                                                                                      \
			}                                                                                                       \
		}                                                                                                           \
	}

/// @brief Creates functions `Log::TraceResult`, `Log::TraceHResult` and `Log::ExceptionToHResult`.
/// @param type_ The type of the log message.
#define RESULT_METHOD(type_)                                                                                                                                     \
	template <typename R, typename... Args>                                                                                                                      \
	[[nodiscard]] static _Ret_range_(==, rv) R TraceResult(R&& rv, const internal::LogContext<type_>&& context, Args&&... args) noexcept {                       \
		if (Priority::kTrace <= kLevel) {                                                                                                                        \
			GetInstance().LogMessage(Priority::kTrace, context, rv, args...);                                                                                    \
		}                                                                                                                                                        \
		return std::forward<R>(rv);                                                                                                                              \
	}                                                                                                                                                            \
	template <typename... Args>                                                                                                                                  \
	[[nodiscard]] static _Ret_range_(==, rv) HRESULT TraceHResult(const HRESULT rv, const internal::LogContext<type_>&& context, Args&&... args) noexcept {      \
		if (Priority::kTrace <= kLevel) {                                                                                                                        \
			GetInstance().LogMessage(Priority::kTrace, context, static_cast<DWORD>(rv), args...);                                                                \
		}                                                                                                                                                        \
		return rv;                                                                                                                                               \
	}                                                                                                                                                            \
	template <typename... Args>                                                                                                                                  \
	[[nodiscard]] static _Ret_range_(<, 0) HRESULT ExceptionToHResult(const Priority priority, internal::LogContext<type_>&& context, Args&&... args) noexcept { \
		return ExceptionToHResult(priority, [&context, &args...](const Priority priority, const HRESULT hr) noexcept {                                           \
			GetInstance().LogException(priority, context, args..., hresult(hr));                                                                                 \
		});                                                                                                                                                      \
	}

/// @brief Creates either method `Log::<Priority>` or `Log::<Priority>Exception` for each priority.
/// @param type_ The type of the log message.
/// @param message_ Either `Message` or empty.
/// @param exception_ Either `Exception` or empty.
#define FOR_EACH_PRIORITY(type_, message_, exception_)  \
	LEVEL_METHOD(Critical, type_, message_, exception_) \
	LEVEL_METHOD(Error, type_, message_, exception_)    \
	LEVEL_METHOD(Warning, type_, message_, exception_)  \
	LEVEL_METHOD(Info, type_, message_, exception_)     \
	LEVEL_METHOD(Verbose, type_, message_, exception_)  \
	LEVEL_METHOD(Debug, type_, message_, exception_)    \
	LEVEL_METHOD(Trace, type_, message_, exception_)

/// @brief Creates either method `Log::<Priority>Once` or `Log::<Priority>ExceptionOnce` for each priority.
/// @remarks This family of methods is required when logging code might generated the same message again.
/// Typically, this only affects formatters and methods that append to any kind of `LogArgs`.
/// @param type_ The type of the log message (always `const EVENT_DESCRIPTOR&`, but helps supplying empty @p message_ and @p exception_ parameters).
/// @param message_ Either `Message` or empty.
/// @param exception_ Either `Exception` or empty.
#define FOR_EACH_PRIORITY_ONCE(type_, message_, exception_)  \
	LEVEL_METHOD_ONCE(Critical, type_, message_, exception_) \
	LEVEL_METHOD_ONCE(Error, type_, message_, exception_)    \
	LEVEL_METHOD_ONCE(Warning, type_, message_, exception_)  \
	LEVEL_METHOD_ONCE(Info, type_, message_, exception_)     \
	LEVEL_METHOD_ONCE(Verbose, type_, message_, exception_)  \
	LEVEL_METHOD_ONCE(Debug, type_, message_, exception_)    \
	LEVEL_METHOD_ONCE(Trace, type_, message_, exception_)

/// @brief Creates methods `Log::Message`, `Log::Exception`, `Log::TraceResult`, `Log::TraceHResult`,
/// `Log::ExceptionToHResult`, `Log::<Priority>` and Log::<Priority>Exception` for each priority.
/// @param type_ The type of the log message.
#define FOR_EACH_CASE(type_)              \
	MAIN_METHOD(type_, Message, )         \
	MAIN_METHOD(type_, , Exception)       \
	FOR_EACH_PRIORITY(type_, Message, )   \
	FOR_EACH_PRIORITY(type_, , Exception) \
	RESULT_METHOD(type_)

/// @brief Creates both methods `Log::<Priority>Once` and Log::<Priority>ExceptionOnce` for each priority.
/// @param type_ The type of the log message.
#define FOR_EACH_CASE_ONCE(type_)            \
	FOR_EACH_PRIORITY_ONCE(type_, Message, ) \
	FOR_EACH_PRIORITY_ONCE(type_, , Exception)

/// @brief Creates all static log methods for types of log messages.
#define FOR_EACH_TYPE()                    \
	FOR_EACH_CASE(const EVENT_DESCRIPTOR&) \
	FOR_EACH_CASE(const char*)             \
	FOR_EACH_CASE_ONCE(const EVENT_DESCRIPTOR&)

	FOR_EACH_TYPE()

#pragma pop_macro("MAIN_METHOD")
#pragma pop_macro("LEVEL_METHOD")
#pragma pop_macro("LEVEL_METHOD_ONCE")
#pragma pop_macro("RESULT_METHOD")
#pragma pop_macro("FOR_EACH_PRIORITY")
#pragma pop_macro("FOR_EACH_PRIORITY_ONCE")
#pragma pop_macro("FOR_EACH_CASE")
#pragma pop_macro("FOR_EACH_CASE_ONCE")
#pragma pop_macro("FOR_EACH_TYPE")

	/// @brief Convert the priority to a string.
	/// @param priority The priority.
	/// @return The string naming the priority.
	[[nodiscard]] static std::string GetLevelName(Priority priority);

	/// @brief Print a log message to debug output and `stderr`.
	/// @param priority The log priority.
	/// @param message The formatted log message.
	static void PrintDefault(Priority priority, const std::string& message);

private:
	/// @brief Get the single logger instance.
	/// @return The single logger instance.
	[[nodiscard]] static Log& GetInstance() noexcept;

	/// @brief Registers this logger with Windows event log.
	void RegisterEvents() noexcept;
	/// @brief Unregisters this logger with Windows event log.
	void UnregisterEvents() noexcept;

	/// @brief Log a message.
	/// @tparam M The type of the log message.
	/// @tparam Args The type of the additional log arguments.
	/// @param priority The log priority.
	/// @param context Log message and source location.
	/// @param args Additional log arguments.
	template <internal::LogMessage M, typename... Args>
	void LogMessage(const Priority priority, const internal::LogContext<M>& context, const Args&... args) noexcept {
		try {
			DoLogMessage(priority, context, "",
			             GetPrintCapture<internal::LogContext<M>::kIsEventDescriptor>(args...),
			             GetEventCapture<internal::LogContext<M>::kIsEventDescriptor>(args...));
		} catch (...) {
			LogInternalError<M>(context);
		}
	}

	/// @brief Log a message together with the current exception from a `catch` block.
	/// @note The function MUST be called from within a catch block.
	/// @tparam M The type of the log message.
	/// @tparam Args The type of the additional log arguments.
	/// @param priority The log priority.
	/// @param context Log message and source location.
	/// @param args Additional log arguments.
	template <internal::LogMessage M, typename... Args>
	void LogException(const Priority priority, const internal::LogContext<M>& context, const Args&... args) noexcept {
		try {
			DoLogException(priority, context,
			               GetPrintCapture<internal::LogContext<M>::kIsEventDescriptor>(args...),
			               GetEventCapture<internal::LogContext<M>::kIsEventDescriptor>(args...));
		} catch (...) {
			LogInternalError<M>(context);
		}
	}

	/// @brief Log an error which happened during logging.
	/// @tparam M The type of the log message which caused the error.
	/// @param loggedContext The `LogContext` from the call which caused the error.
	template <internal::LogMessage M>
	void LogInternalError(const internal::LogContext<M>& loggedContext) noexcept;

	/// @brief Convert any exception to a `HRESULT` and log an event.
	/// @details The `HRESULT` is converted from `#system_error::code()` if available, is set to `E_OUTOFMEMORY` for
	/// `std::bad_alloc` and `E_FAIL` for any other type of exception.
	/// @note The function MUST be called from within a catch block.
	/// @param priority The priority of the exception (`com_invalid_argument_error` uses a fixed value of `Priority::kDebug`).
	/// @param log A callable `Closure` making the actual logging call with dynamic `Priority` and `HRESULT`.
	[[nodiscard]] static _Ret_range_(<, 0) HRESULT ExceptionToHResult(Priority priority, const internal::ClosureNoexcept<void, Priority, HRESULT>&& log) noexcept;

	/// @brief Return a lambda which populates a `LogFormatArgs` object.
	/// @tparam kEvent `true` when an event was supplied as a log message, `false` for string messages
	/// @tparam Args The types of the log arguments.
	/// @param args The log arguments.
	/// @return A lambda which populates a `LogFormatArgs` object.
	template <bool kEvent, typename... Args>
	[[nodiscard]] static constexpr auto GetPrintCapture([[maybe_unused]] const Args&... args) {
		if constexpr (sizeof...(Args) > 0) {
			if constexpr (kEvent) {
				// use formatting compatible with Windows event log, e.g. split arguments into multiple arguments.
				return [&args...](_Inout_ LogFormatArgs& formatArgs) {
					((formatArgs << args), ...);
				};
			} else {
				// string message may use default formatter
				return [&args...](_Inout_ LogFormatArgs& formatArgs) {
					((formatArgs % args), ...);
				};
			}
		} else {
			return [](_Inout_ LogFormatArgs&) constexpr noexcept {
			    // empty
			};
		}
	};

	/// @brief Return a lambda which populates a `LogEventArgs` object.
	/// @tparam kEvent `true` when an event was supplied as a log message, `false` for string messages
	/// @tparam Args The types of the log arguments.
	/// @param args The log arguments.
	/// @return A lambda which populates a `LogEventArgs` object.
	template <bool kEvent, typename... Args>
	[[nodiscard]] static constexpr auto GetEventCapture([[maybe_unused]] const Args&... args) noexcept {
		if constexpr (kEvent && sizeof...(Args) > 0) {
			return [&args...](_Inout_ LogEventArgs& eventArgs) {
				((eventArgs << args), ...);
			};
		} else {
			return [](_Inout_ LogEventArgs&) constexpr noexcept {
			    // empty
			};
		}
	}

	/// @brief Internal method to log a message.
	/// @remarks The method is defined in a separate source file and is compiled for each executable to allow
	/// optimizations based on compile-time conditions.
	/// @tparam M The type of the log message.
	/// @param priority The log priority.
	/// @param context Log message and source location.
	/// @param cause Log output for an exception for printing.
	/// @param print A callable `Closure` to obtain log arguments for printing to debug output and `stderr`.
	/// @param event A callable `Closure` to obtain log arguments for writing to the Windows event log.
	template <internal::LogMessage M>
	void DoLogMessage(Priority priority, const internal::LogContext<M>& context, _In_z_ const char* cause, const internal::ClosureExcept<void, LogFormatArgs&>&& print, const internal::ClosureExcept<void, LogEventArgs&>&& event);

	/// @brief Internal method  to log a message together with an exception.
	/// @remarks The method is defined in a separate source file and is compiled for each executable to allow
	/// optimizations based on compile-time conditions.
	/// @tparam M The type of the log message.
	/// @param priority The log priority.
	/// @param context Log message and source location.
	/// @param print A callable `Closure` to obtain log arguments for printing to debug output and `stderr`.
	/// @param event A callable `Closure` to obtain log arguments for writing to the Windows event log.
	template <internal::LogMessage M>
	void DoLogException(Priority priority, const internal::LogContext<M>& context, const internal::ClosureExcept<void, LogFormatArgs&>&& print, const internal::ClosureExcept<void, LogEventArgs&>&& event);

	/// @brief Print a log message to print output.
	/// @details Delegates to `log::Print(Priority, const std::string&)` which MUST be implemented by the application.
	/// @param priority The log priority.
	/// @param pattern The formatter pattern.
	/// @param formatArgs The formatter arguments.
	/// @param cause Log output for an exception.
	/// @param sourceLocation The source location of the log message.
	static void Print(Priority priority, _In_z_ const char* pattern, const LogFormatArgs& formatArgs, _In_z_ const char* cause, const std::source_location& sourceLocation);

	/// @brief Write an event to the Windows event log.
	/// @param event The `EVENT_DESCRIPTOR` containing message, priority and other attributes.
	/// @param eventArgs The arguments for the event.
	/// @param pRelatedActivityId An optional pointer to a `GUID` for grouping related events.
	void WriteEvent(const EVENT_DESCRIPTOR& event, LogEventArgs& eventArgs, _In_opt_ const GUID* pRelatedActivityId = nullptr) const;

	/// @brief Writes events for an exception and generates a string containing the log messages.
	/// @details The method handles nested exceptions.
	/// @tparam kPrint `true` if the logger writes to debug output or `stderr`.
	/// @tparam kEvent `true` if the logger writes to the Windows event log.
	/// @param priority The log priority.
	/// @param activityId The activity id of the main log message.
	/// @param cause A string to which the log output for each exception is prepended.
	template <bool kPrint, bool kEvent>
	void WriteException(Priority priority, const GUID& activityId, _Inout_ std::string& cause);

	/// @brief The actual method to write events and output for a single exception
	/// @tparam kPrint `true` if the logger writes to debug output or `stderr`.
	/// @tparam kEvent `true` if the logger writes to the Windows event log.
	/// @param priority The log priority.
	/// @param activityId The activity id of the main log message.
	/// @param cause A string to which the log output for each exception is prepended.
	template <bool kPrint, bool kEvent>
	void DoWriteException(Priority priority, const GUID& activityId, _Inout_ std::string& cause);

	/// @brief Helper method to ensure that a particular event is logged once only.
	/// @param eventId The id of the event.
	/// @return Either `nullptr` to skip logging or a pointer which MUST be set to 0 after logging has completed.
	[[nodiscard]] static USHORT* LogOnce(USHORT eventId) noexcept;

	/// @brief Sets the activity id of the current thread.
	/// @remarks If the method returns `true`, the code MUST call `ResetActivityId` with @p activityId.
	/// @param activityId The previously set activity id.
	/// @return `true` if the activity was set, `false` in case of errors.
	[[nodiscard]] static bool SetActivityId(_Out_ GUID& activityId) noexcept;

	/// @brief Resets the activity id of the current thread.
	/// @remarks @p activityId is copied because the function of the Windows API requires a non-const argument.
	/// @param activityId The activity id to restore.
	static void ResetActivityId(GUID activityId) noexcept;

private:
	static constinit const Priority kLevel;  ///< @brief The log level of the logger.
	static constinit const GUID kGuid;       ///<@ brief The `GUID` of the log provider for the Windows event log.

	/// @brief Stores event ids handled by the current thread to prevent infinite loops.
	static thread_local inline USHORT s_logging[4] = {0};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): Thread local variable.

private:
	REGHANDLE m_handle = 0;  ///< @brief Handle of the windows event logger.
};

}  // namespace m3c
