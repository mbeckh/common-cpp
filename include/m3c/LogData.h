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

#include <m3c/LogArgs.h>
#include <m3c/format.h>
#include <m3c/type_traits.h>

#include <windows.h>
#include <oaidl.h>
#include <propidl.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifndef M3C_LOGDATA_SIZE
/// @brief The size of a log line in bytes.
/// @details Defined as a macro to allow redefinition for different use cases.
/// @note The value MUST be a power of 2, else compilation will fail.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Use macro to allow override in build settings.
#define M3C_LOGDATA_SIZE 128
#endif


namespace m3c {
namespace internal {
namespace logdata {

/// @brief The maximum size of a custom type.
constexpr std::size_t kMaxCustomTypeSize = 0xFFFFFFFu;  // allow max. 255 MB

using Size = std::uint32_t;    ///< @brief A data type for indexes in the buffer representing *bytes*.
using Length = std::uint16_t;  ///< @brief The length of a string in number of *characters*.
using Align = std::uint8_t;    ///<@ brief Alignment requirement of a data type in *bytes*.

/// @brief A structure with management data for a custom type.
struct FunctionTable {
	/// @brief Type of the function to fill an event data descriptor.
	using AddEventDataFunction = void (*)(_Inout_ LogEventArgs&, _In_ const std::byte* __restrict);
	/// @brief Type of the function to add a formatter argument.
	using AddFormatArgsFunction = void (*)(_Inout_ LogFormatArgs&, _In_ const std::byte* __restrict);

	const Align align;                          ///< @brief Alignment requirement.
	const Size size;                            ///< @brief Size of the type in bytes.
	const AddEventDataFunction addEventData;    ///< @brief Function to add argument to an event data structure.
	const AddFormatArgsFunction addFormatArgs;  ///< @brief Function to add argument to a formatter arguments structure.
};

/// @brief A structure with additional management data for custom types which are not trivially copyable.
struct FunctionTableNonTrivial : FunctionTable {
	/// @brief Type of the function to destruct an argument in the buffer.
	using DestructFunction = void (*)(_Inout_ std::byte* __restrict) noexcept;

	const DestructFunction destruct;  ///< @brief Function to destruct argument in the buffer.
};

/// @brief A structure with additional management data for custom types which are not no-throw constructible.
struct FunctionTableNoThrowConstructible : FunctionTableNonTrivial {
	/// @brief Type of the function to construct an argument in the buffer by copying.
	using CopyFunction = void (*)(_In_ const std::byte* __restrict, _Out_ std::byte* __restrict) noexcept;
	/// @brief Type of the function to construct an argument in the buffer by moving.
	using MoveFunction = void (*)(_Inout_ std::byte* __restrict, _Out_ std::byte* __restrict) noexcept;

	const CopyFunction copy;  ///< @brief Function to copy argument into the buffer.
	const MoveFunction move;  ///< @brief Function to move argument into the buffer.
};


/// @brief Helper function to add a formatter or event argument to an arguments container.
/// @tparam A The type of the arguments container.
/// @tparam T The type of the argument.
/// @param objectData The serialized byte stream of an object of type @p T.
template <typename A, typename T>
inline void AddLogArgs(_Inout_ A& args, _In_reads_bytes_(sizeof(T)) const std::byte* __restrict const objectData) noexcept(noexcept(args << std::declval<const T&>())) {
	args << *reinterpret_cast<const T*>(objectData);
}

/// @brief Helper function to create a type by copying from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T>
requires std::is_nothrow_copy_constructible_v<T>
inline void Copy(_In_reads_bytes_(sizeof(T)) const std::byte* __restrict const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	std::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
}

/// @brief Helper function to create a type by moving from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T>
requires std::is_nothrow_move_constructible_v<T>
inline void Move(_Inout_updates_bytes_(sizeof(T)) std::byte* __restrict const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	std::construct_at(reinterpret_cast<T*>(dst), std::move(*reinterpret_cast<T*>(src)));
}

/// @brief Helper function to create a type by copying from the source.
/// @details This version is used if the type does not support moving.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T>
requires requires {
	requires !std::is_nothrow_move_constructible_v<T>;
} && std::is_nothrow_copy_constructible_v<T>
inline void Move(_In_reads_bytes_(sizeof(T)) std::byte* __restrict const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	std::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<const T*>(src));
}

/// @brief Helper function to call the destructor of the internal object.
/// @tparam T The type of the argument.
/// @param obj The object.
template <typename T>
requires std::is_nothrow_destructible_v<T>
inline void Destruct(_Inout_updates_bytes_(sizeof(T)) std::byte* __restrict const obj) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Destruct MUST NOT be used for trivially copyable types");
	std::destroy_at(reinterpret_cast<T*>(obj));
}

}  // namespace logdata


