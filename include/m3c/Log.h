#pragma once

#include "LogData.h"
#include "source_location.h"
#include "type_traits.h"

#include <windows.h>
#include <evntprov.h>
#include <winmeta.h>

#include <cstddef>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>

namespace m3c {

namespace internal {

template <typename E>
concept EventInfo = std::is_same_v<E, const EVENT_DESCRIPTOR&> || std::is_same_v<E, const char*>;

template <EventInfo E>
class EventContext {
public:
	static constexpr bool kIsEventDescriptor = std::is_same_v<E, const EVENT_DESCRIPTOR&>;

public:
	[[nodiscard]] constexpr EventContext(const EventContext&) noexcept = delete;

public:
	[[nodiscard]] constexpr EventContext(E event, source_location sourceLocation = source_location::current()) noexcept
		: m_event(event)
		, m_sourceLocation(std::move(sourceLocation)) {
	}

	template <typename X = E, typename std::enable_if_t<std::is_same_v<X, const char*>, int> = 0>
	[[nodiscard]] constexpr EventContext(const std::string& event, source_location sourceLocation = source_location::current()) noexcept
		: m_event(event.c_str())
		, m_sourceLocation(std::move(sourceLocation)) {
	}

	template <typename X = E, typename std::enable_if_t<std::is_same_v<X, const char*>, int> = 0>
	[[nodiscard]] constexpr EventContext(std::string&& event, source_location sourceLocation = source_location::current()) noexcept
		: m_event(event.c_str())
		, m_sourceLocation(std::move(sourceLocation)) {
	}

public:
	[[nodiscard]] constexpr const E GetEvent() const noexcept {
		return m_event;
	}
	[[nodiscard]] constexpr const source_location& GetSourceLocation() const noexcept {
		return m_sourceLocation;
	}

private:
	E m_event;
	source_location m_sourceLocation;
};

extern template class EventContext<const EVENT_DESCRIPTOR&>;
extern template class EventContext<const char*>;
extern template EventContext<const char*>::EventContext(const std::string&, source_location) noexcept;

template <typename... Args>
struct Thunk {
	void (*thunk)(void*, Args...);
	void* function;
};

}  // namespace internal

//
// Logger
//

/// @brief Enum for different log priorities.
/// @note Priorities MUST be divisible by 4 because the values `| 1`, `| 2` and `| 3` are reserved for internal use.
/// @copyright This enum is based on `enum class LogLevel` from NanoLog.
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

class Log {
public:
	Log() noexcept = default;
	Log(const Log&) = delete;
	Log(Log&& log) noexcept;
	Log(const GUID& guid) noexcept;
	~Log() noexcept;

public:
	Log& operator=(const Log&) = delete;
	Log& operator=(Log&& log) noexcept;

public:
	template <internal::EventInfo E, typename... Args>
	void LogEvent(const Priority priority, const internal::EventContext<E>& context, Args&&... args) noexcept {
		try {
			auto print = GetDebugCapture(args...);
			auto event = GetEventCapture<internal::EventContext<E>::kIsEventDescriptor>(args...);

			DoLogEvent(priority, context, "",
			           {[](void* fn, internal::LogData& logData) {
							(*static_cast<decltype(print)*>(fn))(logData);
						},
			            &print},
			           {[](void* fn, EVENT_DATA_DESCRIPTOR* data) noexcept {
							(*static_cast<decltype(event)*>(fn))(data);
						},
			            &event},
			           static_cast<ULONG>(sizeof...(args)));
		} catch (...) {
			LogInternalError<E>(context.GetEvent());
		}
	}

