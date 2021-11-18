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
#pragma once

#include <m3c/format.h>  // IWYU pragma: export
#include <m3c/string_encode.h>
#include <m3c/type_traits.h>

#include <fmt/args.h>

#include <windows.h>
#include <evntprov.h>
#include <oaidl.h>
#include <propidl.h>
#include <wincodec.h>
#include <wtypes.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace m3c {

namespace internal {

/// @brief Helper concept to detect types supported by the logging API natively.
/// @tparam T The type.
template <typename T>
inline constexpr bool is_trivial_loggable_v = std::is_fundamental_v<T> || std::is_enum_v<T> || AnyOf<T, GUID, FILETIME, SYSTEMTIME, win32_error, hresult, rpc_status>;  // NOLINT(readability-identifier-naming): Follows naming of std lib

/// @brief A type natively supported by Windows event logging, except strings.
/// @tparam T The type.
template <typename T>
concept TriviallyLoggable = is_trivial_loggable_v<std::decay_t<T>>;

/// @brief Internal concept to check for the presence of a method in a constexpr if statement.
/// @tparam T The type of the argument.
/// @tparam Arguments The type of the arguments container.
template <typename T, typename Arguments>
concept LogArgAddableTo = requires(Arguments& args, T&& arg) {
	args.AddArgument(std::forward<T>(arg));
};

/// @brief Internal concept to check for the presence of a method in a constexpr if statement.
/// @tparam T The type of the argument.
/// @tparam Arguments The type of the arguments container.
template <typename T, typename Arguments>
concept LogCustomTo = requires(T&& arg, Arguments& args) {
	std::forward<T>(arg) >> args;
};

/// @brief Base class for a container for arguments to `std::format`.
class LogFormatArgsBase {
public:
	[[nodiscard]] LogFormatArgsBase() noexcept = default;
	LogFormatArgsBase(const LogFormatArgsBase&) = delete;
	LogFormatArgsBase(LogFormatArgsBase&&) = delete;
	~LogFormatArgsBase() noexcept = default;

public:
	LogFormatArgsBase& operator=(const LogFormatArgsBase&) = delete;
	LogFormatArgsBase& operator=(LogFormatArgsBase&&) = delete;

	/// @brief Add an argument for logging.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <typename T>
	requires internal::TriviallyLoggable<T> || requires {
		requires SpecializationOf<T, std::basic_string> || SpecializationOf<T, std::basic_string_view>;
		requires std::same_as<typename T::value_type, char>;
	}
	void AddArgument(const T& arg) {
		m_args.push_back(arg);
		++m_size;
	}

	/// @brief Add a native C-string argument for logging.
	/// @param arg The log argument.
	void AddArgument(_In_z_ const char* __restrict const arg) {
		m_args.push_back(arg);
		++m_size;
	}

	/// @brief Add a "stringish" argument for logging.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <internal::ConvertibleToCStrOf<char> T>
	requires requires {
		requires(!SpecializationOf<T, std::basic_string>);
	}
	void AddArgument(const T& arg) {
		m_args.push_back(std::string_view(arg.c_str(), internal::ConvertibleToCStrTraits<T>::length(arg)));
		++m_size;
	}

	/// @brief Add a native wide-character C-string argument for logging.
	/// @param arg The log argument.
	void AddArgument(_In_z_ const wchar_t* __restrict const arg) {
		m_args.push_back(EncodeUtf8(arg));
		++m_size;
	}

	/// @brief Add a wide-character "stringish" argument for logging.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <internal::ConvertibleToCStrOf<wchar_t> T>
	void AddArgument(const T& arg) {
		m_args.push_back(EncodeUtf8(arg.c_str(), internal::ConvertibleToCStrTraits<T>::length(arg)));
		++m_size;
	}

	/// @brief Add a wide-character `std::basic_string_view` argument for logging.
	/// @tparam Args Template arguments of `std::basic_string_view` after the character type.
	/// @param arg The log argument.
	template <typename... Args>
	void AddArgument(const std::basic_string_view<wchar_t, Args...>& arg) {
		m_args.push_back(EncodeUtf8(arg.data(), arg.size()));
		++m_size;
	}

	/// @brief Add a native pointer argument for logging.
	/// @param arg The log argument.
	void AddArgument(_In_opt_ const void* __restrict const arg) {
		m_args.push_back(arg);
		++m_size;
	}

	/// @brief Add a wrapped pointer argument for logging.
	/// @tparam T The target type of the wrapped pointer.
	/// @param arg The log argument.
	template <typename T>
	void AddArgument(const fmt_ptr<T>& arg) {
		m_args.push_back(arg);
		++m_size;
	}

