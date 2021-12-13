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
#include "m3c/exception.h"
#include "m3c/finally.h"
#include "m3c/source_location.h"

#include "m3c.events.h"

#include <windows.h>
#include <evntprov.h>
#include <rpc.h>
#include <winmeta.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>

namespace m3c {

namespace internal {

template class LogContext<const EVENT_DESCRIPTOR&>;
template class LogContext<const char*>;

template class Closure<false, void, LogFormatArgs&>;
template class Closure<false, void, LogEventArgs&>;
template class Closure<true, void, Priority, HRESULT>;

}  // namespace internal

namespace {

/// @brief Get the string for a message id.
/// @tparam kDefaultBufferSize The size of the stack buffer which is used before a dynamically allocated buffer.
/// @param messageId The message id.
/// @return The string for the message id from the executables message table.
template <std::size_t kDefaultBufferSize>
[[nodiscard]] std::string FormatMessageCode(const DWORD messageId) {
	char buffer[kDefaultBufferSize];
	DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, messageId, 0, buffer, sizeof(buffer) / sizeof(*buffer), nullptr);
	if (length) {
		[[likely]];
		return std::string(buffer, length);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		char* pBuffer = nullptr;

		const auto freeBuffer = m3c::finally([&pBuffer]() noexcept {
			if (LocalFree(pBuffer)) {
				[[unlikely]];
				Log::ErrorOnce(m3c::evt::MemoryLeak_E, m3c::last_error());
			}
		});
		length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, messageId, 0, reinterpret_cast<char*>(&pBuffer), 0, nullptr);
		if (length) {
			[[likely]];
			return std::string(pBuffer, length);
		}
		lastError = GetLastError();
	}
	throw windows_error(lastError) + m3c::evt::FormatMessageId_E << messageId;
}


/// @brief Get the message id for a log level.
/// @param priority The priority.
/// @return The message id for the log level.
[[nodiscard]] DWORD GetLevelMessageId(const Priority priority) {
	switch (priority) {
	case Priority::kNone:
		return MSG_level_LogAlways;
	case Priority::kCritical:
		return MSG_level_Critical;
	case Priority::kError:
		return MSG_level_Error;
	case Priority::kWarning:
		return MSG_level_Warning;
	case Priority::kInfo:
		return MSG_level_Informational;
	case Priority::kVerbose:
		return MSG_level_Verbose;
	case Priority::kDebug:
		return MSG_m3c_level_Debug;
	case Priority::kTrace:
		return MSG_m3c_level_Trace;
	default:
		try {
			return internal::kLevelNames.at(static_cast<std::underlying_type_t<Priority>>(priority));
		} catch (const std::out_of_range&) {
			std::throw_with_nested(std::out_of_range("level") + evt::Default);
		}
	}
}

/// @brief Get the message id for an event.
/// @param eventId The event id.
/// @return The message id for the event id.
[[nodiscard]] DWORD GetEventMessageId(const USHORT eventId) {
	try {
		return internal::kEventMessages.at(eventId);
	} catch (const std::out_of_range&) {
		std::throw_with_nested(std::out_of_range("message") + evt::Default);
	}
}

}  // namespace