/// @brief The base class for `LogData`.
/// @remarks The alignment of the class MUST match the alignment of `new` to allow copying padded values.
class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) LogDataBase {  // NOLINT(cppcoreguidelines-pro-type-member-init): m_stackBuffer is raw buffer.
private:
	using HeapBuffer = std::shared_ptr<std::byte[]>;

	using Size = internal::logdata::Size;      ///< @brief A data type for indexes in the buffer representing *bytes*.
	using Length = internal::logdata::Length;  ///< @brief The length of a string in number of *characters*.
	using Align = internal::logdata::Align;    ///<@ brief Alignment requirement of a data type in *bytes*.

	/// @brief Type of a structure with management data for a custom type.
	using FunctionTable = internal::logdata::FunctionTable;

	/// @brief Type of a structure with additional management data for custom types which are not trivially copyable.
	using FunctionTableNonTrivial = internal::logdata::FunctionTableNonTrivial;

	/// @brief Type of a structure with additional management data for custom types which are not no-throw constructible.
	using FunctionTableNoThrowConstructible = internal::logdata::FunctionTableNoThrowConstructible;

public:
	[[nodiscard]] constexpr LogDataBase() noexcept = default;  // NOLINT(cppcoreguidelines-pro-type-member-init): m_stackBuffer is raw buffer.

	/// @brief Copy the buffers. @details The copy constructor is required for exceptions being no-throw copyable
	/// @param other The source log data.
	[[nodiscard]] LogDataBase(const LogDataBase& other) noexcept;

	/// @brief Move the buffers. @details The copy constructor is required for exceptions being no-throw moveable.
	/// @param other The source log data.
	[[nodiscard]] LogDataBase(LogDataBase&& other) noexcept;

	/// @brief Calls the destructor of every custom argument.
	~LogDataBase() noexcept;

public:
	/// @brief Copy the buffers. @details The copy constructor is required for exceptions being no-throw assignable.
	/// @param other The source log data.
	/// @return This object.
	LogDataBase& operator=(const LogDataBase& other) noexcept;

	/// @brief Move the buffers. @details The copy constructor is required for exceptions being no-throw move-assignable.
	/// @param other The source log data.
	/// @return This object.
	LogDataBase& operator=(LogDataBase&& other) noexcept;

	/// @brief Add a log argument.
	/// @tparam T The type of the argument.
	/// @param arg The argument.
	/// @copyright Based on `NanoLogLine::operator<<(T)` from NanoLog.
	template <internal::TriviallyLoggable T>
	void AddArgument(const T& arg) {
		Write<std::remove_cv_t<T>>(arg);
	}

	/// @brief Add an address as a log argument. @note Please note that one MUST NOT use this pointer to access an
	/// object because at the time of logging, the object might no longer exist.
	/// @param arg The argument.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	void AddArgument(_In_opt_ const void* __restrict const arg) {
		Write<const void* __restrict>(arg);
	}

	/// @brief Add a nullptr s a log argument. Same as providing a void* pointer but required for overload resolution.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	void AddArgument(std::nullptr_t /* arg */) {
		Write<const void* __restrict>(nullptr);
	}

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @tparam CharT The character type.
	/// @param arg The argument.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	template <AnyOf<char, wchar_t> CharT>
	void AddArgument(_In_z_ const CharT* __restrict const arg) {
		WriteString(arg, std::char_traits<CharT>::length(arg));
	}

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @tparam T The type of the argument.
	/// @param arg The argument.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	template <internal::ConvertibleToCStr T>
	void AddArgument(const T& arg) {
		WriteString(arg.c_str(), internal::ConvertibleToCStrTraits<T>::length(arg));
	}

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @tparam Args Template arguments of `std::basic_string_view`.
	/// @param arg The argument.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	template <typename... Args>
	void AddArgument(const std::basic_string_view<Args...>& arg) {
		WriteString(arg.data(), arg.size());
	}

	/// @brief Add a log argument. @details The value is copied into the buffer.
	/// @param arg The argument.
	void AddArgument(_In_opt_ const SID& arg) {
		WriteSID(arg);
	}

	/// @brief Add a custom argument.
	/// @details Only this method if a custom `operator>>` MUST NOT be taken into account for adding the type.
	/// @tparam T The type of the argument.
	/// @param arg The argument.
	template <typename T>
	requires requires {
		requires !std::is_pointer_v<std::decay<T>>;
		requires(std::is_trivially_copyable_v<std::remove_cvref_t<T>> || std::is_copy_constructible_v<std::remove_cvref_t<T>>);
	}
	void AddCustomArgument(T&& arg) {
		using X = std::decay_t<T>;
		if constexpr (std::is_trivially_copyable_v<X>) {
			X* __restrict const address = WriteTriviallyCopyable<std::decay_t<T>>();  // not writing X gives nicer error messages
			std::memcpy(address, std::addressof(arg), sizeof(X));
		} else {
			X* __restrict const address = WriteNonTriviallyCopyable<std::decay_t<T>>();  // not writing X gives nicer error messages
			if constexpr (std::is_move_constructible_v<X>) {
				std::construct_at(address, std::forward<T>(arg));
			} else {
				std::construct_at(address, arg);
			}
		}
	}