	/// @brief Add an arbitrary type as a reference to prevent slicing.
	/// @remarks Not named `AddArgument` to bind after `AddArgument` methods.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <typename T>
	void AddArgumentFallback(const T& arg) {
		// prevent slicing for any other types
		m_args.push_back(std::cref(arg));
		++m_size;
	}

	/// @brief Get the actual formatter arguments.
	/// @return A reference to the formatter arguments.
	[[nodiscard]] constexpr const fmt::dynamic_format_arg_store<fmt::format_context>& operator*() const noexcept {
		return m_args;
	}

	/// @brief Get the number of formatter arguments.
	/// @return The number of formatter arguments.
	[[nodiscard]] constexpr std::size_t size() const noexcept {
		return m_size;
	}

private:
	fmt::dynamic_format_arg_store<fmt::format_context> m_args;  ///< @brief The formatter arguments.
	std::size_t m_size = 0;                                     ///< @brief The number of formatter arguments.
};

class LogEventArgsBase {
public:
	[[nodiscard]] LogEventArgsBase() noexcept = default;
	LogEventArgsBase(const LogEventArgsBase&) = delete;
	LogEventArgsBase(LogEventArgsBase&&) = delete;
	~LogEventArgsBase() noexcept = default;

public:
	LogEventArgsBase& operator=(const LogEventArgsBase&) = delete;
	LogEventArgsBase& operator=(LogEventArgsBase&&) = delete;

public:
	/// @brief Add an argument for logging an event message.
	/// @remarks The native argument must remain valid until the end of the logging call.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <internal::TriviallyLoggable T>
	constexpr void AddArgument(const T& arg) {
		EventDataDescCreate(&m_args.emplace_back(), std::addressof(arg), sizeof(arg));
	}

	/// @brief Add a native string argument for logging an event message.
	/// @remarks The native string argument must remain valid until the end of the logging call.
	/// @tparam T The character type of the log argument.
	/// @param arg The log argument.
	template <AnyOf<char, wchar_t> T>
	constexpr void AddArgument(_In_z_ const T* __restrict const arg) {
		EventDataDescCreate(&m_args.emplace_back(), arg, static_cast<ULONG>((std::char_traits<T>::length(arg) + 1) * sizeof(T)));
	}

	/// @brief Add a "stringish" argument for logging an event message.
	/// @remarks The "stringish" argument must remain valid until the end of the logging call.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <internal::ConvertibleToCStr T>
	constexpr void AddArgument(const T& arg) {
		EventDataDescCreate(&m_args.emplace_back(), arg.c_str(), static_cast<ULONG>((internal::ConvertibleToCStrTraits<T>::length(arg) + 1) * sizeof(typename internal::ConvertibleToCStrTraits<T>::CharT)));
	}

	/// @brief Add a `std::basic_string_view` argument for logging an event message.
	/// @remarks The value is copied to ensure a trailing null character at the end of the string.
	/// @tparam Args Template arguments of `std::basic_string_view` after the character type.
	/// @param arg The log argument.
	template <typename... Args>
	void AddArgument(const std::basic_string_view<Args...>& arg) {
		// forward to store operator
		StoreArgument(arg);
	}

	/// @brief Add a native pointer argument for logging an event message.
	/// @param arg The log argument.
	void AddArgument(_In_opt_ const void* __restrict const& arg) {
		EventDataDescCreate(&m_args.emplace_back(), std::addressof(reinterpret_cast<const void* const&>(arg)), sizeof(arg));  // NOLINT(cppcoreguidelines-pro-type-const-cast): else strange error in clang
	}

	/// @brief Add a `SID` argument for logging an event message.
	/// @remarks The `SID` must remain valid until the end of the logging call.
	/// @param arg The log argument.
	void AddArgument(const SID& arg) {
		EventDataDescCreate(&m_args.emplace_back(), std::addressof(arg), SECURITY_SID_SIZE(arg.SubAuthorityCount));
	}

	/// @brief Add a raw data log argument.
	/// @details If character data is provided, it MUST be terminated with a null character.
	/// @remarks The native data must remain valid until the end of the logging call.
	/// @tparam T The type of the native buffer
	/// @tparam kExtent The size of the native buffer in number of elements.
	/// @param arg The log argument.
	template <typename T, std::size_t kExtent>
	constexpr void AddArgument(const std::span<T, kExtent>& arg) {
		if constexpr (is_any_of_v<typename std::span<T, kExtent>::value_type, char, wchar_t>) {
			assert(!arg.empty() && arg.back() == static_cast<T>(0));
		}
		EventDataDescCreate(&m_args.emplace_back(), arg.data(), static_cast<ULONG>(arg.size_bytes()));
	}