namespace internal {

[[nodiscard]] std::string GetEventMessagePattern(const USHORT eventId) {
	const DWORD messageId = GetEventMessageId(eventId);
	std::string message = FormatMessageCode<256>(messageId);

	std::regex regex("(?:%([1-9][0-9]?|[^1-9])(?:!([0+ #-]*)([0-9]+|\\*)?(\\.(?:[0-9]+|\\*))?[^!]*([^!])!)?)|[{}\r\n]", std::regex_constants::ECMAScript);
	std::sregex_iterator it(message.cbegin(), message.cend(), regex);
	const std::sregex_iterator end;
	if (it == end) {
		return message;
	}

	std::string result;
	while (true) {
		std::smatch match = *it;
		result += match.prefix();

		const char group = message[match.position(0)];
		if (group == '{' || group == '}') {
			[[unlikely]];
			result.append(2, group);
		} else if (group == '\n') {
			// replace newline with space to keep log output on a single line
			result += ' ';
		} else if (group == '\r') {
			// remove \r
		} else {
			const char pattern = message[match.position(1)];
			if (pattern >= '1' && pattern <= '9') {
				const std::string type = match.str(5);
				std::uint_fast8_t index = static_cast<std::uint_fast8_t>(std::atoi(match.str(1).c_str())) - 1;  // NOLINT(cert-err34-c): argument to atoi is digits only
				if (type.empty()) {
					result += '{' + std::to_string(index) + '}';
				} else {
					const std::string flags = match.str(2);
					const std::string width = match.str(3);
					const std::string precision = match.str(4);
					std::string subformat = ":";
					if (!flags.empty()) {
						if (flags.find('-') != std::string::npos) {
							subformat += '<';
						} else {
							subformat += '>';
						}
						if (flags.find('+') != std::string::npos) {
							subformat += '+';
						} else if (flags.find(' ') != std::string::npos) {
							subformat += ' ';
						} else {
							// use default
						}
						if (flags.find('#') != std::string::npos) {
							subformat += '#';
						}
						if (flags.find('0') != std::string::npos) {
							subformat += '0';
						}
					}
					if (!width.empty()) {
						if (flags.empty()) {
							subformat += '>';
						}
						if (width == "*") {
							subformat += '{' + std::to_string(index++) + '}';
						} else {
							subformat += width;
						}
					}
					if (!precision.empty()) {
						if (precision == ".*") {
							subformat += ".{" + std::to_string(index++) + '}';
						} else {
							subformat += precision;
						}
					}

					switch (type[0]) {
					case 'C':
					case 'S':
						subformat += static_cast<char>(std::tolower(type[0]));
						break;
					case 'i':
					case 'u':
						subformat += 'd';
						break;
					default:
						subformat += type[0];
						break;
					}
					result += '{' + std::to_string(index) + subformat + '}';
				}
			} else {
				constexpr char kSpecial[] = "0bnrt";
				constexpr char kReplace[] = "\0  \r\t";  // replace \n with space!
				static_assert(sizeof(kSpecial) == sizeof(kReplace));
				const char* pos = std::strchr(kSpecial, pattern);
				if (pos) {
					if (*pos != 'r') {  // skip \r
						result += kReplace[pos - kSpecial];
					}
				} else {
					result += pattern;
				}
			}
		}

		++it;
		if (it == end) {
			[[unlikely]];
			result += match.suffix();
			break;
		}
	}

	return result;
}

}  // namespace internal


//
// Logger
//

[[nodiscard]] std::string Log::GetLevelName(const Priority priority) {
	const DWORD messageId = GetLevelMessageId(priority);
	return FormatMessageCode<16>(messageId);
}

void Log::PrintDefault(const Priority priority, const std::string& message) {
	const std::string str = FMT_FORMAT("[{}] [{}] {}", GetLevelName(priority), GetCurrentThreadId(), message);

	OutputDebugStringA(str.c_str());

	SYSTEMTIME st;
	GetSystemTime(&st);
	const std::string timeAndMessage = FMT_FORMAT("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} {}",
	                                              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
	                                              str);
	std::ignore = fputs(timeAndMessage.c_str(), stderr);
	// no need to check return code; failure to write to stderr will prevent logging
}

Log& Log::GetInstance() noexcept {
	static Log instance;
	return instance;
}

void Log::RegisterEvents() noexcept {
	const ULONG result = EventRegister(&kGuid, nullptr, nullptr, &m_handle);
	if (result != ERROR_SUCCESS) {
		Log::Error(evt::Log_Register_E, kGuid, win32_error(result));
	}
}

void Log::UnregisterEvents() noexcept {
	if (m_handle) {
		const ULONG result = EventUnregister(m_handle);
		if (result != ERROR_SUCCESS) {
			Log::Error(evt::Log_Unregister_E, kGuid, win32_error(result));
		}
		m_handle = 0;
	}
}

template <internal::LogMessage M>
void Log::LogInternalError(const internal::LogContext<M>& loggedContext) noexcept {
	const std::source_location& sourceLocation = loggedContext.GetSourceLocation();
	const auto eventData = [&loggedContext]() noexcept {
		if constexpr (internal::LogContext<M>::kIsEventDescriptor) {
			return loggedContext.GetLogMessage().Id;
		} else {
			return loggedContext.GetLogMessage();
		}
	}();
	try {
#ifdef _DEBUG
		try {
#endif
			DoLogException(Priority::kError, internal::LogContext<const EVENT_DESCRIPTOR&>(evt::Log_InternalError, sourceLocation),
			               GetPrintCapture<true>(eventData),
			               GetEventCapture<true>(eventData));
#ifdef _DEBUG
		} catch (const std::exception& e) {
			if constexpr (internal::LogContext<M>::kIsEventDescriptor) {
				OutputDebugStringA(FMT_FORMAT("Error logging event {}: {}\n\tat {}({})", eventData, e.what(), sourceLocation.file_name(), sourceLocation.line()).c_str());
			} else {
				OutputDebugStringA(FMT_FORMAT("Error logging event {}: {}\n\tat {}({}) ({})", eventData, e.what(), sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name()).c_str());
			}
		}
#endif
	}  // namespace m3c
	catch (...) {
#ifdef _DEBUG
		OutputDebugStringA("Error logging\n");
#endif
	}
}

