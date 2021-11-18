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

#include "m3c/Log.h"

#include "m3c/PropVariant.h"
#include "m3c/exception.h"
#include <m3c/source_location.h>

#include <m4t/m4t.h>

#include "test.events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>
#include <detours_gmock.h>
#include <evntprov.h>
#include <propvarutil.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#ifndef M3C_LOG_LEVEL
#define M3C_LOG_LEVEL Info
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

static std::ostream& operator<<(std::ostream& os, const Priority priority) {
	std::string name;
	switch (priority) {
	case Priority::kNone:
		name = "None";
		break;
	case Priority::kCritical:
		name = "Critical";
		break;
	case Priority::kError:
		name = "Error";
		break;
	case Priority::kWarning:
		name = "Warning";
		break;
	case Priority::kInfo:
		name = "Info";
		break;
	case Priority::kVerbose:
		name = "Verbose";
		break;
	case Priority::kDebug:
		name = "Debug";
		break;
	case Priority::kTrace:
		name = "Trace";
		break;
	default:
		throw std::out_of_range(FMT_FORMAT("priority {}", priority));
	}
	return os << name;
}

}  // namespace m3c

std::ostream& operator<<(std::ostream& os, const EVENT_DESCRIPTOR* const pEventDescriptor) {
	if (pEventDescriptor) {
		return os << "(event=" << pEventDescriptor->Id << ", level=" << static_cast<m3c::Priority>(pEventDescriptor->Level) << ", keyword=0x" << std::hex << pEventDescriptor->Keyword << ")";
	}
	return os << "(event=nullptr)";
}

namespace m3c::test {
namespace {

namespace t = testing;

#undef WIN32_FUNCTIONS
#define WIN32_FUNCTIONS(fn_)                                                                                                                                                                         \
	fn_(1, void, WINAPI, OutputDebugStringA,                                                                                                                                                         \
	    (LPCSTR lpOutputString),                                                                                                                                                                     \
	    (lpOutputString),                                                                                                                                                                            \
	    nullptr);                                                                                                                                                                                    \
	fn_(8, ULONG, __stdcall, EventWriteEx,                                                                                                                                                           \
	    (REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor, ULONG64 Filter, ULONG Flags, LPCGUID ActivityId, LPCGUID RelatedActivityId, ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData), \
	    (RegHandle, EventDescriptor, Filter, Flags, ActivityId, RelatedActivityId, UserDataCount, UserData),                                                                                         \
	    nullptr);                                                                                                                                                                                    \
	fn_(2, int, __cdecl, fputs,                                                                                                                                                                      \
	    (const char* str, FILE* stream),                                                                                                                                                             \
	    (str, stream),                                                                                                                                                                               \
	    nullptr)

constexpr Priority kMinimumLevel = Priority::M3C_LOG_LEVEL_ENUM(M3C_LOG_LEVEL);

constexpr bool kOutputPrint = M3C_LOG_OUTPUT_PRINT;
constexpr bool kOutputEvent = M3C_LOG_OUTPUT_EVENT;

enum class EventType : std::uint_fast8_t {
	kEvent = 0,
	kCharPtr = 1,
	kString = 2,
	kStringRef = 3
};

std::ostream& operator<<(std::ostream& os, const EventType eventType) {
	std::string name;
	switch (eventType) {
	case EventType::kEvent:
		name = "Event";
		break;
	case EventType::kCharPtr:
		name = "char_ptr";
		break;
	case EventType::kString:
		name = "string";
		break;
	case EventType::kStringRef:
		name = "string_ref";
		break;
	default:
		throw std::out_of_range(FMT_FORMAT("eventType {}", eventType));
	}
	return os << name;
}

class Log_Test : public t::Test {
protected:
	virtual EventType GetEventType() const = 0;
	virtual Priority GetLevel() const = 0;

	std::string GetLevelOutput() const {
		const Priority priority = GetLevel();
		switch (priority) {
		case Priority::kCritical:
			return "Critical";
		case Priority::kError:
			return "Error";
		case Priority::kWarning:
			return "Warning";
		case Priority::kInfo:
			return "Information";
		case Priority::kVerbose:
			return "Verbose";
		case Priority::kDebug:
			return "Debug";
		case Priority::kTrace:
			return "Trace";
		default:
			throw std::out_of_range(FMT_FORMAT("priority {}", priority));
		}
	}

	bool IsLogged() const {
		return GetLevel() <= kMinimumLevel;
	}

