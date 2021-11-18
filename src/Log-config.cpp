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

#include "m3c/Log.h"
#include "m3c/LogArgs.h"
#include "m3c/LogData.h"
#include "m3c/finally.h"

#include <windows.h>
#include <evntprov.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef M3C_LOG_LEVEL
#ifdef _DEBUG
#pragma message("M3C_LOG_LEVEL not defined - using Priority::kDebug for debug build")
#define M3C_LOG_LEVEL Debug
#else
#pragma message("M3C_LOG_LEVEL not defined - using Priority::kInfo for release build")
#define M3C_LOG_LEVEL Info
#endif
#endif

#define M3C_LOG_LEVEL_ENUM_(name_) k##name_
#define M3C_LOG_LEVEL_ENUM(name_) M3C_LOG_LEVEL_ENUM_(name_)

#ifndef M3C_LOG_OUTPUT_PRINT
#define M3C_LOG_OUTPUT_PRINT 0
#endif
#ifndef M3C_LOG_OUTPUT_EVENT
#define M3C_LOG_OUTPUT_EVENT 0
#endif

namespace m3c {

namespace {

/// @brief The log level defined at compile-time.
constexpr Priority kMinimumLevel = Priority::M3C_LOG_LEVEL_ENUM(M3C_LOG_LEVEL);


constexpr bool kOutputPrint = M3C_LOG_OUTPUT_PRINT;  ///< @brief kEvent `true` if the logger writes to the print output.
constexpr bool kOutputEvent = M3C_LOG_OUTPUT_EVENT;  ///< @brief kEvent `true` if the logger writes to the Windows event log.

}  // namespace

constinit const Priority Log::kLevel = kMinimumLevel;

//
// Logger
//

Log::Log() noexcept {
	if constexpr (kOutputEvent) {
		assert(!IsEqualGUID(kGuid, {0}));  // log events but no provider defined
		RegisterEvents();
	}
}

Log::~Log() noexcept {
	if constexpr (kOutputEvent) {
		UnregisterEvents();
	}
}

template <internal::LogMessage M>
void Log::DoLogMessage(const Priority priority, const internal::LogContext<M>& context, _In_z_ const char* const cause, const internal::ClosureExcept<void, LogFormatArgs&>&& print, const internal::ClosureExcept<void, LogEventArgs&>&& event) {
#ifdef _DEBUG
	std::size_t assertArgCount;
#endif
	const std::source_location& sourceLocation = context.GetSourceLocation();
	if constexpr (kOutputPrint) {
		LogFormatArgs formatArgs;
		print(formatArgs);
#ifdef _DEBUG
		assertArgCount = formatArgs.size();
#endif
		if constexpr (internal::LogContext<M>::kIsEventDescriptor) {
			Print(priority, internal::GetEventMessagePattern(context.GetLogMessage().Id).c_str(), formatArgs, cause, sourceLocation);
		} else {
			Print(priority, context.GetLogMessage(), formatArgs, cause, sourceLocation);
		}
	}
	if constexpr (kOutputEvent && internal::LogContext<M>::kIsEventDescriptor) {
		EVENT_DESCRIPTOR eventDesc(context.GetLogMessage());
		EventDescSetLevel(&eventDesc, static_cast<std::underlying_type_t<Priority>>(priority));
		LogEventArgs eventArgs;
		event(eventArgs);
#ifdef _DEBUG
		if constexpr (kOutputPrint) {
			assert(assertArgCount == eventArgs.size());
		}
#endif

		const char* const fileName = sourceLocation.file_name();
		const std::uint_least32_t line = sourceLocation.line();
		eventArgs << fileName << line;
		WriteEvent(eventDesc, eventArgs);
	}
}

template void Log::DoLogMessage(Priority, const internal::LogContext<const EVENT_DESCRIPTOR&>&, const char*, const internal::ClosureExcept<void, LogFormatArgs&>&&, const internal::ClosureExcept<void, LogEventArgs&>&&);
template void Log::DoLogMessage(Priority, const internal::LogContext<const char*>&, const char*, const internal::ClosureExcept<void, LogFormatArgs&>&&, const internal::ClosureExcept<void, LogEventArgs&>&&);

template <internal::LogMessage M>
void Log::DoLogException(const Priority priority, const internal::LogContext<M>& context, const internal::ClosureExcept<void, LogFormatArgs&>&& print, const internal::ClosureExcept<void, LogEventArgs&>&& event) {
	if constexpr (kOutputPrint || (kOutputEvent && internal::LogContext<M>::kIsEventDescriptor)) {
		std::string cause;
		GUID activityId;
		bool mustResetActivityId;
		if constexpr (kOutputEvent && internal::LogContext<M>::kIsEventDescriptor) {
			mustResetActivityId = SetActivityId(activityId);
		}
		auto guard = finally([&activityId, mustResetActivityId]() noexcept {
			if constexpr (kOutputEvent && internal::LogContext<M>::kIsEventDescriptor) {
				if (mustResetActivityId) {
					ResetActivityId(activityId);
				}
			}
		});
		WriteException<kOutputPrint, kOutputEvent && internal::LogContext<M>::kIsEventDescriptor>(priority, activityId, cause);
		DoLogMessage(priority, context, cause.c_str(), std::move(print), std::move(event));
	}
}

template void Log::DoLogException(Priority, const internal::LogContext<const EVENT_DESCRIPTOR&>&, const internal::ClosureExcept<void, LogFormatArgs&>&&, const internal::ClosureExcept<void, LogEventArgs&>&&);
template void Log::DoLogException(Priority, const internal::LogContext<const char*>&, const internal::ClosureExcept<void, LogFormatArgs&>&&, const internal::ClosureExcept<void, LogEventArgs&>&&);

}  // namespace m3c
