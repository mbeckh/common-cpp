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
#pragma once

#include <m3c/com_ptr.h>

#include <fmt/core.h>

#include <propidl.h>
#include <rpc.h>
#include <wtypes.h>

#include <string>

struct IUnknown;
struct IStream;
struct WICRect;

namespace m3c {

class PropVariant;

/// @brief Helper class to allow formatting of pointer types.
/// @tparam T The type of the pointer.
template <typename T>
class fmt_ptr {
public:
	constexpr fmt_ptr(T* p)
		: m_p(p) {
	}
	fmt_ptr(const fmt_ptr&) noexcept = default;
	fmt_ptr(fmt_ptr&&) noexcept = default;
	~fmt_ptr() noexcept = default;

public:
	fmt_ptr& operator=(const fmt_ptr&) noexcept = default;
	fmt_ptr& operator=(fmt_ptr&&) noexcept = default;

	operator T*() {
		return m_p;
	}

	operator T*() const {
		return m_p;
	}

private:
	T* const m_p;
};

}  // namespace m3c


//
// UUID
//

/// @brief Specialization of `fmt::formatter` for a `UUID`.
template <>
struct fmt::formatter<UUID> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `UUID`.
	/// @param arg A `UUID`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const UUID& arg, fmt::format_context& ctx);
};


//
// PROPVARIANT
//

/// @brief Specialization of `fmt::formatter` for a `PROPVARIANT`.
template <>
struct fmt::formatter<PROPVARIANT> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `PROPVARIANT` (or `PropVariant`).
	/// @param arg A `PROPVARIANT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const PROPVARIANT& arg, fmt::format_context& ctx);
};

/// @brief Specialization of `fmt::formatter` for a `m3c::PropVariant`.
template <>
struct fmt::formatter<m3c::PropVariant> : public fmt::formatter<PROPVARIANT> {
	// empty
};


//
// IUnknown, IStream
//

/// @brief Specialization of `fmt::formatter` for a `IUnknown*`.
template <>
struct fmt::formatter<m3c::fmt_ptr<IUnknown>> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `IUnknown*`.
	/// @param arg A pointer to a COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const m3c::fmt_ptr<IUnknown>& arg, fmt::format_context& ctx);
};


/// @brief Specialization of `fmt::formatter` for a `IStream*`.
template <>
struct fmt::formatter<m3c::fmt_ptr<IStream>> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `IStream*`.
	/// @param arg A pointer to a COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const m3c::fmt_ptr<IStream>& arg, fmt::format_context& ctx);
};

/// @brief Specialization of `fmt::formatter` for a `m3c::com_ptr<IUnknown>`.
template <>
struct fmt::formatter<m3c::com_ptr<IUnknown>> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `m3c::com_ptr<IUnknown>`.
	/// @param arg A smart pointer to a COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const m3c::com_ptr<IUnknown>& arg, fmt::format_context& ctx);
};

/// @brief Specialization of `fmt::formatter` for a `m3c::com_ptr<IStream>`.
template <>
struct fmt::formatter<m3c::com_ptr<IStream>> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `m3c::com_ptr<IStream>`.
	/// @param arg A smart pointer to a COM object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const m3c::com_ptr<IStream>& arg, fmt::format_context& ctx);
};


//
// PROPERTYKEY
//

/// @brief Specialization of `fmt::formatter` for a `PROPERTYKEY`.
template <>
struct fmt::formatter<PROPERTYKEY> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) noexcept;

	/// @brief Format the `PROPERTYKEY`.
	/// @param arg An object.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const PROPERTYKEY& arg, fmt::format_context& ctx);
};


//
// WICRect
//

/// @brief Specialization of `fmt::formatter` for a `WICRect`.
template <>
struct fmt::formatter<WICRect> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx);

	/// @brief Format the `WICRect`.
	/// @param arg A rectangle.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const WICRect& arg, fmt::format_context& ctx);

private:
	std::string m_format;  ///< @brief The format pattern for all four values.
};