public:
	/// @brief Get the arguments for writing the message.
	/// @tparam A The type of the log arguments.
	/// @param args The target to receive the message arguments.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	template <LogArgs A>
	void CopyArgumentsTo(_Inout_ A& args) const;

private:
	/// @brief Gets the heap buffer for writing.
	/// @remarks The object does not yet exist, if `m_hasHeapBuffer` is false.
	/// @return The heap buffer or uninitialized memory.
	[[nodiscard]] HeapBuffer& GetHeapBuffer() noexcept;

	/// @brief Gets the heap buffer for reading.
	/// @remarks The object does not yet exist, if `m_hasHeapBuffer` is false.
	/// @return The heap buffer or uninitialized memory.
	[[nodiscard]] const HeapBuffer& GetHeapBuffer() const noexcept;

	/// @brief Get the size of the heap buffer.
	/// @return The size of the heap buffer.
	[[nodiscard]] Size& GetHeapBufferSize() noexcept;

	/// @brief Get the size of the heap buffer.
	/// @return The size of the heap buffer.
	[[nodiscard]] const Size& GetHeapBufferSize() const noexcept;

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
	/// @param forceHeap If `true`, a heap buffer is allocated so that copying the object only requires a simple pointer copy.
	/// @return The next write position.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) std::byte* GetWritePosition(Size additionalBytes, bool forceHeap = false);

	/// @brief Copy an argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	/// @copyright Derived from both methods `NanoLogLine::encode` from NanoLog.
	template <typename T>
	void Write(std::conditional_t<std::is_fundamental_v<T>, const T, const T&> arg);

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

	/// @brief Copy a `SID` to the argument buffer.
	/// @param arg The `SID` to add.
	void WriteSID(const SID& arg);

	/// @brief Reserve space for a custom trivially copyable type in the argument buffer.
	/// @tparam T The type of the argument.
	/// @return The address where the type MUST be constructed by the caller.
	template <typename T>
	[[nodiscard]] _Ret_notnull_ T* WriteTriviallyCopyable() {
		static_assert(std::is_same_v<T, std::decay_t<T>>, "unnecessary code duplication");
		static_assert(std::is_trivially_copyable_v<T>, "type MUST be trivially copyable");
		static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
		static_assert(sizeof(T) <= internal::logdata::kMaxCustomTypeSize, "custom type is too large");

		constexpr static FunctionTable kFunctionTable{
		    .align = alignof(T),
		    .size = sizeof(T),
		    .addEventData = internal::logdata::AddLogArgs<LogEventArgs, T>,
		    .addFormatArgs = internal::logdata::AddLogArgs<LogFormatArgs, T>};
		return static_cast<T*>(WriteCustomType<true, true>(&kFunctionTable));
	}

	/// @brief Reserve space for a custom, but not trivially copyable type in the argument buffer.
	/// @tparam T The type of the argument.
	/// @return The address where the type MUST be constructed by the caller.
	template <typename T>
	[[nodiscard]] _Ret_notnull_ T* WriteNonTriviallyCopyable() {
		static_assert(std::is_same_v<T, std::decay_t<T>>, "unnecessary code duplication");
		static_assert(!std::is_trivially_copyable_v<T>, "type MUST NOT be trivially copyable");
		static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
		static_assert(sizeof(T) <= internal::logdata::kMaxCustomTypeSize, "custom type is too large");

		if constexpr (std::is_nothrow_copy_constructible_v<T>) {
			constexpr static FunctionTableNoThrowConstructible kFunctionTable{
			    {{.align = alignof(T),
			      .size = sizeof(T),
			      .addEventData = internal::logdata::AddLogArgs<LogEventArgs, T>,
			      .addFormatArgs = internal::logdata::AddLogArgs<LogFormatArgs, T>},
			     internal::logdata::Destruct<T>},
			    internal::logdata::Copy<T>,
			    internal::logdata::Move<T>,
			};
			return static_cast<T*>(WriteCustomType<false, true>(&kFunctionTable));
		} else {
			constexpr static FunctionTableNonTrivial kFunctionTable{
			    {.align = alignof(T),
			     .size = sizeof(T),
			     .addEventData = internal::logdata::AddLogArgs<LogEventArgs, T>,
			     .addFormatArgs = internal::logdata::AddLogArgs<LogFormatArgs, T>},
			    internal::logdata::Destruct<T>};
			return static_cast<T*>(WriteCustomType<false, false>(&kFunctionTable));
		}
	}

	/// @brief Get space for a custom object in the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by a pointer to the function table, padding as
	/// required and finally the bytes of the object.
	/// @tparam kTriviallyCopyable `true` if the type is trivially copyable.
	/// @tparam kNoThrowConstructible `true` if the type is no-throw constructible.
	/// @param pFunctionTable A pointer to the `FunctionTable` or a derived type.
	/// @return A pointer to the location where the custom type MUST be created.
	template <bool kTriviallyCopyable, bool kNoThrowConstructible>
	[[nodiscard]] _Ret_notnull_ void* WriteCustomType(_In_ const FunctionTable* __restrict pFunctionTable);

