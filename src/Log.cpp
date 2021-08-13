#include "m3c/Log.h"

#include "m3c_events.h"

#include "m3c/LogData.h"
#include "m3c/exception.h"
#include "m3c/format.h"
#include "m3c/source_location.h"

#include <fmt/core.h>

#include <windows.h>
#include <evntprov.h>
#include <winmeta.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <map>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace m3c {

namespace {

template <std::size_t kDefaultBufferSize>
std::string FormatMessageCode(const DWORD messageId) {
	char buffer[kDefaultBufferSize];
	DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, messageId, 0, buffer, sizeof(buffer) / sizeof(*buffer), nullptr);
	if (length) [[likely]] {
		return std::string(buffer, length);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		char* pBuffer = nullptr;

		auto freeBuffer = m3c::finally([&pBuffer]() noexcept {
			if (LocalFree(pBuffer)) [[unlikely]] {
				Log::ErrorOnce(m3c::evt::LocalFree, m3c::make_win32_error());
			}
		});
		length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, messageId, 0, reinterpret_cast<char*>(&pBuffer), 0, nullptr);
		if (length) [[likely]] {
			return std::string(pBuffer, length);
		}
		lastError = GetLastError();
	}
	throw windows_error(lastError) + m3c::evt::FormatMessageX << messageId;
}

std::string GetLevelName(const Priority priority) {
	DWORD messageId;
	switch (priority) {
	case Priority::kNone:
		messageId = MSG_level_LogAlways;
		break;
	case Priority::kCritical:
		messageId = MSG_level_Critical;
		break;
	case Priority::kError:
		messageId = MSG_level_Error;
		break;
	case Priority::kWarning:
		messageId = MSG_level_Warning;
		break;
	case Priority::kInfo:
		messageId = MSG_level_Informational;
		break;
	case Priority::kVerbose:
		messageId = MSG_level_Verbose;
		break;
	case Priority::kDebug:
		messageId = MSG_level_Debug;
		break;
	case Priority::kTrace:
		messageId = MSG_level_Trace;
		break;
	default:
		try {
			messageId = internal::kLevelNames.at(static_cast<std::underlying_type_t<Priority>>(priority));
		} catch (const std::out_of_range&) {
			std::throw_with_nested(std::out_of_range("level") + evt::Default);
		}
	}

	return FormatMessageCode<16>(messageId);
}

}  // namespace

namespace internal {

std::string GetEventMessagePattern(const USHORT eventId) {
	DWORD messageId;
	try {
		messageId = kEventMessages.at(eventId);
	} catch (const std::out_of_range&) {
		std::throw_with_nested(std::out_of_range("message") + evt::Default);
	}
	const std::string message = FormatMessageCode<256>(messageId);

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
		if (group == '{' || group == '}') [[unlikely]] {
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
				std::uint_fast8_t index = static_cast<std::uint_fast8_t>(atoi(match.str(1).c_str())) - 1;
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
		if (it == end) [[unlikely]] {
			result += match.suffix();
			break;
		}
	}

	return result;
}

}  // namespace internal

namespace internal {

#if 0
template <EventInfo E>
EventContext<E>::EventContext(E event, source_location sourceLocation) noexcept
	: m_event(event)
	, m_sourceLocation(std::move(sourceLocation)) {
}

template <EventInfo E>
template <typename X, typename std::enable_if_t<std::is_same_v<X, const char*>, int>>
EventContext<E>::EventContext(const std::string& event, source_location sourceLocation) noexcept
	: m_event(event.c_str())
	, m_sourceLocation(std::move(sourceLocation)) {
}


template <EventInfo E>
const E EventContext<E>::GetEvent() const noexcept {
	return m_event;
}

template <EventInfo E>
const source_location& EventContext<E>::GetSourceLocation() const noexcept {
	return m_sourceLocation;
}

template <EventInfo E>
ExceptionEventContext<E>::ExceptionEventContext(EventContext<E>&& context) noexcept
	: EventContext<E>(std::move(context)) {
}

template <EventInfo E>
const std::exception_ptr& ExceptionEventContext<E>::GetException() const noexcept {
	return m_exception;
}

#endif
template class EventContext<const EVENT_DESCRIPTOR&>;
template class EventContext<const char*>;
template EventContext<const char*>::EventContext(const std::string&, source_location) noexcept;

}  // namespace internal

//
// Logger
//