	/// @brief Add a log argument and copy the data to an internal store.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <typename T>
	requires std::is_trivially_copyable_v<T>
	void StoreArgument(const T& arg) {
		constexpr std::size_t kSize = sizeof(T);
		std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(kSize);
		std::memcpy(buffer.get(), std::addressof(arg), kSize);
		EventDataDescCreate(&m_args.emplace_back(), buffer.get(), static_cast<ULONG>(kSize));
		m_backingStore.push_back(std::move(buffer));
	}

	/// @brief Add a native string log argument and copy the data to an internal store.
	/// @tparam T The character type of the log argument.
	/// @param arg The log argument.
	template <AnyOf<char, wchar_t> T>
	void StoreArgument(_In_z_ const T* __restrict const arg) {
		const std::size_t size = (std::char_traits<T>::length(arg) + 1) * sizeof(T);
		std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(size);
		std::memcpy(buffer.get(), arg, size);
		EventDataDescCreate(&m_args.emplace_back(), buffer.get(), static_cast<ULONG>(size));
		m_backingStore.push_back(std::move(buffer));
	}

	/// @brief Add a "stringish" log argument and copy the data to an internal store.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	template <internal::ConvertibleToCStr T>
	void StoreArgument(const T& arg) {
		const std::size_t size = (internal::ConvertibleToCStrTraits<T>::length(arg) + 1) * sizeof(typename internal::ConvertibleToCStrTraits<T>::CharT);
		std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(size);
		std::memcpy(buffer.get(), arg.c_str(), size);
		EventDataDescCreate(&m_args.emplace_back(), buffer.get(), static_cast<ULONG>(size));
		m_backingStore.push_back(std::move(buffer));
	}

	/// @brief Add a `std::basic_string_view` log argument and copy the data to an internal store.
	/// @tparam Args Template arguments of `std::basic_string_view` after the character type.
	/// @param arg The log argument.
	template <typename... Args>
	void StoreArgument(const std::basic_string_view<Args...>& arg) {
		using CharT = typename std::basic_string_view<Args...>::value_type;

		const std::size_t lengthInBytes = arg.size() * sizeof(CharT);
		std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(lengthInBytes + sizeof(CharT));
		std::memcpy(buffer.get(), arg.data(), lengthInBytes);
		std::memset(buffer.get() + lengthInBytes, 0, sizeof(CharT));
		EventDataDescCreate(&m_args.emplace_back(), buffer.get(), static_cast<ULONG>(lengthInBytes + sizeof(CharT)));
		m_backingStore.push_back(std::move(buffer));
	}

	/// @brief Add a raw data log argument and copy the data to an internal store.
	/// @tparam T The type of the native buffer
	/// @tparam kExtent The size of the native buffer in number of elements.
	/// @param arg The log argument.
	template <typename T, size_t kExtent>
	void StoreArgument(const std::span<T, kExtent>& arg) {
		const std::size_t size = arg.size_bytes();
		std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(size);
		std::memcpy(buffer.get(), arg.data(), size);
		EventDataDescCreate(&m_args.emplace_back(), buffer.get(), static_cast<ULONG>(size));
		m_backingStore.push_back(std::move(buffer));
	}

	/// @brief Get the actual event arguments.
	/// @return A reference to the event arguments.
	[[nodiscard]] constexpr __declspec(restrict) EVENT_DATA_DESCRIPTOR* data() noexcept {
		return m_args.data();
	}

	/// @brief Get a single event argument.
	/// @param index The index of the argument.
	/// @return A reference to the event argument.
	constexpr const EVENT_DATA_DESCRIPTOR& operator[](const std::size_t index) const noexcept {
		return m_args[index];
	}

	/// @brief Get the number of event arguments.
	/// @return The number of event arguments.
	[[nodiscard]] constexpr ULONG size() const noexcept {
		return static_cast<ULONG>(m_args.size());
	}

private:
	std::vector<EVENT_DATA_DESCRIPTOR> m_args;                 ///< @brief The event arguments.
	std::vector<std::unique_ptr<std::byte[]>> m_backingStore;  ///< @brief The backing store for event arguments.
};

}  // namespace internal


/// @brief A container for arguments to `std::format`.
/// @remarks Modeled as a subclass of `internal::LogFormatArgsBase` to hide access to `AddArgument` methods.
class LogFormatArgs : private internal::LogFormatArgsBase {
public:
	using LogFormatArgsBase::LogFormatArgsBase;
	using LogFormatArgsBase::operator*;
	using LogFormatArgsBase::size;

	/// @brief Add an argument of any other type for logging a string message.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	/// @return The current object for method chaining.
	template <typename T>
	LogFormatArgs& operator%(T&& arg) {
		if constexpr (internal::LogArgAddableTo<T, internal::LogFormatArgsBase>) {
			AddArgument(arg);
		} else if constexpr (!std::is_rvalue_reference_v<T&&>) {
			AddArgumentFallback(arg);
		} else {
			// prevent storing references to temporary objects
			static_assert_no_clang(false, "MUST NOT use rvalue to add to LogFormatArgs");
		}
		return *this;
	}

