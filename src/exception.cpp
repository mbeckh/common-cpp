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

#include "m3c/exception.h"

#include "m3c_events.h"

#include "m3c/Log.h"
#include "m3c/source_location.h"

#include <cstring>
#include <exception>
#include <new>
#include <string>

namespace m3c {

system_error::system_error(const int code, const std::error_category& category, const std::string& message)
	: std::runtime_error(message)
	, m_code(code, category) {
	// empty
}

system_error::system_error(const int code, const std::error_category& category, _In_opt_z_ const char* __restrict const message)
	: std::runtime_error(message ? message : "")
	, m_code(code, category) {
	// empty
}

_Ret_z_ const char* system_error::what() const noexcept {
	if (m_what) {
		return m_what.get();
	}
	try {
		const char* message = __super::what();
		const std::size_t messageLen = std::strlen(message);

		const std::string errorMessage = m_code.message();
		const std::size_t errorMessageLen = errorMessage.length();

		const std::size_t offset = messageLen + (messageLen ? 2 : 0);
		const std::size_t len = offset + errorMessageLen;
		m_what = std::make_shared_for_overwrite<char[]>(len + 1);
		char* const ptr = m_what.get();
		if (messageLen) {
			std::memcpy(ptr, message, messageLen * sizeof(char));
			ptr[messageLen] = ':';
			ptr[messageLen + 1] = ' ';
		}
		std::memcpy(&ptr[offset], errorMessage.data(), errorMessageLen * sizeof(char));
		ptr[len] = '\0';
		return ptr;
	} catch (...) {
		Log::ErrorException(evt::system_error_what, m_code.category().name(), m_code.value());
	}
	return "<Error>";
}

namespace internal {

#if 0
DefaultContext::DefaultContext(Default, const source_location& sourceLocation) noexcept
	: m_sourceLocation(sourceLocation) {
}

DefaultContext::DefaultContext(const source_location& sourceLocation) noexcept
	: m_sourceLocation(sourceLocation) {
}

const source_location& DefaultContext::GetSourceLocation() noexcept {
	return m_sourceLocation;
}

ExceptionContext::ExceptionContext(EVENT_DESCRIPTOR event, source_location sourceLocation) noexcept
	: m_event(event)
	, DefaultContext(sourceLocation) {
}

EVENT_DESCRIPTOR&& ExceptionContext::GetEvent() && noexcept {
	return std::move(m_event);
}

#endif

BaseException::BaseException(DefaultContext&& context) noexcept
	: m_sourceLocation(std::move(context).GetSourceLocation()) {
	EventDescSetId(&m_event, 0);
}

BaseException::BaseException(ExceptionContext&& context) noexcept
	: m_event(std::move(context).GetEvent())
	, m_sourceLocation(std::move(context).GetSourceLocation()) {
}

#if 0

namespace {

/// @brief An error category for windows errors returned by `GetLastError`.
class error_category : public std::error_category {
public:
	/// @brief Create the singleton.
	explicit error_category(const char* const name) noexcept
		: m_name(name) {
		//empty
	}

	/// @brief The category name.
	/// @return The name.
	[[nodiscard]] _Ret_z_ const char* name() const noexcept final {
		return m_name;
	}