Log Log::m_log;

Log::Log(Log&& log) noexcept
	: m_handle(log.m_handle) {
	log.m_handle = 0;
}

Log& Log::operator=(Log&& log) noexcept {
	if (m_handle) {
		UnregisterEvents();
	}
	m_handle = log.m_handle;
	log.m_handle = 0;
	return *this;
}

void Log::RegisterEvents(const GUID& guid) noexcept {
	const ULONG result = EventRegister(&guid, nullptr, nullptr, &m_handle);
	if (result != ERROR_SUCCESS) {
		Error(evt::EventRegister, guid);
	}
}

void Log::UnregisterEvents() noexcept {
	if (m_handle) {
		const ULONG result = EventUnregister(m_handle);
		if (result != ERROR_SUCCESS) {
			Error(evt::EventUnregister);
		}
	}
}


template <internal::EventInfo E>
void Log::LogInternalError(E loggedEvent) noexcept {
	try {
#ifdef _DEBUG
		try {
#endif
			if constexpr (std::is_same_v<E, const EVENT_DESCRIPTOR&>) {
				auto print = GetDebugCapture(loggedEvent.Id);
				auto event = GetEventCapture<true>(loggedEvent.Id);

				DoLogExceptionEvent(Priority::kError, internal::EventContext<E>{evt::Log_LogInternalError},
				                    {[](void* fn, internal::LogData& logData) {
										 (*static_cast<decltype(print)*>(fn))(logData);
									 },
				                     &print},
				                    {[](void* fn, EVENT_DATA_DESCRIPTOR* data) noexcept {
										 (*static_cast<decltype(event)*>(fn))(data);
									 },
				                     &event},
				                    1);
			} else {
				auto print = GetDebugCapture(loggedEvent);
				auto event = GetEventCapture<true>(loggedEvent);

				DoLogExceptionEvent(Priority::kError, internal::EventContext<E>{internal::GetEventMessagePattern(evt::Log_LogInternalError.Id)},
				                    {[](void* fn, internal::LogData& logData) {
										 (*static_cast<decltype(print)*>(fn))(logData);
									 },
				                     &print},
				                    {[](void* fn, EVENT_DATA_DESCRIPTOR* data) noexcept {
										 (*static_cast<decltype(event)*>(fn))(data);
									 },
				                     &event},
				                    1);
			}
#ifdef _DEBUG
		} catch (const std::exception& e) {
			if constexpr (std::is_same_v<E, const EVENT_DESCRIPTOR&>) {
				OutputDebugStringA(fmt::format("Error logging event {}: {}\n", loggedEvent.Id, e.what()).c_str());
			} else {
				OutputDebugStringA(fmt::format("Error logging event {}: {}\n", loggedEvent, e.what()).c_str());
			}
		}
#endif
	} catch (...) {
#ifdef _DEBUG
		OutputDebugStringA("Error logging\n");
#endif
	}
}

template void Log::LogInternalError(const EVENT_DESCRIPTOR&) noexcept;
template void Log::LogInternalError(const char*) noexcept;

std::uint_fast8_t Log::LogOnce(const USHORT eventId) noexcept {
	std::uint_fast8_t index = 0;
	static_assert(sizeof(m_logging) / sizeof(m_logging[0]) > 0);
	while (m_logging[index])
		[[unlikely]] {
			if (m_logging[index] == eventId) {
				return kNoLogging;
			}
			++index;
			if (index >= sizeof(m_logging) / sizeof(m_logging[0])) [[unlikely]] {
				return kNoLogging;
			}
		}
	return index;
}

void Log::WriteEvent(const EVENT_DESCRIPTOR& event, _In_range_(0, MAX_EVENT_DATA_DESCRIPTORS) const ULONG count, _In_reads_(count) EVENT_DATA_DESCRIPTOR* const data, _In_opt_ const GUID* const pRelatedActivityId) {
	const ULONG result = EventWriteEx(m_handle, &event, 0, 0, nullptr, pRelatedActivityId, count, count ? data : nullptr);
	if (result == ERROR_SUCCESS) [[likely]] {
		return;
	}
#ifndef _DEBUG
	if (result == ERROR_MORE_DATA || result == ERROR_NOT_ENOUGH_MEMORY || result == 0xC0000188L /* STATUS_LOG_FILE_FULL */) {
		return;
	}
#endif
	throw windows_error(result) + evt::EventWrite << event.Id;
}