private:
	struct HeapBufferInfo {
		/// @brief The heap buffer used for larger payloads.
		/// @details Using std::shared_ptr to allow noexcept copy operation for use in exceptions.
		/// @copyright Same as `NanoLogLine::m_heap_buffer` from NanoLog.
		HeapBuffer heapBuffer;
		/// @brief The current capacity of the buffer in bytes.
		/// @copyright Same as `NanoLogLine::m_buffer_size` from NanoLog.
		Size size;
	};

	/// @brief The stack buffer used for small payloads.
	/// @remarks When `m_hashHeapBuffer` is `true`, an object of type `HeapBufferInfo` is present at this address.
	/// Not using a union because that would add unwanted padding.
	/// @copyright Same as `NanoLogLine::m_stack_buffer` from NanoLog.
	std::byte m_stackBuffer[M3C_LOGDATA_SIZE  // target size
	                        - sizeof(bool)    // m_hasNonTriviallyCopyable
	                        - sizeof(Size)    // m_used
	];
	static_assert(sizeof(m_stackBuffer) >= sizeof(HeapBufferInfo));

	/// @brief `true` if a heap buffer is present inside `m_stackBuffer`. @hideinitializer
	bool m_hasHeapBuffer : 1 = false;
	/// @brief `true` if at least one argument needs special handling on buffer operations. @hideinitializer
	bool m_hasNonTriviallyCopyable : 1 = false;

	/// @brief The number of bytes used in the buffer.
	/// @copyright Same as `NanoLogLine::m_bytes_used` from NanoLog. @hideinitializer
	Size m_used = 0;
};

}  // namespace internal


/// @brief The class contains additional data for logging exceptions.s
/// @details The class is similar to a `std::vector` of `std::variant` objects, but more memory efficient.
/// @remarks Modeled as a subclass of `internal::LogData` to hide access to methods `AddArgument` and `AddCustomArgument`.
/// @copyright The interface of this class is based on `class NanoLogLine` from NanoLog.
class LogData : private internal::LogDataBase {
public:
	using LogDataBase::LogDataBase;
	using LogDataBase::operator=;
	using LogDataBase::AddCustomArgument;
	using LogDataBase::CopyArgumentsTo;

	/// @brief Add a custom log argument, using custom operator >> if available.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	template <typename T>
	LogData& operator<<(T&& arg) {
		if constexpr (internal::LogCustomTo<T, LogData>) {
			std::forward<T>(arg) >> *this;
		} else if constexpr (internal::LogArgAddableTo<T, internal::LogDataBase>) {
			AddArgument(std::forward<T>(arg));
		} else {
			AddCustomArgument(std::forward<T>(arg));
		}
		return *this;
	}
};

}  // namespace m3c


/// @brief Log a `PropVariant` as `VT_xx: \<value\>`.
/// @param logData The `LogData`.
/// @param arg The value.
void operator>>(const VARIANT& arg, _Inout_ m3c::LogData& logData);

/// @brief Log a `PropVariant` as `VT_xx: \<value\>`.
/// @param logData The `LogData`.
/// @param arg The value.
void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogData& logData);
