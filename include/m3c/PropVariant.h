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

#include <m3c/format.h>

#include <oaidl.h>
#include <propidl.h>
#include <wtypes.h>

#include <string>

namespace m3c {

/// @brief Returns a string representation of the type of a `VARIANT` or `PROPVARIANT`.
/// @param vt A `VARTYPE` from a `Variant` or `PropVariant`.
/// @return The type as a string.
[[nodiscard]] std::string VariantTypeToString(VARTYPE vt);


/// @brief A class to manage stack unwinding for `VARIANT` objects.
class Variant final : public VARIANT {
public:
	/// @brief Ensures that `VariantInit` is called for an empty instance.
	[[nodiscard]] Variant() noexcept;

	/// @brief Creates a managed copy of a `VARIANT`.
	/// @param var A `VARIANT` object.
	[[nodiscard]] explicit Variant(const VARIANT& var);

	/// @brief Take ownership of a `VARIANT`.
	/// @param var A `VARIANT` object.
	[[nodiscard]] explicit Variant(VARIANT&& var) noexcept;

	/// @brief Copy constructor.
	/// @param oth A `Variant` object.
	[[nodiscard]] Variant(const Variant& oth);

	/// @brief Move constructor.
	/// @param oth A `Variant` object.
	[[nodiscard]] Variant(Variant&& oth) noexcept;

	/// @brief Internally calls `VariantClear`.
	~Variant() noexcept;

public:
	Variant& operator=(const Variant&) = delete;
	Variant& operator=(Variant&&) = delete;

public:
	/// @brief Get the type as a string.
	/// @return The name of the variant type.
	[[nodiscard]] std::string GetVariantType() const {
		return VariantTypeToString(vt);
	}
};


/// @brief A class to manage stack unwinding for `PROPVARIANT` objects.
class PropVariant final : public PROPVARIANT {
public:
	/// @brief Ensures that `PropVariantInit` is called for an empty instance.
	[[nodiscard]] PropVariant() noexcept;

	/// @brief Creates a managed copy of a `PROPVARIANT`.
	/// @param pv A `PROPVARIANT` object.
	[[nodiscard]] explicit PropVariant(const PROPVARIANT& pv);

	/// @brief Take ownership of a `PROPVARIANT`.
	/// @param pv A `PROPVARIANT` object.
	[[nodiscard]] explicit PropVariant(PROPVARIANT&& pv) noexcept;

	/// @brief Copy constructor.
	/// @param oth A `PropVariant` object.
	[[nodiscard]] PropVariant(const PropVariant& oth);

	/// @brief Move constructor.
	/// @param oth A `PropVariant` object.
	[[nodiscard]] PropVariant(PropVariant&& oth) noexcept;

	/// @brief Internally calls `PropVariantClear`.
	~PropVariant() noexcept;

public:
	PropVariant& operator=(const PropVariant&) = delete;
	PropVariant& operator=(PropVariant&&) = delete;

public:
	/// @brief Get the type as a string.
	/// @return The name of the variant type.
	[[nodiscard]] std::string GetVariantType() const {
		return VariantTypeToString(vt);
	}
};

}  // namespace m3c


/// @brief A formatter for `m3c::Variant` objects.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<m3c::Variant, CharT> : fmt::formatter<VARIANT, CharT> {
	// empty
};

/// @brief A formatter for `m3c::PropVariant` objects.
/// @tparam CharT The character type of the formatter.
template <typename CharT>
struct fmt::formatter<m3c::PropVariant, CharT> : fmt::formatter<PROPVARIANT, CharT> {
	// empty
};