	void SetUp() override {
		ON_CALL(m_win32, OutputDebugStringA(t::StartsWith("[")))
		    .WillByDefault(t::Invoke([this](LPCSTR lpOutputString) {
			    m_debug += lpOutputString;
			    m_win32.DTGM_Real_OutputDebugStringA(lpOutputString);
		    }));
		ON_CALL(m_win32, fputs(m4t::MatchesRegex("^\\d{4}-[\\s\\S]*"), stderr))
		    .WillByDefault(t::Invoke([this](const char* const str, FILE* const stream) {
			    m_stderr += str;
			    return m_win32.DTGM_Real_fputs(str, stream);
		    }));
	}

protected:
	std::string GetLogEventOutput(const char* const message, const char* const dynamicLogMessage, const std::source_location& loc, const std::uint32_t line) {
		return FMT_FORMAT("[{}] [{}] {}{}\n\tat {}({}) ({})\n", GetLevelOutput(), GetCurrentThreadId(), message, dynamicLogMessage, loc.file_name(), line, loc.function_name());
	}

	static std::string GetExceptionOutput(const char* const message, const std::source_location& loc, const std::uint32_t line) {
		return fmt::format(fmt::runtime(std::string("\tcaused by: {}\n") + (line ? "\t\tat {}({}) ({})\n" : "")), message, loc.file_name(), line, loc.function_name());
	}