template void Log::LogInternalError(const internal::LogContext<const EVENT_DESCRIPTOR&>&) noexcept;
template void Log::LogInternalError(const internal::LogContext<const char*>&) noexcept;

_Ret_range_(<, 0) HRESULT Log::ExceptionToHResult(const Priority priority, const internal::ClosureNoexcept<void, Priority, HRESULT>&& log) noexcept {
	try {
		throw;
	} catch (const com_invalid_argument_error& e) {
		// calling with invalid argument may happen outside the scope of our code, so only log as debug
		const HRESULT hr = e.code().value();
		log(Priority::kDebug, hr);
		return hr;
	} catch (const com_error& e) {
		const HRESULT hr = e.code().value();
		log(priority, hr);
		return hr;
	} catch (const windows_error& e) {
		const HRESULT hr = HRESULT_FROM_WIN32(e.code().value());
		log(priority, hr);
		return hr;
	} catch (const rpc_error& e) {
		const HRESULT hr = HRESULT_FROM_WIN32(e.code().value());
		log(priority, hr);
		return hr;
	} catch (const std::bad_alloc&) {
		log(priority, E_OUTOFMEMORY);
		return E_OUTOFMEMORY;
	} catch (...) {
		// everything else, including std::exception is just E_FAIL
		log(priority, E_FAIL);
		return E_FAIL;
	}
}

void Log::Print(const Priority priority, _In_z_ const char* const pattern, const LogFormatArgs& formatArgs, _In_z_ const char* cause, const std::source_location& sourceLocation) {
	const std::string message = FMT_FORMAT("{}\n\tat {}({}) ({})\n{}",
	                                       fmt::vformat(pattern, *formatArgs),
	                                       sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name(),
	                                       cause);
	log::Print(priority, message);
}

void Log::WriteEvent(const EVENT_DESCRIPTOR& event, LogEventArgs& eventArgs, _In_opt_ const GUID* const pRelatedActivityId) const {
	const ULONG count = eventArgs.size();
	const ULONG result = EventWriteEx(m_handle, &event, 0, 0, nullptr, pRelatedActivityId, count, count ? eventArgs.data() : nullptr);
	if (result == ERROR_SUCCESS) {
		[[likely]];
		return;
	}
#ifndef _DEBUG
	if (result == ERROR_MORE_DATA || result == ERROR_NOT_ENOUGH_MEMORY || result == 0xC0000188L /* STATUS_LOG_FILE_FULL */) {
		return;
	}
#endif
	throw windows_error(result) + evt::Log_WriteEvent_E << event.Id;
}

template <bool kPrint, bool kEvent>
void Log::WriteException(const Priority priority, const GUID& activityId, _Inout_ std::string& cause) {
	static_assert(kPrint || kEvent, "MUST NOT call function when no output is required");
	try {
		throw;
	} catch (const std::nested_exception& nested) {
		try {
			std::rethrow_exception(nested.nested_ptr());
		} catch (...) {
			WriteException<kPrint, kEvent>(priority, activityId, cause);
		}
		DoWriteException<kPrint, kEvent>(priority, activityId, cause);
	} catch (...) {
		DoWriteException<kPrint, kEvent>(priority, activityId, cause);
	}
}

template void Log::WriteException<false, true>(Priority, const GUID&, std::string&);
template void Log::WriteException<true, false>(Priority, const GUID&, std::string&);
template void Log::WriteException<true, true>(Priority, const GUID&, std::string&);

