/*
Copyright 2020 Michael Beckh

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

#include "m3c/rpc_string.h"

#include "m3c/Log.h"
#include "m3c/exception.h"

#include "m3c.events.h"

#include <memory>
#include <type_traits>

namespace m3c {

template <AnyOf<char, wchar_t> T>
basic_rpc_string<T>::basic_rpc_string(basic_rpc_string&& str) noexcept
    : m_ptr(str.release()) {
	// empty
}

template <AnyOf<char, wchar_t> T>
basic_rpc_string<T>::~basic_rpc_string() noexcept {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) {
		[[unlikely]];
		// destructor SHOULD NOT throw
		Log::Error(evt::MemoryLeak_R, rpc_status(status));
	}
}

template <AnyOf<char, wchar_t> T>
basic_rpc_string<T>& basic_rpc_string<T>::operator=(basic_rpc_string<T>&& p) noexcept {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) {
		[[unlikely]];
		// move operator SHOULD NOT throw
		Log::Error(evt::MemoryLeak_R, rpc_status(status));
	}
	m_ptr = p.release();
	return *this;
}

template <AnyOf<char, wchar_t> T>
basic_rpc_string<T>& basic_rpc_string<T>::operator=(std::nullptr_t) {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) {
		[[unlikely]];
		throw rpc_error(status) + evt::MemoryLeak_R;
	}
	m_ptr = nullptr;
	return *this;
}

template <AnyOf<char, wchar_t> T>
_Ret_notnull_ typename basic_rpc_string<T>::rpc_str_type* basic_rpc_string<T>::operator&() {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) {
		[[unlikely]];
		throw rpc_error(status) + evt::MemoryLeak_R;
	}
	return std::addressof(m_ptr);
}

template <>
RPC_STATUS basic_rpc_string<char>::destroy() noexcept {
	return RpcStringFreeA(&m_ptr);
}

template <>
RPC_STATUS basic_rpc_string<wchar_t>::destroy() noexcept {
	return RpcStringFreeW(&m_ptr);
}

template class basic_rpc_string<char>;
template class basic_rpc_string<wchar_t>;

}  // namespace m3c