	/// @brief Add an argument of any other type for logging an event message.
	/// @remarks If defined for a type, the custom `operator>>`  is used.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	/// @return The current object for method chaining.
	template <typename T>
	LogFormatArgs& operator<<(T&& arg) {
		if constexpr (internal::LogCustomTo<T, LogFormatArgs>) {
			std::forward<T>(arg) >> *this;
		} else if constexpr (internal::LogArgAddableTo<T, internal::LogFormatArgsBase>) {
			AddArgument(std::forward<T>(arg));
		} else if constexpr (!std::is_rvalue_reference_v<T&&>) {
			AddArgumentFallback(arg);
		} else {
			// prevent storing references to temporary objects
			static_assert_no_clang(false, "MUST NOT use rvalue to add to LogFormatArgs");
		}
		return *this;
	}
};

/// @brief A container for arguments to `EventWrite`.
/// @remarks Modeled as a subclass of `internal::LogEventArgsBase` to hide access to methods `AddArgument` and `StoreArgument`.
class LogEventArgs : private internal::LogEventArgsBase {
public:
	using LogEventArgsBase::data;
	using LogEventArgsBase::LogEventArgsBase;
	using LogEventArgsBase::operator[];
	using LogEventArgsBase::size;

	/// @brief Add an argument of any other type for logging an event message.
	/// @remarks The method is only selected if no custom operator >> is defined for the type.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	/// @return The current object for method chaining.
	template <typename T>
	LogEventArgs& operator<<(T&& arg) {
		if constexpr (internal::LogCustomTo<T, LogEventArgs>) {
			std::forward<T>(arg) >> *this;
		} else if constexpr (internal::LogArgAddableTo<T, internal::LogEventArgsBase>) {
			AddArgument(std::forward<T>(arg));
		} else {
			if constexpr (std::is_rvalue_reference_v<T&&>) {
				static_assert_no_clang(false, "MUST NOT use rvalue to add to LogEventArgs");
			}
			// prevent storing references to temporary objects
			static_assert_no_clang(false, "MUST use custom operator>> to add to LogEventArgs");
		}
		return *this;
	}

	/// @brief Add a log argument and copy the data to an internal store.
	/// @tparam T The type of the log argument.
	/// @param arg The log argument.
	/// @return The current object for method chaining.
	template <typename T>
	LogEventArgs& operator+(const T& arg) {
		// forward to the internal function
		StoreArgument(arg);

		return *this;
	}
};


/// @brief Any type of log argument container.
/// @tparam T The type of the log argument container.
template <typename T>
concept LogArgs = AnyOf<T, LogFormatArgs, LogEventArgs>;

}  // namespace m3c


/// @brief Add a `VARIANT` to a `m3c::LogFormatArgs` structure.
/// @param arg The log argument.
/// @param formatArgs The formatter arguments.
void operator>>(const VARIANT& arg, _Inout_ m3c::LogFormatArgs& formatArgs);


/// @brief Add a `VARIANT` to a `m3c::LogEventArgs` structure.
/// @param arg The log argument.
/// @param eventArgs The event arguments.
void operator>>(const VARIANT& arg, _Inout_ m3c::LogEventArgs& eventArgs);


/// @brief Add a `PROPVARIANT` to a `m3c::LogFormatArgs` structure.
/// @param arg The log argument.
/// @param formatArgs The formatter arguments.
void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogFormatArgs& formatArgs);

/// @brief Add a `PROPVARIANT` to a `m3c::LogEventArgs` structure.
/// @param arg The log argument.
/// @param eventArgs The event arguments.
void operator>>(const PROPVARIANT& arg, _Inout_ m3c::LogEventArgs& eventArgs);


/// @brief Add a `PROPERTYKEY` to a `m3c::LogEventArgs` structure.
/// @param arg The log argument.
/// @param eventArgs The event arguments.
void operator>>(const PROPERTYKEY& arg, _Inout_ m3c::LogEventArgs& eventArgs);


/// @brief Add a `WICRect` to a `m3c::LogFormatArgs` structure.
/// @param arg The log argument.
/// @param formatArgs The formatter arguments.
void operator>>(const WICRect& arg, _Inout_ m3c::LogFormatArgs& formatArgs);

/// @brief Add a `WICRect` to a `_Inout_ m3c::LogEventArgs` structure.
/// @param arg The log argument.
/// @param eventArgs The event arguments.
void operator>>(const WICRect& arg, _Inout_ m3c::LogEventArgs& eventArgs);