template <bool kDebug, bool kStderr>
void Log::Print(Priority priority, const char* const pattern, const internal::LogData& logData, _In_z_ const char* cause, const source_location& sourceLocation) noexcept(!kDebug && !kStderr) {
	if constexpr (kDebug || kStderr) {
		fmt::dynamic_format_arg_store<fmt::format_context> formatArgs;
		logData.CopyArgumentsTo(formatArgs);
		const std::string message = fmt::format("[{}] [{}] {}\n\tat {}({}) ({})\n{}",
		                                        GetLevelName(priority), GetCurrentThreadId(),
		                                        fmt::vformat(pattern, formatArgs),
		                                        sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name(),
		                                        cause);
		if constexpr (kDebug) {
			OutputDebugStringA(message.c_str());
		}
		if constexpr (kStderr) {
			SYSTEMTIME st;
			GetSystemTime(&st);
			const std::string timeAndMessage = fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} {}",
			                                               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			                                               message);
			fputs(timeAndMessage.c_str(), stderr);
			// no need to check return code; failure to write to stderr will prevent logging
		}
	}
}

template void Log::Print<false, false>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(true);
template void Log::Print<false, true>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);
template void Log::Print<true, false>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);
template void Log::Print<true, true>(Priority, const char*, const internal::LogData&, const char*, const source_location&) noexcept(false);

template <bool kPrint, bool kEvent>
void Log::DoLogException(const Priority priority, const GUID& activityId, _Inout_ std::string& cause) {
	try {
		throw;
	} catch (const std::nested_exception& nested) {
		try {
			std::rethrow_exception(nested.nested_ptr());
		} catch (...) {
			DoLogException<kPrint, kEvent>(priority, activityId, cause);
		}
		WriteExceptionEvent<kPrint, kEvent>(priority, activityId, cause);
	} catch (...) {
		WriteExceptionEvent<kPrint, kEvent>(priority, activityId, cause);
	}
}

template void Log::DoLogException<false, false>(Priority, const GUID&, std::string&);
template void Log::DoLogException<false, true>(Priority, const GUID&, std::string&);
template void Log::DoLogException<true, false>(Priority, const GUID&, std::string&);
template void Log::DoLogException<true, true>(Priority, const GUID&, std::string&);