template <bool kPrint, bool kEvent>
void Log::DoWriteException(const Priority priority, const GUID& activityId, _Inout_ std::string& cause) {
	static_assert(kPrint || kEvent, "MUST NOT call function when no output is required");

	EVENT_DESCRIPTOR event;
	EventDescSetId(&event, 0);

	std::string message;

	LogFormatArgs formatArgs;
	LogEventArgs eventArgs;
	const std::source_location* pSourceLocation = nullptr;

	// Container to persist value until end of function.
	union {  // NOLINT(cppcoreguidelines-pro-type-member-init): Only used locally in blocks, but MUST exist until end of funtion.
		DWORD code;
		win32_error win32;
		hresult hr;
		rpc_status rpc;
	} code;
	try {
		try {
			throw;
		} catch (const internal::BaseException<const EVENT_DESCRIPTOR&>& e) {
			[[likely]];
			event = e.GetEvent();

			const LogData& logData = e.GetLogData();
			pSourceLocation = &e.GetSourceLocation();
			if constexpr (kPrint) {
				logData.CopyArgumentsTo(formatArgs);
			}
			if constexpr (kEvent) {
				logData.CopyArgumentsTo(eventArgs);
			}
			throw;
		} catch (const internal::BaseException<const char*>& e) {
			[[likely]];
			const char* pattern = e.GetLogMessage();

			const LogData& logData = e.GetLogData();
			pSourceLocation = &e.GetSourceLocation();

			if (pattern) {
				// Format with a "private" set of arguments (except for Default message)
				LogFormatArgs args;
				logData.CopyArgumentsTo(args);
				message = fmt::vformat(pattern, *args);
			}

			throw;
		}
	} catch (const windows_error& e) {
		code.win32 = win32_error(e.code().value());
		if (!event.Id) {
			// logged with evt::Default or string message
			event = evt::windows_error_E;
			const char* msg;  // NOLINT(cppcoreguidelines-init-variables): Initialization would require checking condition twice.
			if (message.empty()) {
				msg = e.message();
				if (*msg == '\0') {
					[[unlikely]];
					msg = "Error";
				}
			} else {
				msg = message.c_str();
			}
			if constexpr (kPrint) {
				formatArgs << msg;
			}
			if constexpr (kEvent) {
				eventArgs << msg;
			}
		}
		if constexpr (kPrint) {
			formatArgs << code.win32;
		}
		if constexpr (kEvent) {
			eventArgs << code.win32;
		}
	} catch (const com_error& e) {
		code.hr = hresult(e.code().value());
		if (!event.Id) {
			// logged with evt::Default or string message
			event = evt::com_error_H;
			const char* msg;  // NOLINT(cppcoreguidelines-init-variables): Initialization would require checking condition twice.
			if (message.empty()) {
				msg = e.message();
				if (*msg == '\0') {
					[[unlikely]];
					msg = "Error";
				}
			} else {
				msg = message.c_str();
			}
			if constexpr (kPrint) {
				formatArgs << msg;
			}
			if constexpr (kEvent) {
				eventArgs << msg;
			}
		}
		if constexpr (kPrint) {
			formatArgs << code.hr;
		}
		if constexpr (kEvent) {
			eventArgs << code.hr;
		}
	} catch (const rpc_error& e) {
		code.rpc = rpc_status(e.code().value());
		if (!event.Id) {
			// logged with evt::Default or string message
			event = evt::rpc_error_R;
			const char* msg;  // NOLINT(cppcoreguidelines-init-variables): Initialization would require checking condition twice.
			if (message.empty()) {
				msg = e.message();
				if (*msg == '\0') {
					[[unlikely]];
					msg = "Error";
				}
			} else {
				msg = message.c_str();
			}
			if constexpr (kPrint) {
				formatArgs << msg;
			}
			if constexpr (kEvent) {
				eventArgs << msg;
			}
		}
		if constexpr (kPrint) {
			formatArgs << code.rpc;
		}
		if constexpr (kEvent) {
			eventArgs << code.rpc;
		}
	} catch (const system_error& e) {
		code.code = static_cast<DWORD>(e.code().value());
		if (!event.Id) {
			// logged with evt::Default or string message
			event = evt::system_error;
			const char* msg;  // NOLINT(cppcoreguidelines-init-variables): Initialization would require checking condition twice.
			if (message.empty()) {
				msg = e.what();
				if (*msg == '\0') {
					[[unlikely]];
					msg = "Error";
				}
			} else {
				msg = message.c_str();
			}
			if constexpr (kPrint) {
				if (message.empty()) {
					// Add code to error message for printing outputs
					message = FMT_FORMAT("{} ({})", msg, code.code);
				} else {
					// Add message with code to error message for printing string messages
					message = FMT_FORMAT("{}: {}", message, win32_error(code.code));
				}
				formatArgs << message;
			}
			if constexpr (kEvent) {
				eventArgs << msg;
			}
		}
		if constexpr (kPrint) {
			formatArgs << code.code;
		}
		if constexpr (kEvent) {
			eventArgs << code.code;
		}
	} catch (const std::system_error& e) {
		const std::error_code& errorCode = e.code();
		const std::error_category& category = errorCode.category();
		code.code = static_cast<DWORD>(errorCode.value());

		if (category == std::system_category()) {
			if (!event.Id) {
				// logged with evt::Default or string message
				event = evt::system_error;

				const char* msg;  // NOLINT(cppcoreguidelines-init-variables): Initialization would require checking condition twice.
				if (message.empty()) {
					msg = e.what();
					if (*msg == '\0') {
						[[unlikely]];
						msg = "Error";
					}
				} else {
					msg = message.c_str();
				}
				if constexpr (kPrint) {
					const std::string errorMessage = errorCode.message();
					const std::string formattedCode = FMT_FORMAT(" ({})", code.code);
					if (message.empty()) {
						message = msg;
						// Add code to error message for printing outputs
						if (const auto pos = message.find(errorMessage); pos != std::string::npos) {
							message.insert(pos + errorMessage.size(), formattedCode);
						} else {
							message.append(formattedCode);
						}
					} else {
						message.append(": ").append(errorMessage).append(formattedCode);
					}
					formatArgs << message;
				}
				if constexpr (kEvent) {
					eventArgs << msg;
				}
			}
			if constexpr (kPrint) {
				formatArgs << code.code;
			}
			if constexpr (kEvent) {
				eventArgs << code.code;
			}
		} else {
			// other categories than std::system_category never have context data
			assert(!event.Id);
			event = evt::std_system_error;

			const char* const categoryName = category.name();
			const char* const msg = message.empty() ? e.what() : message.c_str();
			if constexpr (kPrint) {
				formatArgs << categoryName << msg << code.code;
			}
			if constexpr (kEvent) {
				eventArgs << categoryName << msg << code.code;
			}
		}
	} catch (const std::exception& e) {
		if (!event.Id) {
			// logged with evt::Default or string message
			event = evt::std_exception;
			const char* const msg = message.empty() ? e.what() : message.c_str();
			if constexpr (kPrint) {
				formatArgs << msg;
			}
			if constexpr (kEvent) {
				eventArgs << msg;
			}
		}
	} catch (...) {
		assert(!event.Id);
		assert(message.empty());
		event = evt::exception;
	}

#ifdef _DEBUG
	if constexpr (kPrint && kEvent) {
		assert(formatArgs.size() == eventArgs.size());
	}
#endif
	if constexpr (kPrint) {
		std::string pattern = "\tcaused by: " + internal::GetEventMessagePattern(event.Id) + "\n";
		if (pSourceLocation) {
			const std::size_t count = formatArgs.size();
			pattern += FMT_FORMAT("\t\tat {{{}}}({{{}}}) ({{{}}})\n", count, count + 1, count + 2);
			formatArgs << pSourceLocation->file_name() << pSourceLocation->line() << pSourceLocation->function_name();
		}
		if (!cause.empty()) {
			pattern += FMT_FORMAT("{{{}}}", formatArgs.size());
			formatArgs << cause;
		}
		cause = fmt::vformat(pattern, *formatArgs);
	}

	if constexpr (kEvent) {
		std::uint_least32_t line;  // NOLINT(cppcoreguidelines-init-variables): MUST be outside if block to keep value alive for WriteEvent.
		if (pSourceLocation) {
			const char* const fileName = pSourceLocation->file_name();
			line = pSourceLocation->line();
			eventArgs << fileName << line;
		}

		EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
		EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
		WriteEvent(event, eventArgs, &activityId);
	}
}

