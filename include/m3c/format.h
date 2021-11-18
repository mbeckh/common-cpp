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
#pragma once

#include <m3c/type_traits.h>

#include <fmt/format.h>  // IWYU pragma: export
#include <fmt/xchar.h>   // IWYU pragma: export

#include <windows.h>
#include <oaidl.h>
#include <objidl.h>
#include <propidl.h>
#include <rpc.h>
#include <unknwn.h>
#include <wincodec.h>
#include <wtypes.h>

#include <bit>
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#ifdef __clang_analyzer__
#include <utility>
#endif

namespace m3c {

#ifdef __clang_analyzer__

namespace internal {

template <typename... Args>
inline auto IwyuFormatWorkaround(const char* pattern, Args&&... args) {
	return fmt::format(fmt::runtime(pattern), std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline auto IwyuFormatWorkaround(T&& pattern, Args&&... args) {
	return fmt::format(std::forward<T>(pattern), std::forward<Args>(args)...);
}

}  // namespace internal

/// @brief Replace literal format strings for fmt compile with runtime strings.
/// @details clang-tidy and include-what-you-use have problems parsing and diagnose false positives or hang/crash.
#define FMT_FORMAT(...) m3c::internal::IwyuFormatWorkaround(__VA_ARGS__)
#else
/// @brief Replace literal format strings for fmt compile with runtime strings.
/// @details clang-tidy and include-what-you-use have problems parsing and diagnose false positives or hang/crash.
#define FMT_FORMAT(...) fmt::format(__VA_ARGS__)
#endif


//
// String encoding
//

/// @brief Helper class to encode a string based on string type an target.
/// @details No data is copied, i.e. the source string must remain valid until the formatting is completed.
/// @tparam CharT The character type of the source string.
template <typename CharT>
class fmt_encode {
public:
	/// @brief Create a new string wrapper for a native C-string.
	/// @param sz The native string.
	[[nodiscard]] constexpr explicit fmt_encode(_In_z_ const CharT* const sz) noexcept
	    : m_view(sz) {
	}

	/// @brief Create a new string wrapper for a STL string.
	/// @param str The STL string.
	template <typename... Args>
	[[nodiscard]] constexpr explicit fmt_encode(const std::basic_string<CharT, Args...>& str) noexcept
	    : m_view(str.c_str(), str.size()) {
	}

	/// @brief Create a new string wrapper for a STL string view.
	/// @param str The STL string view.
	template <typename... Args>
	[[nodiscard]] constexpr explicit fmt_encode(const std::basic_string_view<CharT, Args...>& str) noexcept
	    : m_view(str.data(), str.size()) {
	}

	fmt_encode(const fmt_encode&) = delete;
	fmt_encode(fmt_encode&&) = delete;

	constexpr ~fmt_encode() noexcept = default;

public:
	fmt_encode& operator=(const fmt_encode&) = delete;
	fmt_encode& operator=(fmt_encode&&) = delete;

public:
	/// @brief Get the string's contents.
	/// @return A reference to the internal string view.
	[[nodiscard]] constexpr const std::basic_string_view<CharT>& get() const noexcept {
		return m_view;
	}

private:
	const std::basic_string_view<CharT> m_view;  ///< @brief The source string data.
};

}  // namespace m3c


/// @brief Specialization of `fmt::formatter` for cases where no encoding is required.
/// @tparam CharT The character type of the string and the formatter.
template <typename CharT>
struct fmt::formatter<m3c::fmt_encode<CharT>, CharT> : fmt::formatter<std::basic_string_view<CharT>, CharT> {
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_encode<CharT>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(arg.get(), ctx);
	}
};


/// @brief Specialization of `fmt::formatter` for cases where encoding is required.
/// @tparam T The character type of the string.
/// @tparam CharT The character type of the formatter.
template <typename T, typename CharT>
requires requires {
	requires !std::same_as<CharT, T>;
}
struct fmt::formatter<m3c::fmt_encode<T>, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the string view.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped string view.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_encode<T>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = encode(arg.get());
		return __super::format(value, ctx);
	}

private:
	/// @brief Encode a string view to a string using @p CharT.
	[[nodiscard]] static std::basic_string<CharT> encode(const std::basic_string_view<T>& arg);
};


//
// GUID
//