	template <EVENT_DESCRIPTOR kLoggedExceptionEvent = EVENT_DESCRIPTOR{}, bool kNested = false, bool kHResult = false, typename E = int, typename D = int, typename... Args>
	auto TestMethod(const char* const dynamicLogMessage = "", E&& exception = 0, const char* const exceptionMessage = nullptr, const D detail = 0, Args&&... args) {
		constexpr bool kException = !std::is_same_v<E, int>;
		constexpr bool kDetail = !std::is_same_v<D, int>;
		const EventType eventType = GetEventType();
		t::Sequence seq;

		EXPECT_CALL(m_win32, OutputDebugStringA(t::StartsWith("[")))
		    .Times(kOutputPrint && IsLogged() ? 1 : 0);

		EXPECT_CALL(m_win32, fputs(m4t::MatchesRegex("^\\d{4}-[\\s\\S]*"), stderr))
		    .Times(kOutputPrint && IsLogged() ? 1 : 0);

		EXPECT_CALL(m_win32, EventWriteEx)
		    .Times(0);
		if constexpr (kOutputEvent) {
			if (eventType == EventType::kEvent) {
				if constexpr (kNested) {
					EXPECT_CALL(m_win32, EventWriteEx(t::_,
					                                  t::AllOf(t::Field(&EVENT_DESCRIPTOR::Id, evt::std_exception.Id),
					                                           t::Field(&EVENT_DESCRIPTOR::Level, static_cast<std::underlying_type_t<Priority>>(GetLevel())),
					                                           t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_EXCEPTION))),
					                                  DTGM_ARG6))
					    .Times(IsLogged() ? 1 : 0)
					    .InSequence(seq);
				}
				if constexpr (kException) {
					EXPECT_CALL(m_win32, EventWriteEx(t::_,
					                                  t::AllOf(t::Field(&EVENT_DESCRIPTOR::Id, kLoggedExceptionEvent.Id),
					                                           t::Field(&EVENT_DESCRIPTOR::Level, static_cast<std::underlying_type_t<Priority>>(GetLevel())),
					                                           t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_EXCEPTION))),
					                                  DTGM_ARG6))
					    .Times(IsLogged() ? 1 : 0)
					    .InSequence(seq);
				}
				EXPECT_CALL(m_win32, EventWriteEx(t::_,
				                                  t::AllOf(t::Field(&EVENT_DESCRIPTOR::Id, !kHResult ? evt::Test_Event_String.Id : evt::Test_Event_String_H.Id),
				                                           t::Field(&EVENT_DESCRIPTOR::Level, static_cast<std::underlying_type_t<Priority>>(GetLevel())),
				                                           t::Field(&EVENT_DESCRIPTOR::Keyword, t::Not(m4t::BitsSet(M3C_KEYWORD_EXCEPTION)))),
				                                  DTGM_ARG4, !kHResult ? 3 : 4, t::_))
				    .Times(IsLogged() ? 1 : 0)
				    .InSequence(seq);
			}
		}


		std::uint32_t exceptionLine = 0;
		std::uint32_t innerExceptionLine = 0;
		std::uint32_t line = 0;
		std::source_location exceptionLocation;
		std::source_location innerExceptionLocation;
		std::source_location location;
		HRESULT hr = S_OK;

		if constexpr (!kException) {
			m4t::WithLocale("en-US", [eventType, level = GetLevel(), &line, &location] {
				switch (eventType) {
				case EventType::kEvent:
					line = (location = std::source_location::current()).line() + 1;
					Log::Message(level, evt::Test_Event_String, "mymessage");
					break;
				case EventType::kCharPtr:
					line = (location = std::source_location::current()).line() + 1;
					Log::Message(level, "zstring event with string {}", "mymessage");
					break;
				case EventType::kString:
					line = (location = std::source_location::current()).line() + 1;
					Log::Message(level, std::string("string event with string {}"), "mymessage");
					break;
				case EventType::kStringRef: {
					const std::string str = "string& event with string {}";
					line = (location = std::source_location::current()).line() + 1;
					Log::Message(level, str, "mymessage");
					break;
				}
				default:
					throw std::out_of_range(FMT_FORMAT("eventType {}", eventType));
				}
			});
		} else {
			try {
				if constexpr (!kDetail) {
					if constexpr (!kNested) {
						throw std::forward<E>(exception);  // NOLINT(hicpp-exception-baseclass): Allow throwing non-exceptions for testing purposes.
					} else {
						try {
							throw std::exception("inner cause");
						} catch (...) {
							std::throw_with_nested(std::forward<E>(exception));
						}
					}
				} else {
					if constexpr (!kNested) {
						exceptionLine = (exceptionLocation = std::source_location::current()).line() + 1;
						throw((std::forward<E>(exception) + detail) << ... << std::forward<Args>(args));
					} else {
						try {
							innerExceptionLine = (innerExceptionLocation = std::source_location::current()).line() + 1;
							throw std::exception("inner cause") + evt::Default;
						} catch (...) {
							exceptionLine = (exceptionLocation = std::source_location::current()).line() + 1;
							std::throw_with_nested(((std::forward<E>(exception) + detail) << ... << std::forward<Args>(args)));
						}
					}
				}
			} catch (...) {
				if constexpr (!kHResult) {
					m4t::WithLocale("en-US", [eventType, level = GetLevel(), &line, &location] {
						switch (eventType) {
						case EventType::kEvent:
							line = (location = std::source_location::current()).line() + 1;
							Log::Exception(level, evt::Test_Event_String, "mymessage");
							break;
						case EventType::kCharPtr:
							line = (location = std::source_location::current()).line() + 1;
							Log::Exception(level, "zstring event with string {}", "mymessage");
							break;
						case EventType::kString:
							line = (location = std::source_location::current()).line() + 1;
							Log::Exception(level, "string event with string {}", "mymessage");
							break;
						case EventType::kStringRef: {
							const std::string str = "string& event with string {}";
							line = (location = std::source_location::current()).line() + 1;
							Log::Exception(level, str, "mymessage");
							break;
						}
						default:
							throw std::out_of_range(FMT_FORMAT("eventType {}", eventType));
						}
					});
				} else {
					m4t::WithLocale("en-US", [eventType, level = GetLevel(), &line, &location, &hr] {
						switch (eventType) {
						case EventType::kEvent:
							line = (location = std::source_location::current()).line() + 1;
							hr = Log::ExceptionToHResult(level, evt::Test_Event_String_H, "mymessage");
							break;
						case EventType::kCharPtr:
							line = (location = std::source_location::current()).line() + 1;
							hr = Log::ExceptionToHResult(level, "zstring event with string {} and error: {}", "mymessage");
							break;
						case EventType::kString:
							line = (location = std::source_location::current()).line() + 1;
							hr = Log::ExceptionToHResult(level, "string event with string {} and error: {}", "mymessage");
							break;
						case EventType::kStringRef: {
							const std::string str = "string& event with string {} and error: {}";
							line = (location = std::source_location::current()).line() + 1;
							hr = Log::ExceptionToHResult(level, str, "mymessage");
							break;
						}
						default:
							throw std::out_of_range(FMT_FORMAT("eventType {}", eventType));
						}
					});
				}
			}
		}

		std::string logOutput;
		switch (eventType) {
		case EventType::kEvent:
			logOutput = GetLogEventOutput(!kHResult ? "Testing event with string mymessage" : "Testing event with string mymessage and error", dynamicLogMessage, location, line);
			break;
		case EventType::kCharPtr:
			logOutput = GetLogEventOutput(!kHResult ? "zstring event with string mymessage" : "zstring event with string mymessage and error", dynamicLogMessage, location, line);
			break;
		case EventType::kString:
			logOutput = GetLogEventOutput(!kHResult ? "string event with string mymessage" : "string event with string mymessage and error", dynamicLogMessage, location, line);
			break;
		case EventType::kStringRef:
			logOutput = GetLogEventOutput(!kHResult ? "string& event with string mymessage" : "string& event with string mymessage and error", dynamicLogMessage, location, line);
			break;
		default:
			throw std::out_of_range(FMT_FORMAT("eventType {}", eventType));
		}
		if constexpr (kException) {
			logOutput += GetExceptionOutput(exceptionMessage, exceptionLocation, exceptionLine);
			if constexpr (kNested) {
				logOutput += GetExceptionOutput("inner cause", innerExceptionLocation, innerExceptionLine);
			}
		}

