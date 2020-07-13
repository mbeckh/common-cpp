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

#include <fmt/core.h>
#include <llamalog/LogLine.h>
#include <llamalog/llamalog.h>
#include <llamalog/winapi_format.h>  // IWYU pragma: keep

#include <sal.h>

#include <new>
#include <string>

namespace m3c {

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
		return fmt::format("{:%}", lg::error_code{condition});
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

namespace internal {

namespace {

/// @brief Helper that logs a message without throwing any exception.
/// @param priority The priority of the log message.
/// @param file The name of the current source file.
/// @param line The currently source code line.
/// @param function The name of the current function.
/// @param e The exception.
/// @param thunk A function that receives @p log, the priority, the message and the exception and calls @p log accordingly.
/// @param log A function that logs a message at level `llamalog::Priority::kError`.
void SafeLog(const llamalog::Priority priority, _In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, const std::exception& e, _In_ void (*const thunk)(_In_opt_ void*, llamalog::Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		thunk(log, priority, file, line, function, e);
	} catch (...) {
		// try to log error that happened during error logging, use "_DEBUG" because it logs for any type of exception
		ExceptionToHRESULT_DEBUG(
			file, line, function,
			[](void* /* p */, const llamalog::Priority /* priority */, _In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, const std::exception& e) {
				try {
					// always log as error, argument priority is ignored, use original source location
					llamalog::Log(llamalog::Priority::kError, file, line, function, "{}", e);
				} catch (...) {
					// cannot log
					LLAMALOG_PANIC("ExceptionToHRESULT");
				}
			},
			nullptr);
	}
}

}  // namespace

HRESULT ExceptionToHRESULT_DEBUG(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*const thunk)(_In_opt_ void*, llamalog::Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		throw;
	} catch (const com_invalid_argument_exception& e) {
		// calling with invalid argument may happen outside the scope of our code, so only log as debug
		SafeLog(llamalog::Priority::kDebug, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const com_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const windows_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return HRESULT_FROM_WIN32(e.code().value());
	} catch (const rpc_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, e.code().value());
	} catch (const std::bad_alloc& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return E_OUTOFMEMORY;
	} catch (const std::exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return E_FAIL;
	} catch (...) {
		// MUST supply a proper exception to the formatter
		SafeLog(llamalog::Priority::kError, file, line, function, std::exception("Unknown exception"), thunk, log);
		return E_FAIL;
	}
}

HRESULT ExceptionToHRESULT_ERROR(_In_z_ const char* file, std::uint32_t line, _In_z_ const char* function, _In_ void (*const thunk)(_In_opt_ void*, llamalog::Priority, _In_z_ const char*, std::uint32_t, _In_z_ const char*, const std::exception&), _In_opt_ void* const log) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	try {
		throw;
	} catch (const com_invalid_argument_exception& e) {
		// don't log trivial messages
		return e.code().value();
	} catch (const com_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return e.code().value();
	} catch (const windows_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return HRESULT_FROM_WIN32(e.code().value());
	} catch (const rpc_exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, e.code().value());
	} catch (const std::bad_alloc& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return E_OUTOFMEMORY;
	} catch (const std::exception& e) {
		SafeLog(llamalog::Priority::kError, file, line, function, e, thunk, log);
		return E_FAIL;
	} catch (...) {
		// MUST supply a proper exception to the formatter
		SafeLog(llamalog::Priority::kError, file, line, function, std::exception("Unknown exception"), thunk, log);
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

}  // namespace internal

}  // namespace m3c