/// @brief Specialization of `fmt::formatter` for a `GUID`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<GUID, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the `GUID`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A `GUID`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const GUID& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = to_string(arg);
		return __super::format(value, ctx);
	}

private:
	/// @brief Format the `GUID` as a string.
	[[nodiscard]] static std::basic_string<CharT> to_string(const GUID& arg);
};


//
// FILETIME, SYSTEMTIME
//

/// @brief Specialization of `fmt::formatter` for a `FILETIME`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<FILETIME, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the `FILETIME`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A `FILETIME`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const FILETIME& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = to_string(arg);
		return __super::format(value, ctx);
	}

private:
	/// @brief Format the `FILETIME` as a string.
	[[nodiscard]] static std::basic_string<CharT> to_string(const FILETIME& arg);
};


/// @brief Specialization of `fmt::formatter` for a `SYSTEMTIME`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<SYSTEMTIME, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the `SYSTEMTIME`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A `SYSTEMTIME`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const SYSTEMTIME& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = to_string(arg);
		return __super::format(value, ctx);
	}

private:
	/// @brief Format the `SYSTEMTIME` as a string.
	[[nodiscard]] static std::basic_string<CharT> to_string(const SYSTEMTIME& arg);

	// allow std::formatter<FILETIME> to forward calls to this class
	friend struct fmt::formatter<FILETIME, CharT>;
};


//
// SID
//

/// @brief Specialization of `fmt::formatter` for a `SID`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<SID, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the `SID`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A `SID`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const SID& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = to_string(arg);
		return __super::format(value, ctx);
	}

private:
	/// @brief Format the `SID` as a string.
	/// @param arg The `SID` to format.
	[[nodiscard]] static std::basic_string<CharT> to_string(const SID& arg);
};


//
// Formatting of errors
//

namespace m3c {

namespace internal {

/// @brief Common base class for wrappers of error types.
/// @tparam T The native type of the error code.
template <typename T>
class error {
public:
	[[nodiscard]] constexpr error() noexcept = default;
	[[nodiscard]] constexpr explicit error(const T code) noexcept
	    : m_code(code) {
	}
	[[nodiscard]] constexpr error(const error&) noexcept = default;
	[[nodiscard]] constexpr error(error&&) noexcept = default;
	constexpr ~error() noexcept = default;

	constexpr error& operator=(const error&) noexcept = default;
	constexpr error& operator=(error&&) noexcept = default;

	/// @brief Get the wrapped error code.
	/// @details Returns a reference to allow e.g. event log methods to access the memory location.
	/// @return A reference to the error code.
	[[nodiscard]] constexpr const T& code() const noexcept {
		return m_code;
	}

private:
	T m_code;  ///< @brief The wrapped error code.
};

}  // namespace internal


/// @brief Type-safe wrapper to distinguish Win32 error codes from regular DWORD variables.
class win32_error final : public internal::error<DWORD> {
	using error::error;
};
static_assert(sizeof(win32_error) == sizeof(DWORD));

[[nodiscard]] inline win32_error last_error() noexcept {
	return win32_error(GetLastError());
}


/// @brief Type-safe wrapper to distinguish HRESULT values from regular long variables.
class hresult final : public internal::error<HRESULT> {
	using error::error;
};
static_assert(sizeof(hresult) == sizeof(HRESULT));


/// @brief Type-safe wrapper to distinguish RPC_STATUS values from regular long variables.
struct rpc_status final : public internal::error<RPC_STATUS> {
	using error::error;
};
static_assert(sizeof(rpc_status) == sizeof(RPC_STATUS));

}  // namespace m3c


/// @brief Specialization of `fmt::formatter` for errors.
/// @tparam T The type of the error.
/// @tparam CharT The character type of the formatter.
template <m3c::AnyOf<m3c::win32_error, m3c::hresult, m3c::rpc_status> T, typename CharT>
struct fmt::formatter<T, CharT> : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Format the error.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg An error wrapper.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const T& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = to_string(arg);
		return __super::format(value, ctx);
	}

private:
	/// @brief Format the error as a string.
	[[nodiscard]] static std::basic_string<CharT> to_string(const T& arg);
};


//
// VARIANT, PROPVARIANT
//