#pragma warning(suppress : 4127)
		if (kOutputPrint && IsLogged()) {
			EXPECT_EQ(logOutput, m_debug);

			EXPECT_THAT(m_stderr, m4t::MatchesRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}.\\d{3} [\\s\\S]+"));
			EXPECT_EQ(logOutput, m_stderr.substr(24));
		} else {
			EXPECT_EQ("", m_debug);
			EXPECT_EQ("", m_stderr);
		}

		if constexpr (kHResult) {
			return hr;
		}
	}

protected:
	DTGM_API_MOCK(m_win32, WIN32_FUNCTIONS);
	std::string m_debug;
	std::string m_stderr;
};

class LogAll_Test : public Log_Test
    , public t::WithParamInterface<std::tuple<EventType, Priority>> {
protected:
	EventType GetEventType() const override {
		return std::get<0>(GetParam());
	}
	Priority GetLevel() const override {
		return std::get<1>(GetParam());
	}
};

class LogStringContexts_Test : public Log_Test
    , public t::WithParamInterface<EventType> {
protected:
	EventType GetEventType() const override {
		return GetParam();
	}
	Priority GetLevel() const noexcept override {
		return Priority::kCritical;
	}
};

class LogOutputs_Test : public Log_Test
    , public t::WithParamInterface<EventType> {
protected:
	EventType GetEventType() const override {
		return GetParam();
	}
	Priority GetLevel() const noexcept override {
		return Priority::kCritical;
	}
};

class LogDebug_Test : public Log_Test
    , public t::WithParamInterface<EventType> {
protected:
	EventType GetEventType() const override {
		return GetParam();
	}
	Priority GetLevel() const noexcept override {
		return Priority::kDebug;
	}
};

class LogEvent_Test : public Log_Test {
	EventType GetEventType() const noexcept override {
		return EventType::kEvent;
	}
	Priority GetLevel() const noexcept override {
		return Priority::kCritical;
	}
};

class LogString_Test : public Log_Test {
	EventType GetEventType() const noexcept override {
		return EventType::kCharPtr;
	}
	Priority GetLevel() const noexcept override {
		return Priority::kCritical;
	}
};

class custom_system_error : public std::system_error {
public:
	custom_system_error(const int code, const std::error_category& category, const char* const message)
	    : system_error(code, category, message)
	    , m_message(message) {
	}

	[[nodiscard]] const char* what() const noexcept override {
		return m_message;
	}

private:
	const char* m_message;
};

INSTANTIATE_TEST_SUITE_P(LogOutputs, LogOutputs_Test,
                         t::Values(EventType::kEvent, EventType::kCharPtr),
                         [](const t::TestParamInfo<LogOutputs_Test::ParamType>& param) {
	                         // if prefix param.index is missing, source link is not shown in Visual Studio
	                         return FMT_FORMAT("{}_{}", param.index, t::PrintToString(param.param));
                         });

INSTANTIATE_TEST_SUITE_P(LogOutputs, LogDebug_Test,
                         t::Values(EventType::kEvent, EventType::kCharPtr),
                         [](const t::TestParamInfo<LogDebug_Test::ParamType>& param) {
	                         // if prefix param.index is missing, source link is not shown in Visual Studio
	                         return FMT_FORMAT("{}_{}", param.index, t::PrintToString(param.param));
                         });

INSTANTIATE_TEST_SUITE_P(LogContexts, LogStringContexts_Test,
                         t::Values(EventType::kString, EventType::kStringRef),
                         [](const t::TestParamInfo<LogStringContexts_Test::ParamType>& param) {
	                         // if prefix param.index is missing, source link is not shown in Visual Studio
	                         return FMT_FORMAT("{}_{}", param.index, t::PrintToString(param.param));
                         });

INSTANTIATE_TEST_SUITE_P(LogOutputsAndPriorites, LogAll_Test,
                         t::Combine(t::Values(EventType::kEvent, EventType::kCharPtr),
                                    t::Values(Priority::kCritical,
                                              Priority::kError,
                                              Priority::kWarning,
                                              Priority::kInfo,
                                              Priority::kVerbose,
                                              Priority::kDebug,
                                              Priority::kTrace)),
                         [](const t::TestParamInfo<LogAll_Test::ParamType>& param) {
	                         // if prefix param.index is missing, source link is not shown in Visual Studio
	                         return FMT_FORMAT("{:02}_{}_{}", param.index, t::PrintToString(std::get<0>(param.param)), t::PrintToString(std::get<1>(param.param)));
                         });