template <bool kPrint, bool kEvent>
void Log::WriteExceptionEvent(const Priority priority, const GUID& relatedActivityId, _Inout_ std::string& cause) {
	EVENT_DESCRIPTOR event;
	EventDescSetId(&event, 0);

	std::uint32_t count = 0;
	fmt::dynamic_format_arg_store<fmt::format_context> args;
	source_location sourceLocation;

	std::vector<EVENT_DATA_DESCRIPTOR> data;
	DWORD code;
	std::string systemErrorMessage;
	try {
		try {
			throw;
		} catch (const internal::BaseException& e) {
			[[likely]];
			event = e.GetEvent();

			const internal::LogData& logData = e.GetLogData();
			if constexpr (kPrint) {
				count += logData.CopyArgumentsTo(args);
				sourceLocation = e.GetSourceLocation();
			}
			if constexpr (kEvent) {
				logData.CopyArgumentsTo(data);
			}
			throw;
		}
	} catch (const windows_error& e) {
		if (!event.Id) {
			event = evt::windows_error;
			const char* message = e.message();
			if (*message == '\0') {
				message = "Windows error";
			}
			if constexpr (kPrint) {
				args.push_back(message);
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), message);
			}
		}
		code = static_cast<DWORD>(e.code().value());
		if constexpr (kPrint) {
			args.push_back(make_win32_error(code));
			++count;
		}
		if constexpr (kEvent) {
			Set(data.emplace_back(), code);
		}
	} catch (const com_error& e) {
		if (!event.Id) {
			event = evt::com_error;
			const char* message = e.message();
			if (*message == '\0') {
				message = "COM error";
			}
			if constexpr (kPrint) {
				args.push_back(message);
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), message);
			}
		}
		code = static_cast<HRESULT>(e.code().value());
		if constexpr (kPrint) {
			args.push_back(make_hresult(code));
			++count;
		}
		if constexpr (kEvent) {
			Set(data.emplace_back(), code);
		}
	} catch (const rpc_error& e) {
		if (!event.Id) {
			event = evt::rpc_error;
			const char* message = e.message();
			if (*message == '\0') {
				message = "RPC error";
			}
			if constexpr (kPrint) {
				args.push_back(message);
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), message);
			}
		}
		code = static_cast<RPC_STATUS>(e.code().value());
		if constexpr (kPrint) {
			args.push_back(make_rpc_status(code));
			++count;
		}
		if constexpr (kEvent) {
			Set(data.emplace_back(), code);
		}
	} catch (const system_error& e) {
		if (!event.Id) {
			event = evt::system_error;
			const char* message = e.message();
			if (*message == '\0') {
				message = "System error";
			}
			if constexpr (kPrint) {
				args.push_back(message);
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), message);
			}
		}
		code = e.code().value();
		if constexpr (kPrint) {
			args.push_back(make_win32_error(code));
			++count;
		}
		if constexpr (kEvent) {
			Set(data.emplace_back(), code);
		}
	} catch (const std::system_error& e) {
		const std::error_code& errorCode = e.code();
		const std::error_category& category = errorCode.category();
		if (category == std::system_category()) {
			if (!event.Id) {
				event = evt::system_error;
				const char* what = e.what();

				// remove error message from text to prevent printing it twice
				const std::string errorMessage = errorCode.message();
				const auto pos = std::string_view(what).find(errorMessage);
				if (pos != std::string::npos) [[likely]] {
					if (pos < 3) [[unlikely]] {
						// no custom message
						systemErrorMessage.assign("System error");
					} else {
						systemErrorMessage.assign(what, pos < 3 ? 0 : pos - 2);
					}
					// append remaining text after error message
					systemErrorMessage.append(what + pos + errorMessage.size());
				} else {
					if (*what == '\0') [[unlikely]] {
						what = "System error";
					}
				}


				if constexpr (kPrint) {
					if (pos != std::string::npos) [[likely]] {
						args.push_back(systemErrorMessage);
					} else {
						args.push_back(what);
					}
					++count;
				}
				if constexpr (kEvent) {
					if (pos != std::string::npos) [[likely]] {
						Set(data.emplace_back(), systemErrorMessage);
					} else {
						Set(data.emplace_back(), what);
					}
				}
			}
			code = errorCode.value();
			if constexpr (kPrint) {
				args.push_back(make_win32_error(code));
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), code);
			}
		} else {
			assert(!event.Id);
			event = evt::std_system_error;

			code = errorCode.value();
			if constexpr (kPrint) {
				args.push_back(e.what());
				args.push_back(category.name());
				args.push_back(code);
				count += 3;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), e.what());
				Set(data.emplace_back(), category.name());
				Set(data.emplace_back(), code);
			}
		}
	} catch (const std::exception& e) {
		if (!event.Id) {
			event = evt::std_exception;
			if constexpr (kPrint) {
				args.push_back(e.what());
				++count;
			}
			if constexpr (kEvent) {
				Set(data.emplace_back(), e.what());
			}
		}
	} catch (...) {
		assert(!event.Id);
		event = evt::exception;
	}

	if constexpr (kPrint) {
		std::string pattern = "\tcaused by: " + internal::GetEventMessagePattern(event.Id) + "\n";
		if (sourceLocation.file_name()[0]) {
			pattern += fmt::format("\t\tat {{{}}}({{{}}}) ({{{}}})\n", count, count + 1, count + 2);
			args.push_back(sourceLocation.file_name());
			args.push_back(sourceLocation.line());
			args.push_back(sourceLocation.function_name());
			count += 3;
		}
		if (!cause.empty()) {
			pattern += fmt::format("{{{}}}", count);
			args.push_back(cause);
			++count;
		}
		cause = fmt::vformat(pattern, args);
	}

	if constexpr (kEvent) {
		EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
		EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
		WriteEvent(event, static_cast<ULONG>(data.size()), data.data(), &relatedActivityId);
	}