namespace m3c::internal {

/// @brief Common base class for formatters of `VARIANT` and `PROPVARIANT` objects
/// @details The formatter supports default formatting and selecting either the type or the value of the object.
/// The are selected by prefixing the format pattern with `t;` and `v;` respectively.
/// If no custom pattern is used, a trailing semicolon can be omitted.
/// Both the type and the value are formatted as strings.
/// @tparam T The type of the object.
/// @tparam CharT The character type of the formatter.
template <AnyOf<VARIANT, PROPVARIANT> T, typename CharT>
struct BaseVariantFormatter : fmt::formatter<std::basic_string<CharT>, CharT> {
	/// @brief Parse the format string.
	/// @tparam ParseContext see `fmt::formatter::parse`.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		const auto* it = ctx.begin();
		const auto* const end = ctx.end();

		if (it != end && (*it == 't' || *it == 'v')) {
			const auto* const next = it + 1;
			if (next != end && (*next == '}' || *next == ';')) {
				m_presentation = static_cast<char>(*it);
				ctx.advance_to(next + (*next == '}' ? 0 : 1));
			}
		}

		it = __super::parse(ctx);
		if (it != end && *it != '}') {
			throw fmt::format_error("invalid format");
		}

		return it;
	}

protected:
	/// @brief Format the object as a string.
	[[nodiscard]] std::basic_string<CharT> to_string(const T& arg) const;

private:
	char m_presentation = '\0';  ///< @brief Select the data to print.
};

}  // namespace m3c::internal


/// @brief Specialization of `fmt::formatter` for a `VARIANT`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<VARIANT, CharT> : m3c::internal::BaseVariantFormatter<VARIANT, CharT> {
	/// @brief Format the `VARIANT`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg An `VARIANT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const VARIANT& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = __super::to_string(arg);
		return __super::format(value, ctx);
	}
};

/// @brief Specialization of `fmt::formatter` for a `PROPVARIANT`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<PROPVARIANT, CharT> : m3c::internal::BaseVariantFormatter<PROPVARIANT, CharT> {
	/// @brief Format the `PROPVARIANT`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg An `PROPVARIANT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const PROPVARIANT& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::basic_string<CharT> value = __super::to_string(arg);
		return __super::format(value, ctx);
	}
};


//
// Pointers
//

namespace m3c {

/// @brief Helper class to allow formatting of pointer types.
/// @tparam T The type of the object referenced by the pointer.
template <typename T>
class fmt_ptr {
public:
	/// @brief Create a new object by wrapping a pointer.
	/// @param p The pointer to be wrapped.
	[[nodiscard]] constexpr explicit fmt_ptr(T* p) noexcept
	    : m_p(p) {
	}
	[[nodiscard]] constexpr fmt_ptr(const fmt_ptr&) noexcept = default;
	[[nodiscard]] constexpr fmt_ptr(fmt_ptr&&) noexcept = default;
	constexpr ~fmt_ptr() noexcept = default;

public:
	constexpr fmt_ptr& operator=(const fmt_ptr&) noexcept = default;
	constexpr fmt_ptr& operator=(fmt_ptr&&) noexcept = default;

	/// @brief Access the wrapped type.
	/// @return The wrapped pointer.
	[[nodiscard]] constexpr T* get() const noexcept {
		return m_p;
	}

private:
	T* const m_p;  ///< @brief The wrapped pointer.
};

}  // namespace m3c


//
// IUnknown, IStream
//