//
// Basic logging
//

TEST_P(LogAll_Test, Event_PriorityIsEnabled_Log) {
	TestMethod();
}

TEST_P(LogStringContexts_Test, Event_PriorityIsEnabled_Log) {
	TestMethod();
}

#if M3C_LOG_OUTPUT_EVENT

TEST_F(LogEvent_Test, Event_ErrorLogging_LogError) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, EventWriteEx).Times(0);

	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String.Id), DTGM_ARG6))
	    .InSequence(seq)
	    .WillOnce(t::Return(ERROR_ACCESS_DENIED));

	EXPECT_CALL(m_win32, EventWriteEx(t::_,
	                                  t::AllOf(
	                                      t::Field(&EVENT_DESCRIPTOR::Id, evt::Log_WriteEvent_E.Id),
	                                      t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_EXCEPTION))),
	                                  DTGM_ARG6))
	    .InSequence(seq);
	EXPECT_CALL(m_win32, EventWriteEx(t::_,
	                                  t::AllOf(
	                                      t::Field(&EVENT_DESCRIPTOR::Id, evt::Log_InternalError.Id),
	                                      t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_LOG))),
	                                  DTGM_ARG6))
	    .InSequence(seq);

	Log::Critical(evt::Test_Event_String, "mymessage");
}

TEST_F(LogEvent_Test, Event_ErrorLoggingError_PrintDebug) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, EventWriteEx).Times(0);

	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String.Id), DTGM_ARG6))
	    .InSequence(seq)
	    .WillOnce(t::Return(ERROR_ACCESS_DENIED));

	EXPECT_CALL(m_win32, EventWriteEx(t::_,
	                                  t::AllOf(
	                                      t::Field(&EVENT_DESCRIPTOR::Id, evt::Log_WriteEvent_E.Id),
	                                      t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_EXCEPTION))),
	                                  DTGM_ARG6))
	    .InSequence(seq)
	    .WillOnce(t::Return(ERROR_ALERTED));

#if _DEBUG
	EXPECT_CALL(m_win32, OutputDebugStringA(t::StartsWith(FMT_FORMAT("Error logging event {}:", evt::Test_Event_String.Id))))
	    .InSequence(seq);
#endif

	Log::Critical(evt::Test_Event_String, "mymessage");
}

TEST_F(LogEvent_Test, Event_ExceptionLoggingError_PrintDebug) {
	t::Sequence seq;
	EXPECT_CALL(m_win32, EventWriteEx).Times(0);

	EXPECT_CALL(m_win32, EventWriteEx(t::_, t::Field(&EVENT_DESCRIPTOR::Id, evt::Test_Event_String.Id), DTGM_ARG6))
	    .InSequence(seq)
	    .WillOnce(t::Return(ERROR_ACCESS_DENIED));

	EXPECT_CALL(m_win32, EventWriteEx(t::_,
	                                  t::AllOf(
	                                      t::Field(&EVENT_DESCRIPTOR::Id, evt::Log_WriteEvent_E.Id),
	                                      t::Field(&EVENT_DESCRIPTOR::Keyword, m4t::BitsSet(M3C_KEYWORD_EXCEPTION))),
	                                  DTGM_ARG6))
	    .InSequence(seq)
	    .WillOnce(t::Throw(1));

#if _DEBUG
	EXPECT_CALL(m_win32, OutputDebugStringA(t::StrEq("Error logging\n")))
	    .InSequence(seq);
#endif

	Log::Critical(evt::Test_Event_String, "mymessage");
}

#endif

#if M3C_LOG_OUTPUT_PRINT

TEST_F(LogString_Test, TraceResult_int_Print) {
	const int result = Log::TraceResult(7, "Result: {} - {}", "mymessage");
	EXPECT_EQ(7, result);

	if constexpr (kMinimumLevel >= Priority::kTrace) {
		if constexpr (kOutputPrint) {
			EXPECT_THAT(m_debug, m4t::MatchesRegex("\\[Trace\\] \\[\\d+\\] Result: 7 - mymessage\n\tat.*\n"));
			EXPECT_THAT(m_stderr, m4t::MatchesRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}.\\d{3} \\[Trace\\] \\[\\d+\\] Result: 7 - mymessage\n\tat.*\n"));
		}
	} else if constexpr (kMinimumLevel < Priority::kTrace) {
		EXPECT_EQ("", m_debug);
		EXPECT_EQ("", m_stderr);
	}
}