	// template <internal::EventInfo E, typename... Args>
	// std::tuple_element_t<0, std::tuple<Args..., void>> LogExceptionEvent(const Priority priority, const internal::ExceptionEventContext<E>& context, Args&&... args) noexcept {
	template <internal::EventInfo E, typename... Args>
	void LogExceptionEvent(const Priority priority, const internal::EventContext<E>& context, Args&&... args) noexcept {
		try {
			auto print = GetDebugCapture(args...);
			auto event = GetEventCapture<internal::EventContext<E>::kIsEventDescriptor>(args...);

			DoLogExceptionEvent(priority, context,
			                    {[](void* fn, internal::LogData& logData) {
									 (*static_cast<decltype(print)*>(fn))(logData);
								 },
			                     &print},
			                    {[](void* fn, EVENT_DATA_DESCRIPTOR* data) noexcept {
									 (*static_cast<decltype(event)*>(fn))(data);
								 },
			                     &event},
			                    static_cast<ULONG>(sizeof...(args)));
		} catch (...) {
			LogInternalError<E>(context.GetEvent());
		}
	}

public:
	static void Register(const GUID& guid) noexcept {
		m_log = Log(guid);
	}
#if 0
	template <typename... Args>
	static void Event(const Priority priority, const internal::EventContext<const EVENT_DESCRIPTOR&>&& context, Args&&... args) noexcept {
		m_log.LogEvent(priority, context, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void Event(const Priority priority, const internal::EventContext<const char*>&& context, Args&&... args) noexcept {
		m_log.LogEvent(priority, context, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void Exception(const Priority priority, const internal::ExceptionEventContext<const EVENT_DESCRIPTOR&>&& context, Args&&... args) noexcept {
		m_log.LogExceptionEvent(priority, context, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void Exception(const Priority priority, const internal::ExceptionEventContext<const char*>&& context, Args&&... args) noexcept {
		m_log.LogExceptionEvent(priority, context, std::forward<Args>(args)...);
	}

#endif

#pragma push_macro("MAIN_METHOD")
#pragma push_macro("LEVEL_METHOD")
#pragma push_macro("FOR_EACH_PRIORITY")
#pragma push_macro("FOR_EACH_CASE")
#pragma push_macro("FOR_EACH_TYPE")
#undef MAIN_METHOD
#undef LEVEL_METHOD
#undef FOR_EACH_PRIORITY
#undef FOR_EACH_CASE
#undef FOR_EACH_TYPE

#define MAIN_METHOD(name_, type_, exception_)                                                                                        \
	template <typename... Args>                                                                                                      \
	static void name_##exception_(const Priority priority, const internal::EventContext<type_>&& context, Args&&... args) noexcept { \
		m_log.Log##exception_##Event(priority, context, std::forward<Args>(args)...);                                                \
	}

#define LEVEL_METHOD(priority_, type_, exception_)                                                                    \
	template <typename... Args>                                                                                       \
	static void priority_##exception_(const internal::EventContext<type_>&& context, Args&&... args) noexcept {       \
		m_log.Log##exception_##Event(Priority::k##priority_, context, std::forward<Args>(args)...);                   \
	}                                                                                                                 \
	template <typename... Args>                                                                                       \
	static void priority_##exception_##Once(const internal::EventContext<type_>&& context, Args&&... args) noexcept { \
		const USHORT eventId = context.GetEvent().Id;                                                                 \
		const std::uint_fast8_t index = LogOnce(eventId);                                                             \
		if (index != kNoLogging) [[likely]] {                                                                         \
			m_logging[index] = eventId;                                                                               \
			m_log.Log##exception_##Event(Priority::k##priority_, context, std::forward<Args>(args)...);               \
			m_logging[index] = 0;                                                                                     \
		}                                                                                                             \
	}

#define FOR_EACH_PRIORITY(type_, exception_)  \
	LEVEL_METHOD(Critical, type_, exception_) \
	LEVEL_METHOD(Error, type_, exception_)    \
	LEVEL_METHOD(Warning, type_, exception_)  \
	LEVEL_METHOD(Info, type_, exception_)     \
	LEVEL_METHOD(Verbose, type_, exception_)  \
	LEVEL_METHOD(Debug, type_, exception_)    \
	LEVEL_METHOD(Trace, type_, exception_)

#define FOR_EACH_CASE(type_)        \
	MAIN_METHOD(Event, type_, )     \
	MAIN_METHOD(, type_, Exception) \
	FOR_EACH_PRIORITY(type_, )      \
	FOR_EACH_PRIORITY(type_, Exception)

#define FOR_EACH_TYPE()                    \
	FOR_EACH_CASE(const EVENT_DESCRIPTOR&) \
	FOR_EACH_CASE(const char*)

	FOR_EACH_TYPE()

#pragma pop_macro("MAIN_METHOD")
#pragma pop_macro("LEVEL_METHOD")
#pragma pop_macro("FOR_EACH_PRIORITY")
#pragma pop_macro("FOR_EACH_CASE")
#pragma pop_macro("FOR_EACH_TYPE")

private:
	void RegisterEvents(const GUID& guid) noexcept;
	void UnregisterEvents() noexcept;

	template <internal::EventInfo E>
	void LogInternalError(E event) noexcept;

	template <typename... Args>
	constexpr auto GetDebugCapture(const Args&... args) {
		return [&args...](internal::LogData& logData) {
			(logData << ... << args);
		};
	}
	template <bool kEvent, typename... Args>
	constexpr auto GetEventCapture(const Args&... args) noexcept {
		return [&args...]([[maybe_unused]] EVENT_DATA_DESCRIPTOR* data) noexcept {
			if constexpr (kEvent && sizeof...(Args)) {
				int i = 0;
				(Set(data[i++], static_cast<const std::conditional_t<std::is_volatile_v<Args>, std::remove_volatile_t<Args>, Args>>(args)), ...);
			}
		};
	}

	template <internal::EventInfo E>
	void DoLogEvent(Priority priority, const internal::EventContext<E>& context, _In_z_ const char* cause, internal::Thunk<internal::LogData&>&& print, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&& event, ULONG count);
	template <internal::EventInfo E>
	void DoLogExceptionEvent(Priority priority, const internal::EventContext<E>& context, internal::Thunk<internal::LogData&>&& print, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&& event, ULONG count);

	void WriteEvent(const EVENT_DESCRIPTOR& event, _In_range_(0, MAX_EVENT_DATA_DESCRIPTORS) const ULONG count, _In_reads_(count) EVENT_DATA_DESCRIPTOR* data, _In_opt_ const GUID* pRelatedActivityId = nullptr);
	template <bool kDebug, bool kStderr>
	void Print(Priority priority, const char* pattern, const internal::LogData& logData, _In_z_ const char* cause, const source_location& sourceLocation) noexcept(!kDebug && !kStderr);

	template <bool kPrint, bool kEvent>
	void DoLogException(Priority priority, const GUID& activityId, _Inout_ std::string& cause);
	template <bool kPrint, bool kEvent>
	void WriteExceptionEvent(Priority priority, const GUID& relatedActivityId, _Inout_ std::string& cause);

	static std::uint_fast8_t LogOnce(USHORT eventId) noexcept;

	template <typename T>
	static void Set(EVENT_DATA_DESCRIPTOR& data, const T& arg) noexcept requires(is_any_v<std::remove_cvref_t<T>, char, signed char, unsigned char, signed short, unsigned short, signed int, unsigned int, signed long, unsigned long, signed long long, unsigned long long, float, double, bool, GUID, const void*, std::nullptr_t, FILETIME, SYSTEMTIME, SID>) {
		EventDataDescCreate(&data, &arg, sizeof(arg));
	}

	static void Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const char* arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const wchar_t* arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, const std::string& arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, const std::wstring& arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, const internal::win32_error& arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, const internal::rpc_status& arg) noexcept;
	static void Set(EVENT_DATA_DESCRIPTOR& data, const internal::hresult& arg) noexcept;

	static bool SetActivityId(_Out_ GUID& activityId) noexcept;
	static void ResetActivityId(GUID activityId) noexcept;

private:
	static Log m_log;
	static inline thread_local USHORT m_logging[4] = {0};
	static constexpr std::uint_fast8_t kNoLogging = sizeof(m_logging) / sizeof(m_logging[0]);
	REGHANDLE m_handle = 0;
};

extern template void Log::DoLogEvent(Priority, const internal::EventContext<const EVENT_DESCRIPTOR&>&, const char*, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);
extern template void Log::DoLogEvent(Priority, const internal::EventContext<const char*>&, const char*, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);
extern template void Log::DoLogExceptionEvent(Priority, const internal::EventContext<const EVENT_DESCRIPTOR&>&, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);
extern template void Log::DoLogExceptionEvent(Priority, const internal::EventContext<const char*>&, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);

extern template void Log::LogInternalError(const EVENT_DESCRIPTOR&) noexcept;
extern template void Log::LogInternalError(const char*) noexcept;

extern template void Log::Print<false, false>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(true);
extern template void Log::Print<false, true>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);
extern template void Log::Print<true, false>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);
extern template void Log::Print<true, true>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);