/// @brief Specialization of `fmt::formatter` for a `IUnknown*`.
/// @details The formatter supports default formatting and selecting either the pointer or the ref count of the object.
/// The are selected by prefixing the format pattern with `p;` and `r;` respectively.
/// If no custom pattern is used, a trailing semicolon can be omitted.
/// The pointer is formatted as a pointer value and ref count as an integer.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<m3c::fmt_ptr<IUnknown>, CharT> {
	/// @brief Parse the format string.
	/// @tparam ParseContext see `fmt::formatter::parse`.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		const auto* it = ctx.begin();
		const auto* const end = ctx.end();

		if (it != end && (*it == 'p' || *it == 'r')) {
			const auto* const next = it + 1;
			if (next != end && (*next == '}' || *next == ';')) {
				if (*it == 'p') {
					m_formatter.template emplace<fmt::formatter<void*, CharT>>();
				} else if (*it == 'r') {
					m_formatter.template emplace<fmt::formatter<ULONG, CharT>>();
				} else {
					// default
				}
				ctx.advance_to(next + (*next == '}' ? 0 : 1));
			}
		}

		it = std::visit(
		    [&ctx](auto& formatter) constexpr { return formatter.parse(ctx); }, m_formatter);
		if (it != end && *it != '}') {
			throw format_error("invalid format");
		}

		return it;
	}

	/// @brief Format the `IUnknown` object.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped `IUnknown` object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_ptr<IUnknown>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		IUnknown* const ptr = arg.get();

		if (std::holds_alternative<fmt::formatter<void*, CharT>>(m_formatter)) {
			return std::get<fmt::formatter<void*, CharT>>(m_formatter).format(ptr, ctx);
		}

		// AddRef to get the ref count
		const ULONG ref = ptr ? (ptr->AddRef(), ptr->Release()) : 0;
		if (std::holds_alternative<fmt::formatter<std::basic_string<CharT>, CharT>>(m_formatter)) {
			std::basic_string<CharT> value = FMT_FORMAT(
			    m3c::SelectString<CharT>("(ptr={}, ref={})", L"(ptr={}, ref={})"),
			    fmt::ptr(ptr), ref);
			return std::get<fmt::formatter<std::basic_string<CharT>, CharT>>(m_formatter).format(value, ctx);
		}

		return std::get<fmt::formatter<ULONG, CharT>>(m_formatter).format(ref, ctx);
	}

protected:
	/// @brief The formatter which was selected based on the format string.
	std::variant<fmt::formatter<std::basic_string<CharT>, CharT>, fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>, fmt::formatter<void*, CharT>, fmt::formatter<ULONG, CharT>> m_formatter;
};


namespace m3c::internal {

/// @brief A helper class to keep the function out of the templated formatter class.
struct BaseIStreamFormatter {
protected:
	/// @brief Get the name of the stream in a fail-safe way.
	/// @param ptr The pointer to the `IStream` object.
	/// @return The name of the stream, a default or error string respectively.
	[[nodiscard]] static std::wstring GetName(IStream* ptr);
};

}  // namespace m3c::internal

/// @brief Specialization of `fmt::formatter` for a `IStream*`.
/// @details In addition to the selectors supported by `IUnknown`, the stream name can be printed by prefixing the
/// format pattern with `n;`. The name is formatted as a string.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<m3c::fmt_ptr<IStream>, CharT> : fmt::formatter<m3c::fmt_ptr<IUnknown>, CharT>
    , private m3c::internal::BaseIStreamFormatter {
	/// @brief Parse the format string.
	/// @tparam ParseContext see `fmt::formatter::parse`.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		const auto* it = ctx.begin();
		const auto* const end = ctx.end();

		if (it != end && *it == 'n') {
			const auto* const next = it + 1;
			if (next != end && (*next == '}' || *next == ';')) {
				m_formatter.template emplace<fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>>();
				ctx.advance_to(next + (*next == '}' ? 0 : 1));
			}
		}
		it = __super::parse(ctx);

		if (it != end && *it != '}') {
			throw format_error("invalid format");
		}

		return it;
	}

	/// @brief Format the `IStream` object.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped `IStream` object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_ptr<IStream>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		IStream* const ptr = arg.get();
		if (std::holds_alternative<fmt::formatter<std::basic_string<CharT>, CharT>>(m_formatter) || std::holds_alternative<fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>>(m_formatter)) {
			const std::wstring name = GetName(ptr);
			if (std::holds_alternative<fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>>(m_formatter)) {
				return std::get<fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>>(m_formatter).format(m3c::fmt_encode(name), ctx);
			}

			// AddRef to get the ref count
			const ULONG ref = ptr ? (ptr->AddRef(), ptr->Release()) : 0;
			std::basic_string<CharT> value = FMT_FORMAT(
			    m3c::SelectString<CharT>("({}, ptr={}, ref={})", L"({}, ptr={}, ref={})"),
			    m3c::fmt_encode(name), fmt::ptr(ptr), ref);
			return std::get<fmt::formatter<std::basic_string<CharT>, CharT>>(m_formatter).format(value, ctx);
		}

		return __super::format(m3c::fmt_ptr<IUnknown>(ptr), ctx);
	}

private:
	// allow access to m_formatter without "this->" required because of template inheritance
	using fmt::formatter<m3c::fmt_ptr<IUnknown>, CharT>::m_formatter;
};