template void Log::DoWriteException<false, true>(Priority, const GUID&, std::string&);
template void Log::DoWriteException<true, false>(Priority, const GUID&, std::string&);
template void Log::DoWriteException<true, true>(Priority, const GUID&, std::string&);

USHORT* Log::LogOnce(const USHORT eventId) noexcept {
	std::uint_fast8_t index = 0;
	static_assert(sizeof(s_logging) / sizeof(s_logging[0]) > 0);
	while (s_logging[index]) {
		[[unlikely]];
		if (s_logging[index] == eventId) {
			return nullptr;
		}
		++index;
		if (index >= sizeof(s_logging) / sizeof(s_logging[0])) {
			[[unlikely]];
			return nullptr;
		}
	}
	s_logging[index] = eventId;
	return &s_logging[index];
}

bool Log::SetActivityId(_Out_ GUID& activityId) noexcept {
	const ULONG result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &activityId);
	if (result != ERROR_SUCCESS) {
		[[unlikely]];
		// silently ignore errors
		Error(evt::Log_ActivityId_E, EVENT_ACTIVITY_CTRL_CREATE_SET_ID, win32_error(result));
		return false;
	}
	return true;
}

void Log::ResetActivityId(GUID activityId) noexcept {
	const ULONG result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_SET_ID, &activityId);
	if (result != ERROR_SUCCESS) {
		[[unlikely]];
		// silently ignore errors
		Error(evt::Log_ActivityId_E, EVENT_ACTIVITY_CTRL_SET_ID, win32_error(result));
	}
}

}  // namespace m3c
