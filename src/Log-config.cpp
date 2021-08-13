#include "m3c/Log.h"
#include "m3c/LogData.h"
#include "m3c/finally.h"

#include <windows.h>
#include <evntprov.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef M3C_LOG_LEVEL
#define M3C_LOG_LEVEL Info
#endif
#define M3C_LOG_LEVEL_ENUM_(name_) k##name_
#define M3C_LOG_LEVEL_ENUM(name_) M3C_LOG_LEVEL_ENUM_(name_)

#ifndef M3C_LOG_OUTPUT_DEBUG
#define M3C_LOG_OUTPUT_DEBUG 0
#endif
#ifndef M3C_LOG_OUTPUT_STDERR
#define M3C_LOG_OUTPUT_STDERR 0
#endif
#ifndef M3C_LOG_OUTPUT_EVENT
#define M3C_LOG_OUTPUT_EVENT 0
#endif

namespace m3c {

namespace {
constexpr Priority kMinimumLevel = Priority::M3C_LOG_LEVEL_ENUM(M3C_LOG_LEVEL);

constexpr bool kOutputDebug = M3C_LOG_OUTPUT_DEBUG;
constexpr bool kOutputStderr = M3C_LOG_OUTPUT_STDERR;
constexpr bool kOutputEvent = M3C_LOG_OUTPUT_EVENT;

}  // namespace

namespace internal {
std::string GetEventMessagePattern(const USHORT eventId);
}

//
// Logger
//

Log::Log(const GUID& guid) noexcept {
	if constexpr (kOutputEvent) {
		RegisterEvents(guid);
	}
}

Log::~Log() noexcept {
	if constexpr (kOutputEvent) {
		UnregisterEvents();
	}
}

template <internal::EventInfo E>
void Log::DoLogEvent(const Priority priority, const internal::EventContext<E>& context, _In_z_ const char* const cause, internal::Thunk<internal::LogData&>&& print, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&& evt, const ULONG count) {
	if (priority > kMinimumLevel) {
		return;
	}
	if constexpr (kOutputDebug || kOutputStderr) {
		internal::LogData logData;
		print.thunk(print.function, logData);
		if constexpr (internal::EventContext<E>::kIsEventDescriptor) {
			Print<kOutputDebug, kOutputStderr>(priority, internal::GetEventMessagePattern(context.GetEvent().Id).c_str(), logData, cause, context.GetSourceLocation());
		} else {
			Print<kOutputDebug, kOutputStderr>(priority, context.GetEvent(), logData, cause, context.GetSourceLocation());
		}
	}
	if constexpr (kOutputEvent && internal::EventContext<E>::kIsEventDescriptor) {
		EVENT_DESCRIPTOR event(context.GetEvent());
		EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
		std::vector<EVENT_DATA_DESCRIPTOR> data(count);
		evt.thunk(evt.function, data.data());
		WriteEvent(event, count, data.data());
	}
}

template void Log::DoLogEvent(Priority, const internal::EventContext<const EVENT_DESCRIPTOR&>&, const char*, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);
template void Log::DoLogEvent(Priority, const internal::EventContext<const char*>&, const char*, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);

template <internal::EventInfo E>
void Log::DoLogExceptionEvent(const Priority priority, const internal::EventContext<E>& context, internal::Thunk<internal::LogData&>&& print, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&& event, const ULONG count) {
	if (priority > kMinimumLevel) {
		return;
	}
	if constexpr (kOutputDebug || kOutputStderr || (kOutputEvent && internal::EventContext<E>::kIsEventDescriptor)) {
		std::string cause;
		GUID activityId;
		bool mustResetActivityId;
		if constexpr (kOutputEvent && internal::EventContext<E>::kIsEventDescriptor) {
			mustResetActivityId = SetActivityId(activityId);
		}
		auto guard = finally([&activityId, mustResetActivityId]() noexcept {
			if constexpr (kOutputEvent && internal::EventContext<E>::kIsEventDescriptor) {
				if (mustResetActivityId) {
					ResetActivityId(activityId);
				}
			}
		});
		DoLogException<kOutputDebug || kOutputStderr, kOutputEvent && internal::EventContext<E>::kIsEventDescriptor>(priority, activityId, cause);
		DoLogEvent(priority, context, cause.c_str(), std::move(print), std::move(event), count);
	}
}

template void Log::DoLogExceptionEvent(Priority, const internal::EventContext<const EVENT_DESCRIPTOR&>&, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);
template void Log::DoLogExceptionEvent(Priority, const internal::EventContext<const char*>&, internal::Thunk<internal::LogData&>&&, internal::Thunk<EVENT_DATA_DESCRIPTOR*>&&, ULONG);

}  // namespace m3c