#if 0
	try {
		// std::rethrow_exception(exception);
		throw;
	} catch (const internal::BaseException& e) {
		EVENT_DESCRIPTOR event(e.GetEvent());
		const internal::LogData& logData = e.GetLogData();
		const source_location& sourceLocation = e.GetSourceLocation();

		if constexpr (kPrint) {
			fmt::dynamic_format_arg_store<fmt::format_context> args;
			logData.CopyArgumentsTo(args);
			try {
				throw;
			} catch (const windows_error& ex) {
				args.push_back(win32_error{static_cast<DWORD>(ex.code().value())});
			} catch (const rpc_error& ex) {
				args.push_back(rpc_status{static_cast<RPC_STATUS>(ex.code().value())});
			} catch (const com_error& ex) {
				args.push_back(hresult{static_cast<HRESULT>(ex.code().value())});
			} catch (const system_error& ex) {
				args.push_back(ex.code().value());
			} catch (const std::system_error& ex) {
				args.push_back(ex.code().value());
			} catch (...) {
				// do nothing
			}
			cause = fmt::format("\tcaused by: {}\n\t\tat {}({}) ({})\n{}",
			                    fmt::vformat(internal::GetEventMessagePattern(event.Id), args),
			                    sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name(),
			                    cause);
		}
		if constexpr (kEvent) {
			std::vector<EVENT_DATA_DESCRIPTOR> data;
			logData.CopyArgumentsTo(data);
			try {
				throw;
			} catch (const windows_error& ex) {
				Set(data.emplace_back(), static_cast<DWORD>(ex.code().value()));
			} catch (const rpc_error& ex) {
				Set(data.emplace_back(), static_cast<RPC_STATUS>(ex.code().value()));
			} catch (const com_error& ex) {
				Set(data.emplace_back(), static_cast<HRESULT>(ex.code().value()));
			} catch (const system_error& ex) {
				Set(data.emplace_back(), ex.code().value());
			} catch (const std::system_error& ex) {
				Set(data.emplace_back(), ex.code().value());
			} catch (...) {
				// do nothing
			}
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			WriteEvent(event, static_cast<ULONG>(data.size()), data.data(), pRelatedActivityId);
		}
	} catch (const windows_error& e) {
		event = evt::windows_error;
		if constexpr (kPrint) {
			args.push_back(e.what());
			args.push_back(win32_error{static_cast<DWORD>(e.code().value())});
		}
		if constexpr (kEvent) {
			Set(data.emplace_back(), e.what());
			Set(data.emplace_back(), static_cast<DWORD>(e.code().value()));
		}
	} catch (int e) {
		if constexpr (kPrint) {
			cause = fmt::format("\tcaused by: {}\n{}",
			                    fmt::vformat(internal::GetEventMessagePattern(event.Id), ex.what(), ),
			                    cause);
		}
		if constexpr (kEvent) {
			EVENT_DATA_DESCRIPTOR data[2];
			event = evt::windows_error;
			Set(data[0], e.what());
			Set(data[1], static_cast<DWORD>(ex.code().value()));
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			WriteEvent(event, 2, data, pRelatedActivityId);
		}
	} catch (const system_error& e) {
		if constexpr (kPrint) {
			USHORT eventId;
			// fmt::dynamic_format_arg_store<fmt::format_context> args;
			fmt::format_arg_store<fmt::format_context> args;
			try {
				throw;
			} catch (const windows_error& ex) {
				eventId = evt::windows_error.Id;
				args = fmt::make_format_args(e.what(), win32_error{static_cast<DWORD>(ex.code().value())});
			} catch (const rpc_error& ex) {
				eventId = evt::rpc_error.Id;
				args = fmt::make_format_args(e.what(), rpc_status{static_cast<RPC_STATUS>(ex.code().value())});
			} catch (const com_error& ex) {
				eventId = evt::com_error.Id;
				args = fmt::make_format_args(e.what(), hresult{static_cast<HRESULT>(ex.code().value())});
			} catch (...) {
				eventId = evt::system_error.Id;
				args = fmt::make_format_args(e.what(), e.code().value());
			}
			cause = fmt::format("\tcaused by: {}\n{}",
			                    fmt::vformat(internal::GetEventMessagePattern(eventId), args),
			                    cause);
		}
		if constexpr (kEvent) {
			EVENT_DESCRIPTOR event;
			EVENT_DATA_DESCRIPTOR data[2];
			try {
				throw;
			} catch (const windows_error& ex) {
				event = evt::windows_error;
				Set(data[0], e.what());
				Set(data[1], static_cast<DWORD>(ex.code().value()));
			} catch (const rpc_error& ex) {
				event = evt::rpc_error;
				Set(data[0], e.what());
				Set(data[1], static_cast<RPC_STATUS>(ex.code().value()));
			} catch (const com_error& ex) {
				event = evt::com_error;
				Set(data[0], e.what());
				Set(data[1], static_cast<HRESULT>(ex.code().value()));
			} catch (...) {
				event = evt::system_error;
				Set(data[0], e.what());
				Set(data[1], e.code().value());
			}
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			WriteEvent(event, 2, data, pRelatedActivityId);
		}
	} catch (const std::system_error& e) {
		if constexpr (kPrint) {
			cause = fmt::format("\tcaused by: {}\n{}",
			                    fmt::format(internal::GetEventMessagePattern(evt::system_error.Id), e.what(), e.code().value()),
			                    cause);
		}
		if constexpr (kEvent) {
			EVENT_DESCRIPTOR event(evt::system_error);
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			EVENT_DATA_DESCRIPTOR data[2];
			Set(data[0], e.what());
			Set(data[1], e.code().value());
			WriteEvent(event, 2, data, pRelatedActivityId);
		}
	} catch (const std::exception& e) {
		if constexpr (kPrint) {
			cause = fmt::format("\tcaused by: {}\n{}",
			                    fmt::format(internal::GetEventMessagePattern(evt::std_exception.Id), e.what()),
			                    cause);
		}
		if constexpr (kEvent) {
			EVENT_DESCRIPTOR event(evt::std_exception);
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			EVENT_DATA_DESCRIPTOR data[1];
			Set(data[0], e.what());
			WriteEvent(event, 1, data, pRelatedActivityId);
		}
	} catch (...) {
		if constexpr (kPrint) {
			cause = fmt::format("\tcaused by: {}\n{}",
			                    internal::GetEventMessagePattern(evt::exception.Id),
			                    cause);
		}
		if constexpr (kEvent) {
			EVENT_DESCRIPTOR event(evt::exception);
			EventDescSetLevel(&event, static_cast<std::underlying_type_t<Priority>>(priority));
			EventDescOrKeyword(&event, M3C_KEYWORD_EXCEPTION);
			WriteEvent(event, 0, nullptr, pRelatedActivityId);
		}
	}