TEST_F(LogString_Test, TraceHResult_HRESULT_Print) {
	const HRESULT hr = Log::TraceHResult(E_PENDING, "HRESULT: 0x{:X} - {}", "mymessage");
	EXPECT_EQ(E_PENDING, hr);

	if constexpr (kMinimumLevel >= Priority::kTrace) {
		if constexpr (kOutputPrint) {
			EXPECT_THAT(m_debug, m4t::MatchesRegex("\\[Trace\\] \\[\\d+\\] HRESULT: 0x8000000A - mymessage\n\tat.*\n"));
			EXPECT_THAT(m_stderr, m4t::MatchesRegex("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}.\\d{3} \\[Trace\\] \\[\\d+\\] HRESULT: 0x8000000A - mymessage\n\tat.*\n"));
		}
	} else if constexpr (kMinimumLevel < Priority::kTrace) {
		EXPECT_EQ("", m_debug);
		EXPECT_EQ("", m_stderr);
	}
}

#endif

//
// Non-Exception Type
//

TEST_P(LogAll_Test, Exception_intAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::exception>("", std::string{"exception"}, "Unknown exception");
}


//
// std::exception
//

TEST_P(LogAll_Test, Exception_StdExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::std_exception>("", std::exception("myexception"), "myexception");
}

TEST_P(LogStringContexts_Test, Exception_StdExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::std_exception>("", std::exception("myexception"), "myexception");
}

TEST_P(LogAll_Test, Exception_StdExceptionWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::std_exception>("", std::exception("myexception"), "myexception", evt::Default);
}

TEST_P(LogAll_Test, Exception_StdExceptionWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", std::exception("myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// std::system_error
//

TEST_P(LogAll_Test, Exception_StdSystemErrorAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category()); }), "[System] Access is denied. (5)");
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"); }), "[System] myexception: Access is denied. (5)");
}

TEST_P(LogAll_Test, Exception_StdSystemErrorWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category()); }), "[System] Access is denied. (5)", evt::Default);
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithMessageAndLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"); }), "[System] myexception: Access is denied. (5)", evt::Default);
}

TEST_P(LogAll_Test, Exception_StdSystemErrorWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category()); }), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", m4t::WithLocale("en-US", [] { return std::system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"); }), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithInfixMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return std::filesystem::filesystem_error("myexception", R"(C:\mypath.test)", std::error_code(ERROR_ACCESS_DENIED, std::system_category())); }), R"([System] myexception: Access is denied. (5): "C:\mypath.test")");
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithEmptyCustomMessageAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return custom_system_error(ERROR_ACCESS_DENIED, std::system_category(), ""); }), "[System] Error (5)");
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithCustomMessageAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", m4t::WithLocale("en-US", [] { return custom_system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"); }), "[System] myexception (5)");
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithGenericCodeAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::std_system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(std::make_error_code(std::errc::directory_not_empty)); }), "[generic] directory not empty");
}

TEST_P(LogOutputs_Test, Exception_StdSystemErrorWithGenericCodeAndMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::std_system_error>("", m4t::WithLocale("en-US", [] { return std::system_error(std::make_error_code(std::errc::directory_not_empty), "myexception"); }), "[generic] myexception: directory not empty");
}


//
// system_error
//

TEST_P(LogAll_Test, Exception_SystemErrorAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::system_error>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), nullptr), "[System] Access is denied. (5)");
}

TEST_P(LogOutputs_Test, Exception_SystemErrorWithMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::system_error>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"), "[System] myexception: Access is denied. (5)");
}

TEST_P(LogAll_Test, Exception_SystemErrorWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), nullptr), "[System] Access is denied. (5)", evt::Default);
}

TEST_P(LogOutputs_Test, Exception_SystemErrorWithMessageAndLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::system_error>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"), "[System] myexception: Access is denied. (5)", evt::Default);
}

TEST_P(LogAll_Test, Exception_SystemErrorWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), nullptr), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_SystemErrorWithMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", system_error(ERROR_ACCESS_DENIED, std::system_category(), "myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// windows_error
//

TEST_P(LogAll_Test, Exception_WindowsErrorAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::windows_error_E>("", windows_error(ERROR_ACCESS_DENIED), "[Windows] Error: Access is denied. (5)");
}

TEST_P(LogOutputs_Test, Exception_WindowsErrorWithMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::windows_error_E>("", windows_error(ERROR_ACCESS_DENIED, "myexception"), "[Windows] myexception: Access is denied. (5)");
}

TEST_P(LogAll_Test, Exception_WindowsErrorLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::windows_error_E>("", windows_error(ERROR_ACCESS_DENIED), "[Windows] Error: Access is denied. (5)", evt::Default);
}

TEST_P(LogOutputs_Test, Exception_WindowsErrorWithMessageAndLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::windows_error_E>("", windows_error(ERROR_ACCESS_DENIED, "myexception"), "[Windows] myexception: Access is denied. (5)", evt::Default);
}

