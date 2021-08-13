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
/// @copyright Code marked with "from NanoLog" is based on NanoLog (https://github.com/Iyengar111/NanoLog, commit
/// 40a53c36e0336af45f7664abeb939f220f78273e), copyright 2016 Karthik Iyengar and distributed under the MIT License.
#pragma once

/*

Distributed under the MIT License (MIT)

    Copyright (c) 2016 Karthik Iyengar

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "source_location.h"

#include <windows.h>
//
#include "type_traits.h"

#include <evntprov.h>
#include <rpc.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifndef M3C_LOGDATA_SIZE
/// @brief The size of a log line in bytes.
/// @details Defined as a macro to allow redefinition for different use cases.
/// @note The value MUST be a power of 2, else compilation will fail.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Use macro to allow override in build settings.
#define M3C_LOGDATA_SIZE 128
#endif

namespace m3c {

namespace internal {

template <typename T>
struct error_codeX {
	T code;
};

struct win32_error final {
	DWORD code;

	operator DWORD() const noexcept {
		return code;
	}
};
static_assert(sizeof(win32_error) == sizeof(DWORD));

struct hresult final {
	HRESULT code;

	operator HRESULT() const noexcept {
		return code;
	}
};
static_assert(sizeof(hresult) == sizeof(HRESULT));

struct rpc_status final {
	RPC_STATUS code;

	operator RPC_STATUS() const noexcept {
		return code;
	}
};
static_assert(sizeof(rpc_status) == sizeof(RPC_STATUS));
}  // namespace internal

constexpr inline internal::win32_error make_win32_error(const DWORD code = GetLastError()) noexcept {
	return internal::win32_error{code};
}
constexpr inline internal::hresult make_hresult(const HRESULT code) noexcept {
	return internal::hresult{code};
}
constexpr inline internal::rpc_status make_rpc_status(const RPC_STATUS code) noexcept {
	return internal::rpc_status{code};
}

}  // namespace m3c
namespace m3c::internal {
class LogData;

/// @brief Type of the function to create a formatter argument.
template <typename T, typename F>
using Format = F (*)(_In_ const T* __restrict) noexcept;


namespace internal {

/// @brief Helper function to create a type by copying from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T>
void Copy(_In_reads_bytes_(sizeof(T)) const std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) {
	new (dst) T(*reinterpret_cast<const T*>(src));
}

/// @brief Helper function to create a type by moving from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T, typename std::enable_if_t<std::is_nothrow_move_constructible_v<T>, int> = 0>
void Move(_Inout_updates_bytes_(sizeof(T)) std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	new (dst) T(std::move(*reinterpret_cast<T*>(src)));
}

/// @brief Helper function to create a type by copying from the source.
/// @details This version is used if the type does not support moving.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T, typename std::enable_if_t<!std::is_nothrow_move_constructible_v<T>, int> = 0>
void Move(_In_reads_bytes_(sizeof(T)) std::byte* __restrict const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_copy_constructible_v<T>, "type MUST be nothrow copy constructible of it's not nothrow move constructible");
	new (dst) T(*reinterpret_cast<const T*>(src));
}

/// @brief Helper function to call the destructor of the internal object.
/// @tparam T The type of the argument.
/// @param obj The object.
template <typename T>
void Destruct(_Inout_updates_bytes_(sizeof(T)) std::byte* __restrict const obj) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Destruct MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_destructible_v<T>, "type MUST be nothrow destructible");
	reinterpret_cast<T*>(obj)->~T();
}

using Format = void* (*) (_In_ const void* __restrict) noexcept;

/// @brief A struct with all functions to manage objects in the buffer.
/// @details This is the version used to access the data of `FunctionTableInstance` in the buffer.
struct FunctionTable {
	/// @brief Type of the function to construct an argument in the buffer by copying.
	using Copy = void (*)(_In_ const std::byte* __restrict, _Out_ std::byte* __restrict);
	/// @brief Type of the function to construct an argument in the buffer by moving.
	using Move = void (*)(_Inout_ std::byte* __restrict, _Out_ std::byte* __restrict) noexcept;
	/// @brief Type of the function to destruct an argument in the buffer.
	using Destruct = void (*)(_Inout_ std::byte* __restrict) noexcept;

	/// @brief A pointer to a function which creates the custom type either by copying. Both addresses can be assumed to be properly aligned.
	Copy copy;
	/// @brief A pointer to a function which creates the custom type either by copy or move, whichever is
	/// more efficient. Both addresses can be assumed to be properly aligned.
	Move move;
	/// @brief A pointer to a function which calls the type's destructor.
	Destruct destruct;
};

/// @brief A struct with all functions to manage objects in the buffer.
/// @details This is the actual function table with all pointers.
/// @tparam T The type.
/// @tparam kPointer `true` if this is a pointer value with null-safe formatting.
template <typename T>
struct FunctionTableInstance {
	/// @brief A pointer to a function which creates the custom type either by copying. Both addresses can be assumed to be properly aligned.
	FunctionTable::Copy copy = Copy<T>;
	/// @brief A pointer to a function which creates the custom type either by copy or move, whichever is
	/// more efficient. Both addresses can be assumed to be properly aligned.
	FunctionTable::Move move = Move<T>;
	/// @brief A pointer to a function which calls the type's destructor.
	FunctionTable::Destruct destruct = Destruct<T>;
};

constexpr std::size_t kMaxCustomTypeSize = 0xFFFFFFFu;  // allow max. 255 MB

}  // namespace internal

/// @brief The class contains all data for formatting and output which happens asynchronously.
/// @details @internal The stack buffer is allocated in the base class for a better memory layout.
/// @copyright The interface of this class is based on `class NanoLogLine` from NanoLog.
class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) LogData {
public:
	/// @brief Create a new target for the various `operator<<` overloads.
	/// @details The timestamp is not set until `#GenerateTimestamp` is called.
	/// @param priority The `#Priority`.
	/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	/// @param sourceLocation The source location.
	LogData() noexcept = default;

	/// @brief Copy the buffers. @details The copy constructor is required for `std::curent_exception`.
	/// @param logLine The source log line.
	LogData(const LogData& logLine);

	/// @brief Move the buffers.
	/// @param logLine The source log line.
	LogData(LogData&& logLine) noexcept;

	/// @brief Calls the destructor of every custom argument.
	~LogData() noexcept;

public:
	/// @brief Copy the buffers. @details The assignment operator is required for `std::exception`.
	/// @param logLine The source log line.
	/// @return This object.
	LogData& operator=(const LogData& logLine);

	/// @brief Move the buffers.
	/// @param logLine The source log line.
	/// @return This object.
	LogData& operator=(LogData&& logLine) noexcept;

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogData& operator<<(bool arg);

	/// @brief Add a log argument. @note A `char` is distinct from both `signed char` and `unsigned char`.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogData& operator<<(char arg);

	/// @brief Add a log argument. @note A `char` is distinct from both `signed char` and `unsigned char`.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogData& operator<<(wchar_t arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogData& operator<<(signed char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogData& operator<<(unsigned char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogData& operator<<(signed short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogData& operator<<(unsigned short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogData& operator<<(signed int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogData& operator<<(unsigned int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogData& operator<<(signed long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogData& operator<<(unsigned long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
	LogData& operator<<(signed long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogData& operator<<(unsigned long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogData& operator<<(float arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogData& operator<<(double arg);

	/// @brief Add an address as a log argument. @note Please note that one MUST NOT use this pointer to access	an
	/// object because at the time of logging, the object might no longer exist.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogData& operator<<(_In_opt_ const void* __restrict arg);

	/// @brief Add a nullptr s a log argument. Same as providing a void* pointer but required for overload resolution.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogData& operator<<(std::nullptr_t /* arg */);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(_In_z_ const char* __restrict arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(_In_z_ const wchar_t* __restrict arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(const std::string& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(const std::wstring& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(const std::string_view& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogData& operator<<(const std::wstring_view& arg);

	LogData& operator<<(const GUID& arg);
	LogData& operator<<(const FILETIME& arg);
	LogData& operator<<(const SYSTEMTIME& arg);
	LogData& operator<<(const SID& arg);
	LogData& operator<<(const win32_error& arg);
	LogData& operator<<(const rpc_status& arg);
	LogData& operator<<(const hresult& arg);

public:
	/// @brief Get the arguments for formatting the message.
	/// @remarks Using a template removes the need to include the headers of {fmt} in all translation units.
	/// MUST use argument for `std::vector` because guaranteed copy elision does not take place in debug builds.
	/// @tparam T This MUST be `std::vector<fmt::format_context::format_arg>`.
	/// @param args The `std::vector` to receive the message arguments.
	template <typename T>
	std::uint32_t CopyArgumentsTo(T& args) const;

	/// @brief Returns the formatted log message. @note The name `GetMessage` would conflict with the function from the
	/// Windows API having the same name.
	/// @return The log message.
	//[[nodiscard]] std::string GetLogMessage() const;

	/// @brief Copy a log argument of a custom type to the argument buffer.
	/// @details This function handles types which are trivially copyable.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument. This type MUST have a copy constructor.
	/// @param arg The object.
	/// @return The current object for method chaining.
	template <typename T, typename F, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
	LogData& AddCustomArgument(const T& arg, Format<T, F>& format) {
		using X = std::remove_cv_t<T>;

		static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
		static_assert(sizeof(X) <= internal::kMaxCustomTypeSize, "custom type is too large");
		static_assert(sizeof(void(*)()) == sizeof(format));
		WriteTriviallyCopyable(reinterpret_cast<const std::byte*>(std::addressof(arg)), sizeof(X), alignof(X), reinterpret_cast<void (*)()>(format));
		return *this;
	}

#if 0
	/// @brief Copy a log argument of a pointer to a custom type to the argument buffer.
	/// @details This function handles types which are trivially copyable.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument. This type MUST have a copy constructor.
	/// @param arg The pointer to an object.
	/// @return The current object for method chaining.
	template <typename T, typename F, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
	LogData& AddCustomArgument(const T* arg, Format<T, F>& format) {
		if (arg) {
			using X = std::remove_pointer_t<std::remove_cv_t<T>>;

			static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
			static_assert(sizeof(X) <= internal::kMaxCustomTypeSize, "custom type is too large");
			WriteTriviallyCopyable(reinterpret_cast<const std::byte*>(arg), sizeof(X), alignof(X), reinterpret_cast<void (*)()>(format));
		} else {
			WriteNullPointer();
		}
		return *this;
	}

	/// @brief Copy a log argument of a custom type to the argument buffer.
	/// @details This function handles types which are not trivially copyable. However, the type MUST support copy construction.
	/// @note The type @p T MUST be both copy constructible and either nothrow move constructible or nothrow copy constructible.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument.
	/// @param arg The object.
	/// @return The current object for method chaining.
	template <typename T, std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
	LogData& AddCustomArgument(const T& arg) {
		using X = std::remove_cv_t<T>;

		static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
		static_assert(sizeof(X) <= internal::kMaxCustomTypeSize, "custom type is too large");
		static_assert(std::is_copy_constructible_v<X>, "type MUST be copy constructible");

		using FunctionTable = internal::FunctionTableInstance<X, false, kEscape>;  // offsetof does not support , in type
		static_assert(sizeof(FunctionTable) == sizeof(internal::FunctionTable));
		static_assert(offsetof(FunctionTable, copy) == offsetof(internal::FunctionTable, copy));
		static_assert(offsetof(FunctionTable, move) == offsetof(internal::FunctionTable, move));
		static_assert(offsetof(FunctionTable, destruct) == offsetof(internal::FunctionTable, destruct));
		static_assert(offsetof(FunctionTable, createFormatArg) == offsetof(internal::FunctionTable, createFormatArg));

		static constexpr FunctionTable kFunctionTable;
		std::byte* __restrict const ptr = WriteNonTriviallyCopyable(sizeof(X), alignof(X), static_cast<const void*>(&kFunctionTable));
		new (ptr) X(arg);
		return *this;
	}

	/// @brief Copy a log argument of a pointer to a custom type to the argument buffer.
	/// @details This function handles types which are not trivially copyable. However, the type MUST support copy construction.
	/// @note The type @p T MUST be both copy constructible and either nothrow move constructible or nothrow copy constructible.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument.
	/// @param arg The pointer to an object.
	/// @return The current object for method chaining.
	template <typename T, bool kEscaped = false, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
	LogData& AddCustomArgument(const T* arg) {
		if (arg) {
			using X = std::remove_pointer_t<std::remove_cv_t<T>>;

			static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
			static_assert(sizeof(X) <= internal::kMaxCustomTypeSize, "custom type is too large");
			static_assert(std::is_copy_constructible_v<X>, "type MUST be copy constructible");

			using FunctionTable = internal::FunctionTableInstance<X, true, kEscape>;  // offsetof does not support , in type
			static_assert(sizeof(FunctionTable) == sizeof(internal::FunctionTable));
			static_assert(offsetof(FunctionTable, copy) == offsetof(internal::FunctionTable, copy));
			static_assert(offsetof(FunctionTable, move) == offsetof(internal::FunctionTable, move));
			static_assert(offsetof(FunctionTable, destruct) == offsetof(internal::FunctionTable, destruct));
			static_assert(offsetof(FunctionTable, createFormatArg) == offsetof(internal::FunctionTable, createFormatArg));

			static constexpr FunctionTable kFunctionTable;
			std::byte* __restrict const ptr = WriteNonTriviallyCopyable(sizeof(X), alignof(X), static_cast<const void*>(&kFunctionTable));
			new (ptr) X(*arg);
		} else {
			WriteNullPointer();
		}
		return *this;
	}
#endif

public:
	using Size = std::uint32_t;    ///< @brief A data type for indexes in the buffer representing *bytes*.
	using Length = std::uint16_t;  ///< @brief The length of a string in number of *characters*.
	using Align = std::uint8_t;    ///<@ brief Alignment requirement of a data type in *bytes*.

private:
	/// @brief Get the argument buffer for writing.
	/// @return The start of the buffer.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) std::byte* GetBuffer() noexcept;

	/// @brief Get the argument buffer.
	/// @return The start of the buffer.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) const std::byte* GetBuffer() const noexcept;

	/// @brief Get the current position in the argument buffer ensuring that enough space exists for the next argument.
	/// @param additionalBytes The number of bytes that will be appended.
	/// @return The next write position.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) std::byte* GetWritePosition(Size additionalBytes);

	/// @brief Copy an argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	/// @copyright Derived from both methods `NanoLogLine::encode` from NanoLog.
	template <typename T>
	void Write(T arg);

	/// @brief Copy a pointer argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// If @p arg is the `nullptr`, a special NullValue argument is added.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	// template <typename T>
	// void WritePointer(const T* arg);

	/// @brief Copy a nullptr argument to the buffer.
	/// @remarks This function is used internally by `#WritePointer` and `#AddCustomArgument`.
	void WriteNullPointer();

	/// @brief Copy a string to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the string in characters (NOT
	/// including a terminating null character) and finally the string's characters (again NOT including a terminating null character).
	/// This function is optimized compared to `WriteString(const wchar_t*, std::size_t)` because `char` has no alignment requirements.
	/// @remarks The type of @p len is `std::size_t` because the check if the length exceeds the capacity of `#Length` happens inside this function.
	/// @param arg The string to add.
	/// @param len The string length in characters NOT including a terminating null character.
	/// @copyright Derived from `NanoLogLine::encode_c_string` from NanoLog.
	template <typename T>
	void WriteString(_In_reads_(len) const T* __restrict arg, std::size_t len);

	void WriteSID(const SID& arg);

	/// @brief Copy a string to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the string in characters (NOT
	/// including a terminating null character), optional padding as required and finally the string's characters (again
	/// NOT including a terminating null character).
	/// @remarks The type of @p len is `std::size_t` because the check if the length exceeds the capacity of `#Length` happens inside this function.
	/// @param arg The string to add.
	/// @param len The string length in characters NOT including a terminating null character.
	/// @copyright Derived from `NanoLogLine::encode_c_string` from NanoLog.
	// void WriteString(_In_reads_(len) const wchar_t* __restrict arg, std::size_t len);

	/// @brief Add a custom object to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the padding for @p T, a pointer
	/// to the function to create the formatter argument, the size of the data, padding as required and finally the
	/// bytes of @p ptr.
	/// @remark The function to create the formatter argument is supplied as a void (*)() pointer which removes the compile
	/// time dependency to {fmt} from this header.
	/// @param ptr A pointer to the argument data.
	/// @param objectSize The size of the object.
	/// @param align The alignment requirement of the type.
	/// @param createFormatArg A pointer to a function which has a single argument of type `std::byte*` and returns a
	/// newly created `fmt::format_context::format_arg` object.
	void WriteTriviallyCopyable(_In_reads_bytes_(objectSize) const std::byte* __restrict ptr, Size objectSize, Align align, _In_ void (*format)());

	/// @brief Add a custom object to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the padding for @p T, the pointer
	/// to the function table, the size of the data, padding as required and finally the bytes of the object.
	/// @note This function does NOT copy the object but only returns the target address.
	/// @remark @p functionTable is supplied as a void pointer to remove the compile time dependency to {fmt} from this header.
	/// @param objectSize The size of the object.
	/// @param align The alignment requirement of the type.
	/// @param functionTable A pointer to the `internal::FunctionTable`.
	/// @return An address where to copy the current argument.
	[[nodiscard]] __declspec(restrict) std::byte* WriteNonTriviallyCopyable(Size objectSize, Align align, _In_ const void* functionTable);

private:
	/// @brief The stack buffer used for small payloads.
	/// @copyright Same as `NanoLogLine::m_stack_buffer` from NanoLog.
	std::byte m_stackBuffer[M3C_LOGDATA_SIZE                        // target size
	                        - sizeof(bool)                          // m_hasNonTriviallyCopyable
	                        - sizeof(Size) * 2                      // m_used, m_size
	                        - sizeof(std::unique_ptr<std::byte[]>)  // m_heapBuffer
	];

	bool m_hasNonTriviallyCopyable = false;  ///< @brief `true` if at least one argument needs special handling on buffer operations. @hideinitializer

	/// @copyright Same as `NanoLogLine::m_bytes_used` from NanoLog. @hideinitializer
	Size m_used = 0;  ///< @brief The number of bytes used in the buffer.

	/// @copyright Same as `NanoLogLine::m_buffer_size` from NanoLog. @hideinitializer
	Size m_size = sizeof(m_stackBuffer);  ///< @brief The current capacity of the buffer in bytes.

	/// @copyright Same as `NanoLogLine::m_heap_buffer` from NanoLog.
	std::unique_ptr<std::byte[]> m_heapBuffer;  ///< The buffer on the heap if the stack buffer became too small.
};

}  // namespace m3c::internal
