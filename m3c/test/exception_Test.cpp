/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "m3c/exception.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <llamalog/llamalog.h>

#include <windows.h>

#include <exception>
#include <string>
#include <system_error>


namespace m3c::test {

namespace t = testing;

//
// windows_exception
//

TEST(windows_exceptionTest, ctor_WithArgs_HasValues) {
	const windows_exception error(ERROR_INSUFFICIENT_BUFFER, "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(ERROR_INSUFFICIENT_BUFFER, error.code().value());
}


//
// rpc_exception
//

TEST(rpc_exceptionTest, ctor_WithArgs_HasValues) {
	const rpc_exception error(RPC_S_STRING_TOO_LONG, "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(RPC_S_STRING_TOO_LONG, error.code().value());
}


//
// com_exception
//

TEST(com_exceptionTest, ctor_FromHRESULT_HasValues) {
	const com_exception error(E_INVALIDARG, "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_exceptionTest, ctor_FromWIN32_HasValues) {
	const com_exception error(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), error.code().value());
}

TEST(com_exceptionTest, ctor_FromRPC_HasValues) {
	const com_exception error(RPC_S_STRING_TOO_LONG, "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(RPC_S_STRING_TOO_LONG, error.code().value());
}


//
// com_invalid_argument_exception
//

TEST(com_invalid_argument_exceptionTest, ctor_FromMessageOnly_IsInvalidArgumentCode) {
	const com_invalid_argument_exception error("MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(E_INVALIDARG, error.code().value());
}

TEST(com_invalid_argument_exceptionTest, ctor_WithArgs_HasValues) {
	const com_invalid_argument_exception error(E_NOTIMPL, "MyMessage");
	const std::string str(error.what());

	EXPECT_THAT(str, t::StartsWith("MyMessage: "));
	EXPECT_EQ(E_NOTIMPL, error.code().value());
}


//
// ExceptionToHRESULT: No exception
//

TEST(ExceptionToHRESULT, call_NoException_ReturnResult) {
	HRESULT hr = [] {
		std::exception_ptr eptr;
		try {
			if (eptr) {
				std::rethrow_exception(eptr);
			}
			return S_FALSE;
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();
	EXPECT_EQ(S_FALSE, hr);
}


//
// ExceptionToHRESULT: windows_exception
//

TEST(ExceptionToHRESULT, call_ThrowWrappedWindowsException_ReturnCode) {
	HRESULT hr = [] {
		try {
			LLAMALOG_THROW(windows_exception(ERROR_INVALID_PARAMETER, "ERROR_INVALID_PARAMETER"));
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), hr);
}

TEST(ExceptionToHRESULT, call_ThrowPlainWindowsException_ReturnCode) {
	HRESULT hr = [] {
		try {
			throw windows_exception(ERROR_INVALID_PARAMETER, "E_FAIL");
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), hr);
}


//
// ExceptionToHRESULT: rpc_exception
//

TEST(ExceptionToHRESULT, call_ThrowWrappedRpcException_ReturnCode) {
	HRESULT hr = [] {
		try {
			LLAMALOG_THROW(rpc_exception(RPC_S_ADDRESS_ERROR, "RPC_S_ADDRESS_ERROR"));
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, RPC_S_ADDRESS_ERROR), hr);
}

TEST(ExceptionToHRESULT, call_ThrowPlainRpcException_ReturnCode) {
	HRESULT hr = [] {
		try {
			throw rpc_exception(RPC_S_ADDRESS_ERROR, "RPC_S_ADDRESS_ERROR");
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, RPC_S_ADDRESS_ERROR), hr);
}


//
// ExceptionToHRESULT: com_exception
//

TEST(ExceptionToHRESULT, call_ThrowWrappedComException_ReturnCode) {
	HRESULT hr = [] {
		try {
			LLAMALOG_THROW(com_exception(E_BOUNDS, "E_BOUNDS"));
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(E_BOUNDS, hr);
}

TEST(ExceptionToHRESULT, call_ThrowPlainComException_ReturnCode) {
	HRESULT hr = [] {
		try {
			throw com_exception(E_BOUNDS, "E_BOUNDS");
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(E_BOUNDS, hr);
}


//
// ExceptionToHRESULT: system_error
//

TEST(ExceptionToHRESULT, call_ThrowWrappedSystemError_ReturnCode) {
	HRESULT hr = [] {
		try {
			LLAMALOG_THROW(std::system_error(std::make_error_code(std::errc::address_in_use), "EADDRINUSE"));
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(E_FAIL, hr);
}

TEST(ExceptionToHRESULT, call_ThrowPlainSystemError_ReturnCode) {
	HRESULT hr = [] {
		try {
			throw std::system_error(std::make_error_code(std::errc::address_in_use), "EADDRINUSE");
		} catch (...) {
			return M3C_EXCEPTION_TO_HRESULT("Got exception: {}");
		}
	}();

	EXPECT_EQ(E_FAIL, hr);
}

}  // namespace m3c::test