/// @brief Specialization of `fmt::formatter` for classes derived from `IUnknown`.
/// @tparam T The type derived from `IUnknown` .
/// @tparam CharT The character type of the formatter.
template <std::derived_from<IUnknown> T, typename CharT>
struct fmt::formatter<m3c::fmt_ptr<T>, CharT> : public fmt::formatter<m3c::fmt_ptr<IUnknown>, CharT> {
	/// @brief Format the object.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_ptr<T>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(m3c::fmt_ptr(static_cast<IUnknown*>(arg.get())), ctx);
	}
};


/// @brief Specialization of `fmt::formatter` for classes derived from `IStream`.
/// @tparam T The type derived from `IStream`.
/// @tparam CharT The character type of the formatter.
template <std::derived_from<IStream> T, typename CharT>
struct fmt::formatter<m3c::fmt_ptr<T>, CharT> : public fmt::formatter<m3c::fmt_ptr<IStream>, CharT> {
	/// @brief Format the object.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::fmt_ptr<T>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(m3c::fmt_ptr(static_cast<IStream*>(arg.get())), ctx);
	}
};


//
// PROPERTYKEY
//

namespace m3c::internal {

/// @brief A helper class to keep the function out of the templated formatter class.
struct BasePropertyKeyFormatter {
protected:
	/// @brief Get the name of the `PROPERTYKEY` in a fail-safe way.
	/// @param arg The `PROPERTYKEY` object.
	/// @return The name of the `PROPERTYKEY` or its UUID as a fallback value.
	[[nodiscard]] static std::wstring GetName(const PROPERTYKEY& arg);
};

}  // namespace m3c::internal


/// @brief Specialization of `fmt::formatter` for a `PROPERTYKEY`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<PROPERTYKEY, CharT> : public fmt::formatter<m3c::fmt_encode<wchar_t>, CharT>
    , private m3c::internal::BasePropertyKeyFormatter {
	/// @brief Format the `PROPERTYKEY`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped `PROPERTYKEY`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const PROPERTYKEY& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		const std::wstring name = GetName(arg);
		return __super::format(m3c::fmt_encode(name), ctx);
	}
};


//
// WICRect
//

/// @brief Specialization of `fmt::formatter` for a `WICRect`.
/// @details The formatter supports default formatting and selecting either the left, top, width or height value.
/// The are selected by prefixing the format pattern with `x;`, `y;`, `w;` and `h;` respectively.
/// If no custom pattern is used, a trailing semicolon can be omitted. All values are formatted as integers.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<WICRect, CharT> : formatter<int, CharT> {
	/// @brief Parse the format string.
	/// @tparam ParseContext see `fmt::formatter::parse`.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		const auto* it = ctx.begin();
		const auto* const end = ctx.end();

		if (it != end && (*it == 'x' || *it == 'y' || *it == 'w' || *it == 'h')) {
			const auto* const next = it + 1;
			if (next != end && (*next == '}' || *next == ';')) {
				m_presentation = static_cast<char>(*it);
				ctx.advance_to(next + (*next == '}' ? 0 : 1));
			}
		}

		it = __super::parse(ctx);
		if (it != end && *it != '}') {
			throw fmt::format_error("invalid format");
		}

		return it;
	}

	/// @brief Format the `WICRect`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped `WICRect`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const WICRect& arg, FormatContext& ctx) const {
		auto out = ctx.out();
		if (!m_presentation) {
			*out = static_cast<CharT>('(');
			*out = static_cast<CharT>('@');
			*out = static_cast<CharT>('(');
			ctx.advance_to(out);
		}
		if (!m_presentation || m_presentation == 'x') {
			out = __super::format(arg.X, ctx);
		}
		if (!m_presentation) {
			*out = static_cast<CharT>(',');
			*out = static_cast<CharT>(' ');
			ctx.advance_to(out);
		}
		if (!m_presentation || m_presentation == 'y') {
			out = __super::format(arg.Y, ctx);
		}
		if (!m_presentation) {
			*out = static_cast<CharT>(')');
			*out = static_cast<CharT>(' ');
			*out = static_cast<CharT>('/');
			*out = static_cast<CharT>(' ');
			ctx.advance_to(out);
		}
		if (!m_presentation || m_presentation == 'w') {
			out = __super::format(arg.Width, ctx);
		}
		if (!m_presentation) {
			*out = static_cast<CharT>(' ');
			*out = static_cast<CharT>('x');
			*out = static_cast<CharT>(' ');
			ctx.advance_to(out);
		}
		if (!m_presentation || m_presentation == 'h') {
			out = __super::format(arg.Height, ctx);
		}
		if (!m_presentation) {
			*out = static_cast<CharT>(')');
		}
		return out;
	}

