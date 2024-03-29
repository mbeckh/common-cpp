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

#include "m3c/exception.h"

#include "m3c/Log.h"

#include "m3c.events.h"

#include <cstring>
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

	const char* message = __super::what();  // std::exception::what is noexcept
	try {
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
		Log::ErrorException(evt::system_error_what, m_code.category().name(), message, m_code.value());
	}
	return "<Error>";
}

namespace internal {

// in source file to allow pure virtual destructor in header
template <LogMessage M>
BaseException<M>::~BaseException() noexcept = default;

template class BaseException<const EVENT_DESCRIPTOR&>;
template class BaseException<const char*>;

template class ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&>;
template class ExceptionDetail<std::exception, const char*>;
template class ExceptionDetail<windows_error, const EVENT_DESCRIPTOR&>;
template class ExceptionDetail<windows_error, const char*>;
template class ExceptionDetail<rpc_error, const EVENT_DESCRIPTOR&>;
template class ExceptionDetail<rpc_error, const char*>;
template class ExceptionDetail<com_error, const EVENT_DESCRIPTOR&>;
template class ExceptionDetail<com_error, const char*>;
template class ExceptionDetail<com_invalid_argument_error, const EVENT_DESCRIPTOR&>;
template class ExceptionDetail<com_invalid_argument_error, const char*>;

}  // namespace internal

}  // namespace m3c

template m3c::internal::ExceptionDetail<std::exception, const char*> operator+(std::exception&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<std::exception, const char*>(std::forward<std::exception>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::windows_error, const char*> operator+(m3c::windows_error&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::windows_error, const char*>(std::forward<m3c::windows_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::rpc_error, const char*> operator+(m3c::rpc_error&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::rpc_error, const char*>(std::forward<m3c::rpc_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_error, const char*> operator+(m3c::com_error&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_error, const char*>(std::forward<m3c::com_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const char*> operator+(m3c::com_invalid_argument_error&& exception, m3c::internal::DefaultContext&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const char*>(std::forward<m3c::com_invalid_argument_error>(exception), context)));

template m3c::internal::ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&> operator+(std::exception&& exception, m3c::internal::ExceptionContext<const EVENT_DESCRIPTOR&>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<std::exception, const EVENT_DESCRIPTOR&>(std::forward<std::exception>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::windows_error, const EVENT_DESCRIPTOR&> operator+(m3c::windows_error&& exception, m3c::internal::ExceptionContext<const EVENT_DESCRIPTOR&>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::windows_error, const EVENT_DESCRIPTOR&>(std::forward<m3c::windows_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::rpc_error, const EVENT_DESCRIPTOR&> operator+(m3c::rpc_error&& exception, m3c::internal::ExceptionContext<const EVENT_DESCRIPTOR&>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::rpc_error, const EVENT_DESCRIPTOR&>(std::forward<m3c::rpc_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_error, const EVENT_DESCRIPTOR&> operator+(m3c::com_error&& exception, m3c::internal::ExceptionContext<const EVENT_DESCRIPTOR&>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_error, const EVENT_DESCRIPTOR&>(std::forward<m3c::com_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const EVENT_DESCRIPTOR&> operator+(m3c::com_invalid_argument_error&& exception, m3c::internal::ExceptionContext<const EVENT_DESCRIPTOR&>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const EVENT_DESCRIPTOR&>(std::forward<m3c::com_invalid_argument_error>(exception), context)));

template m3c::internal::ExceptionDetail<std::exception, const char*> operator+(std::exception&& exception, m3c::internal::ExceptionContext<const char*>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<std::exception, const char*>(std::forward<std::exception>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::windows_error, const char*> operator+(m3c::windows_error&& exception, m3c::internal::ExceptionContext<const char*>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::windows_error, const char*>(std::forward<m3c::windows_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::rpc_error, const char*> operator+(m3c::rpc_error&& exception, m3c::internal::ExceptionContext<const char*>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::rpc_error, const char*>(std::forward<m3c::rpc_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_error, const char*> operator+(m3c::com_error&& exception, m3c::internal::ExceptionContext<const char*>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_error, const char*>(std::forward<m3c::com_error>(exception), context)));
template m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const char*> operator+(m3c::com_invalid_argument_error&& exception, m3c::internal::ExceptionContext<const char*>&& context) noexcept(noexcept(m3c::internal::ExceptionDetail<m3c::com_invalid_argument_error, const char*>(std::forward<m3c::com_invalid_argument_error>(exception), context)));