TEST_P(LogAll_Test, Exception_WindowsErrorWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", windows_error(ERROR_ACCESS_DENIED), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_WindowsErrorWithMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", windows_error(ERROR_ACCESS_DENIED, "myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// rpc_error
//

TEST_P(LogAll_Test, Exception_RpcErrorAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::rpc_error_R>("", rpc_error(RPC_S_ADDRESS_ERROR), "[RPC] Error: An addressing error occurred in the RPC server. (1768)");
}

TEST_P(LogOutputs_Test, Exception_RpcErrorWithMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::rpc_error_R>("", rpc_error(RPC_S_ADDRESS_ERROR, "myexception"), "[RPC] myexception: An addressing error occurred in the RPC server. (1768)");
}

TEST_P(LogAll_Test, Exception_RpcErrorWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::rpc_error_R>("", rpc_error(RPC_S_ADDRESS_ERROR), "[RPC] Error: An addressing error occurred in the RPC server. (1768)", evt::Default);
}

TEST_P(LogOutputs_Test, Exception_RpcErrorWithMessageAndLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::rpc_error_R>("", rpc_error(RPC_S_ADDRESS_ERROR, "myexception"), "[RPC] myexception: An addressing error occurred in the RPC server. (1768)", evt::Default);
}

TEST_P(LogAll_Test, Exception_RpcErrorWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", rpc_error(RPC_S_ADDRESS_ERROR), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_RpcErrorWithMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", rpc_error(RPC_S_ADDRESS_ERROR, "myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// com_error
//

TEST_P(LogAll_Test, Exception_ComErrorAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::com_error_H>("", com_error(E_ABORT), "[COM] Error: Operation aborted (0x80004004)");
}

TEST_P(LogOutputs_Test, Exception_ComErrorWithMessageAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::com_error_H>("", com_error(E_ABORT, "myexception"), "[COM] myexception: Operation aborted (0x80004004)");
}

TEST_P(LogAll_Test, Exception_ComErrorWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::com_error_H>("", com_error(E_ABORT), "[COM] Error: Operation aborted (0x80004004)", evt::Default);
}

TEST_P(LogOutputs_Test, Exception_ComErrorWithMessageAndLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::com_error_H>("", com_error(E_ABORT, "myexception"), "[COM] myexception: Operation aborted (0x80004004)", evt::Default);
}

TEST_P(LogAll_Test, Exception_ComErrorWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", com_error(E_ABORT), "Testing event with int 3", evt::Test_Event_Int, 3);
}

TEST_P(LogOutputs_Test, Exception_ComErrorWithMessageAndDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int>("", com_error(E_ABORT, "myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// std::nested_exception
//

TEST_P(LogAll_Test, Exception_NestedExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	TestMethod<evt::std_exception, true>("", std::exception("Nested myexception"), "Nested myexception");
}

TEST_P(LogAll_Test, Exception_NestedExceptionWithLocationAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::std_exception, true>("", std::exception("Nested myexception"), "Nested myexception", evt::Default);
}

TEST_P(LogAll_Test, Exception_NestedExceptionWithDetailAndPriorityIsEnabled_LogWithLocation) {
	TestMethod<evt::Test_Event_Int, true>("", std::exception("myexception"), "Testing event with int 3", evt::Test_Event_Int, 3);
}


//
// ExceptionToHRESULT
//

TEST_P(LogAll_Test, ExceptionToHRESULT_NonExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::exception, false, true>(
	    ": Unspecified error (0x80004005)",
	    std::string{"exception"}, "Unknown exception");
	EXPECT_EQ(E_FAIL, hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_ExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::std_exception, false, true>(
	    ": Unspecified error (0x80004005)",
	    std::exception("myexception"), "myexception");
	EXPECT_EQ(E_FAIL, hr);
}

TEST_P(LogStringContexts_Test, ExceptionToHRESULT_ExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::std_exception, false, true>(
	    ": Unspecified error (0x80004005)",
	    std::exception("myexception"), "myexception");
	EXPECT_EQ(E_FAIL, hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_BadAllocAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::std_exception, false, true>(
	    ": Not enough memory resources are available to complete this operation. (0x8007000E)",
	    std::bad_alloc(), "bad allocation");
	EXPECT_EQ(E_OUTOFMEMORY, hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_WindowsErrorAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::windows_error_E, false, true>(
	    ": Access is denied. (0x80070005)",
	    windows_error(ERROR_ACCESS_DENIED), "[Windows] Error: Access is denied. (5)");
	EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_RpcErrorAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::rpc_error_R, false, true>(
	    ": An addressing error occurred in the RPC server. (0x800706E8)",
	    rpc_error(RPC_S_ADDRESS_ERROR), "[RPC] Error: An addressing error occurred in the RPC server. (1768)");
	EXPECT_EQ(HRESULT_FROM_WIN32(RPC_S_ADDRESS_ERROR), hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_ComErrorAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::com_error_H, false, true>(
	    ": Operation aborted (0x80004004)",
	    com_error(E_ABORT), "[COM] Error: Operation aborted (0x80004004)");
	EXPECT_EQ(E_ABORT, hr);
}

TEST_P(LogDebug_Test, ExceptionToHRESULT_ComInvalidArgumentsErrorAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::com_error_H, false, true>(
	    ": The parameter is incorrect. (0x80070057)",
	    com_invalid_argument_error(), "[COM] Error: The parameter is incorrect. (0x80070057)");
	EXPECT_EQ(E_INVALIDARG, hr);
}

TEST_P(LogAll_Test, ExceptionToHRESULT_NestedExceptionAndPriorityIsEnabled_LogWithoutLocation) {
	const HRESULT hr = TestMethod<evt::std_exception, true, true>(
	    ": Unspecified error (0x80004005)",
	    std::exception("myexception"), "myexception");
	EXPECT_EQ(E_FAIL, hr);
}


//
// Formatting
//

#if M3C_LOG_OUTPUT_PRINT

TEST_F(LogString_Test, FormatEvent_Reversed_PrintReversed) {
	Log::Critical(evt::Test_FormatReversed, 3, 5, 7, L"Test-\u00FC");

	EXPECT_THAT(m_debug, t::HasSubstr("] Reversed: Test-\u00C3\u00BC; 7; 5; 3\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_Width_PrintPadded) {
	Log::Critical(evt::Test_FormatWidth, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Width:    3;     Test\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_WidthLeft_PrintPaddedLeft) {
	Log::Critical(evt::Test_FormatWidthLeft, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Width left: 3   ; Test    \n\tat"));
}

TEST_F(LogString_Test, FormatEvent_WidthZeroPadded_PrintZeroPadded) {
	Log::Critical(evt::Test_FormatWidthZeroPadded, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Width zero padded: 0003\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_WidthOPrecision_PrintTruncated) {
	Log::Critical(evt::Test_FormatPrecision, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Precision: Te\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_DynamicWidth_PrintPadded) {
	Log::Critical(evt::Test_FormatDynamicWidth, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Dynamic width:   5\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_DynamicWidthZeroPadded_PrintZeroPadded) {
	Log::Critical(evt::Test_FormatDynamicWidthZeroPadded, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Dynamic width zero padded: 005\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_DynamicPrecision_PrintTruncated) {
	Log::Critical(evt::Test_FormatDynamicPrecision, 3, 5, 2, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Dynamic precision: Te\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_DynamicWidthAndPrecision_PrintPaddedAndTruncated) {
	Log::Critical(evt::Test_FormatDynamicWidthAndPrecision, 3, 5, 2, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Dynamic width and precision:    Te\n\tat"));
}

TEST_F(LogString_Test, FormatEvent_DynamicWidthLeftAndPrecision_PrintPaddedLeftAndTruncated) {
	Log::Critical(evt::Test_FormatDynamicWidthLeftAndPrecision, 3, 5, 2, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Dynamic width left and precision: Te   \n\tat"));
}

TEST_F(LogString_Test, FormatEvent_SpecialCharacters_PrintSpecialCharacters) {
	Log::Critical(evt::Test_FormatSpecialCharacters, 3, 5, 7, L"Test");

	EXPECT_THAT(m_debug, t::HasSubstr("] Special characters: {  \t}; 3!\n\tat"));
}

TEST_F(LogString_Test, FormatString_Reversed_PrintReversed) {
	Log::Critical("Reversed: {3}; {2}; {1}; {0}", 3, 5, 7, L"Test-\u00FC");

	EXPECT_THAT(m_debug, t::HasSubstr("] Reversed: Test-\u00C3\u00BC; 7; 5; 3\n\tat"));
}

TEST_F(LogString_Test, Variant_Value_Print) {
	Variant pv;
	InitVariantFromInt32(456, &pv);

	Log::Critical("Variant: {}", pv);

	EXPECT_THAT(m_debug, t::HasSubstr("] Variant: (I4: 456)\n\tat"));
}

TEST_F(LogString_Test, PropVariant_Value_Print) {
	PropVariant pv;
	InitPropVariantFromInt32(456, &pv);

	Log::Critical("PropVariant: {}", pv);

	EXPECT_THAT(m_debug, t::HasSubstr("] PropVariant: (I4: 456)\n\tat"));
}

#endif
}  // namespace
}  // namespace m3c::test