extern template void Log::DoLogException<false, false>(Priority, const GUID&, std::string&);
extern template void Log::DoLogException<false, true>(Priority, const GUID&, std::string&);
extern template void Log::DoLogException<true, false>(Priority, const GUID&, std::string&);
extern template void Log::DoLogException<true, true>(Priority, const GUID&, std::string&);

extern template void Log::WriteExceptionEvent<false, false>(Priority, const GUID&, std::string&);
extern template void Log::WriteExceptionEvent<false, true>(Priority, const GUID&, std::string&);
extern template void Log::WriteExceptionEvent<true, false>(Priority, const GUID&, std::string&);
extern template void Log::WriteExceptionEvent<true, true>(Priority, const GUID&, std::string&);

}  // namespace m3c

#if _PREFAST_
#define LOG_TRACE_RESULT(rv_, ...) (rv_)
#else
#define LOG_TRACE_RESULT(rv_, context_, ...)          \
	[&](auto&& rv) noexcept {                         \
		m3c::Log::Trace((context_), rv, __VA_ARGS__); \
		return std::forward<decltype(rv)>(rv);        \
	}((rv_))

#define LOG_TRACE_HRESULT(rv_, context_, ...)                             \
	[&](const HRESULT rv) noexcept {                                      \
		m3c::Log::Trace((context_), static_cast<DWORD>(rv), __VA_ARGS__); \
		return rv;                                                        \
	}((rv_))
#endif