private:
	char m_presentation = '\0';  ///< @brief Select the data to print.
};


//
// FILE_ID_128
//

/// @brief Specialization of `fmt::formatter` for a `FILE_ID_128`.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<FILE_ID_128, CharT> : fmt::formatter<GUID, CharT> {
	/// @brief Format the `FILE_ID_128`.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A wrapped `FILE_ID_128`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const FILE_ID_128& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(std::bit_cast<GUID>(arg), ctx);
	}
};


//
// Duck typing for "stringish" classes.
//

namespace m3c::internal {

/// @brief A "stringish" type which implements a `c_str()` method returning a pointer to @p CharT.
/// @tparam T The actual type.
/// @tparam CharT The character type of the "stringish" class.
template <typename T, typename CharT>
concept ConvertibleToCStrOf = requires(T arg) {
	{ arg.c_str() } -> std::convertible_to<const CharT*>;
};

/// @brief A "stringish" type for either `char` or `wchar_t`.
/// @tparam T The actual type.
template <typename T>
concept ConvertibleToCStr = ConvertibleToCStrOf<T, char> || ConvertibleToCStrOf<T, wchar_t>;

/// @brief A type which is a valid unit for a string length, i.e. an integral value or a reference to an integral value.
/// @details Requires as a separate concept because of the or condition.
/// @tparam T The actual type.
template <typename T>
concept StringLength = std::integral<T> || std::integral<std::remove_reference_t<T>>;

/// @brief A helper class for handling "stringish" objects.
/// @tparam T The type of the object.
template <ConvertibleToCStr T>
struct ConvertibleToCStrTraits {
	/// @brief The character type of the "stringish" object.
	using CharT = std::remove_cvref_t<std::remove_pointer_t<decltype(std::declval<T>().c_str())>>;

	/// @brief Calculate the length for objects having a `length()` method.
	/// @param arg The object.
	/// @return The number of characters excluding the terminating null character.
	static constexpr auto length(const T& arg) noexcept(noexcept(arg.length())) requires requires {
		{ arg.length() } -> StringLength;
	}
	{
		return arg.length();
	}

	/// @brief Calculate the length for objects having no `length()` but a `size()` method.
	/// @param arg The object.
	/// @return The number of characters excluding the terminating null character.
	static constexpr auto length(const T& arg) noexcept(noexcept(arg.size())) requires requires {
		{ arg.size() } -> StringLength;
		requires !requires {
			{ arg.length() } -> StringLength;
		};
	}
	{
		return arg.size();
	}

	/// @brief Calculate the length for objects having neither a `length()` nor a `size()` method.
	/// @details The string length is calculated using `std::char_traits`.
	/// @param arg The object.
	/// @return The number of characters excluding the terminating null character.
	static constexpr auto length(const T& arg) noexcept(noexcept(std::char_traits<CharT>::length(arg))) requires requires {
		requires !requires {
			{ arg.length() } -> StringLength;
		};
		requires !requires {
			{ arg.size() } -> StringLength;
		};
	}
	{
		return std::char_traits<CharT>::length(arg);
	}
};

}  // namespace m3c::internal


/// @brief Specialization of `fmt::formatter` for duck-typed C-strings (except STL strings).
/// @remarks Encoding is applied as required.
/// @tparam T The type of the object.
/// @tparam CharT The character type of the formatter.
template <m3c::internal::ConvertibleToCStr T, typename CharT>
requires requires {
	requires !m3c::SpecializationOf<T, std::basic_string>;
}
struct fmt::formatter<T, CharT> : fmt::formatter<m3c::fmt_encode<typename m3c::internal::ConvertibleToCStrTraits<T>::CharT>, CharT> {
	/// @brief Format the string.
	/// @tparam FormatContext see `fmt::formatter::format`.
	/// @param arg A "stringish" object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const T& arg, FormatContext& ctx) const {
		return __super::format(m3c::fmt_encode(std::basic_string_view(arg.c_str(), m3c::internal::ConvertibleToCStrTraits<T>::length(arg))), ctx);
	}
};