	/// @brief The message in the format '\<text\> (0x\<code\>)'.
	/// @return The message as a UTF-8 encoded string.
	[[nodiscard]] std::string message(const int condition) const final {
		//return fmt::format("{:%}", lg::error_code{condition});
		return "";
	}

private:
	const char* const m_name;
};

}  // namespace

const std::error_category& win32_category() noexcept {
	static const error_category kInstance("win32");
	return kInstance;
}

const std::error_category& rpc_category() noexcept {
	static const error_category kInstance("rpc");
	return kInstance;
}

const std::error_category& com_category() noexcept {
	static const error_category kInstance("com");
	return kInstance;
}

#endif
}  // namespace internal
namespace internal {

#if 0
namespace {

/// @brief Helper that logs a message without throwing any exception.
/// @param priority The priority of the log message.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param e The exception.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at level `llamalog::Priority::kError`.
void SafeLog(const Priority priority, _In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, const std::exception& e, _In_ void (*const thunk)(_In_opt_ void*, Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		thunk(log, priority, file, line, function, e);
	} catch (...) {
		// try to log error that happened during error logging, use "_DEBUG" because it logs for any type of exception
		ExceptionToHRESULT_DEBUG(
			file, line, function,
			[](void* /* p */, const Priority /* priority */, _In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, const std::exception& e) {
				try {
					// always log as error, argument priority is ignored, use original source location
					//llamalog::Log(Priority::kError, file, line, function, "{}", e);
				} catch (...) {
					// cannot log
					//LLAMALOG_PANIC("ExceptionToHRESULT");
				}
			},
			nullptr);
	}
}

}  // namespace

HRESULT ExceptionToHRESULT_DEBUG(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*const thunk)(_In_opt_ void*, Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		throw;
	} catch (const com_invalid_argument_exception& e) {
		// calling with invalid argument may happen outside the scope of our code, so only log as debug
		SafeLog(Priority::kDebug, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const com_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const windows_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return HRESULT_FROM_WIN32(e.code().value());
	} catch (const rpc_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, e.code().value());
	} catch (const std::bad_alloc& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return E_OUTOFMEMORY;
	} catch (const std::exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return E_FAIL;
	} catch (...) {
		// MUST supply a proper exception to the formatter
		SafeLog(Priority::kError, file, line, function, std::exception("Unknown exception"), thunk, log);
		return E_FAIL;
	}
}

HRESULT ExceptionToHRESULT_ERROR(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*const thunk)(_In_opt_ void*, Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		throw;
	} catch (const com_invalid_argument_exception& e) {
		// don't log trivial messages
		return e.code().value();
	} catch (const com_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const windows_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return HRESULT_FROM_WIN32(e.code().value());
	} catch (const rpc_exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, e.code().value());
	} catch (const std::bad_alloc& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return E_OUTOFMEMORY;
	} catch (const std::exception& e) {
		SafeLog(Priority::kError, file, line, function, e, thunk, log);
		return E_FAIL;
	} catch (...) {
		// MUST supply a proper exception to the formatter
		SafeLog(Priority::kError, file, line, function, std::exception("Unknown exception"), thunk, log);
		return E_FAIL;
	}
}

HRESULT ExceptionToHRESULT_FATAL() noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	// no logging at all
	try {
		throw;
	} catch (const com_invalid_argument_exception& e) {
		return e.code().value();
	} catch (const com_exception& e) {
		return e.code().value();
	} catch (const windows_exception& e) {
		return HRESULT_FROM_WIN32(e.code().value());
	} catch (const rpc_exception& e) {
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, e.code().value());
	} catch (const std::bad_alloc&) {
		return E_OUTOFMEMORY;
	} catch (const std::exception&) {
		return E_FAIL;
	} catch (...) {
		return E_FAIL;
	}
}

#endif
HRESULT ExceptionToHRESULT(const Priority priority, Thunk<Priority, HRESULT>&& log) noexcept {
	try {
		throw;
	} catch (const com_invalid_argument_error& e) {
		// calling with invalid argument may happen outside the scope of our code, so only log as debug
		const HRESULT hr = e.code().value();
		log.thunk(log.function, Priority::kDebug, hr);
		return hr;
	} catch (const com_error& e) {
		const HRESULT hr = e.code().value();
		log.thunk(log.function, priority, hr);
		return hr;
	} catch (const windows_error& e) {
		const HRESULT hr = HRESULT_FROM_WIN32(e.code().value());
		log.thunk(log.function, priority, hr);
		return hr;
	} catch (const rpc_error& e) {
		const HRESULT hr = HRESULT_FROM_WIN32(e.code().value());
		log.thunk(log.function, priority, hr);
		return hr;
	} catch (const std::bad_alloc&) {
		log.thunk(log.function, priority, E_OUTOFMEMORY);
		return E_OUTOFMEMORY;
	} catch (...) {
		log.thunk(log.function, priority, E_FAIL);
		return E_FAIL;
		//	} catch (...) {
		//	log.thunk(log.function, priority);
		// MUST supply a proper exception to the formatter
		// LogWithException({context.GetEvent(), std::current_exception(), context.GetSourceLocation()}, std::forward<Args>(args)...);
		// SafeLog(Priority::kError, file, line, function, std::exception("Unknown exception"), thunk, log);
		// return E_FAIL;
	}
}

}  // namespace internal

}  // namespace m3c