#endif
}

template void Log::WriteExceptionEvent<false, false>(Priority, const GUID&, std::string&);
template void Log::WriteExceptionEvent<false, true>(Priority, const GUID&, std::string&);
template void Log::WriteExceptionEvent<true, false>(Priority, const GUID&, std::string&);
template void Log::WriteExceptionEvent<true, true>(Priority, const GUID&, std::string&);

void Log::Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const char* const arg) noexcept {
	assert(arg);
	EventDataDescCreate(&data, arg, static_cast<ULONG>(strlen(arg) * sizeof(char) + sizeof(char)));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const wchar_t* const arg) noexcept {
	assert(arg);
	EventDataDescCreate(&data, arg, static_cast<ULONG>(wcslen(arg) * sizeof(wchar_t) + sizeof(wchar_t)));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const std::string& arg) noexcept {
	EventDataDescCreate(&data, arg.c_str(), static_cast<ULONG>(arg.size() * sizeof(char) + sizeof(char)));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, _In_z_ const std::wstring& arg) noexcept {
	EventDataDescCreate(&data, arg.c_str(), static_cast<ULONG>(arg.size() * sizeof(wchar_t) + sizeof(wchar_t)));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, const internal::win32_error& arg) noexcept {
	EventDataDescCreate(&data, &arg.code, sizeof(arg.code));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, const internal::rpc_status& arg) noexcept {
	EventDataDescCreate(&data, &arg.code, sizeof(arg.code));
}

void Log::Set(EVENT_DATA_DESCRIPTOR& data, const internal::hresult& arg) noexcept {
	EventDataDescCreate(&data, &arg.code, sizeof(arg.code));
}

bool Log::SetActivityId(_Out_ GUID& activityId) noexcept {
	const ULONG result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &activityId);
	if (result != ERROR_SUCCESS) [[unlikely]] {
		Error(evt::EventActivityIdControl, EVENT_ACTIVITY_CTRL_CREATE_SET_ID, make_win32_error(result));
		return false;
	}
	return true;
}

void Log::ResetActivityId(GUID activityId) noexcept {
	const ULONG result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_SET_ID, &activityId);
	if (result != ERROR_SUCCESS) [[unlikely]] {
		Error(evt::EventActivityIdControl, EVENT_ACTIVITY_CTRL_SET_ID, make_win32_error(result));
	}
}

}  // namespace m3c
