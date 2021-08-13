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
/// @brief Include the header file to allow compilation and analyzer checks.

#include "m3c/rpc_string.h"

#include "m3c_events.h"

#include "m3c/exception.h"

namespace m3c::internal {

template <typename T>
basic_rpc_string<T>::basic_rpc_string(basic_rpc_string&& str) noexcept
	: m_ptr(str.release()) {
	// empty
}

template <typename T>
basic_rpc_string<T>::~basic_rpc_string() noexcept {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) [[unlikely]] {
		// destructor SHOULD NOT throw
		Log::Error(evt::RpcStringFreeX, rpc_status{status});
	}
}

template <typename T>
basic_rpc_string<T>& basic_rpc_string<T>::operator=(basic_rpc_string<T>&& p) noexcept {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) [[unlikely]] {
		// move operator SHOULD NOT throw
		Log::Error(evt::RpcStringFreeX, rpc_status{status});
	}
	m_ptr = p.release();
	return *this;
}

template <typename T>
basic_rpc_string<T>& basic_rpc_string<T>::operator=(std::nullptr_t) {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) [[unlikely]] {
		throw rpc_error(status) + evt::RpcStringFreeX;
	}
	m_ptr = nullptr;
	return *this;
}

template <typename T>
_Ret_notnull_ T* basic_rpc_string<T>::operator&() {
	const RPC_STATUS status = destroy();
	if (status != RPC_S_OK) [[unlikely]] {
		throw rpc_error(status) + evt::RpcStringFreeX;
	}
	return std::addressof(m_ptr);
}

template <typename T>
RPC_STATUS basic_rpc_string<T>::destroy() noexcept {
	if constexpr (std::is_same_v<T, RPC_CSTR>) {
		return RpcStringFreeA(&m_ptr);
	} else if constexpr (std::is_same_v<T, RPC_WSTR>) {
		return RpcStringFreeW(&m_ptr);
	} else {
		__assume(0);
	}
}

template class basic_rpc_string<RPC_CSTR>;
template class basic_rpc_string<RPC_WSTR>;

}  // namespace m3c::internal
