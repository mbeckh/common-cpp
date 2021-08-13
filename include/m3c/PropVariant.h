/*
Copyright 2019-2020 Michael Beckh

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

#include <propidl.h>
#include <wtypes.h>

#include <string>

namespace llamalog {
class LogLine;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
}  // namespace llamalog

namespace m3c {

/// @brief Returns a string representation of the type of a `VARIANT` or `PROPVARIANT`.
/// @param pv A `VARTYPE` from a `Variant` or `PropVariant`.
/// @return The type as a string.
[[nodiscard]] std::string VariantTypeToString(VARTYPE vt);  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

/// @brief A class to manage stack unwinding for `VARIANT` objects.
class Variant final : public VARIANT {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
public:
	/// @brief Ensures that `VariantInit` is called for an empty instance.
	Variant() noexcept;

	Variant(const Variant& oth);
	Variant(Variant&& oth) noexcept;

	explicit Variant(const VARIANT& pv);
	explicit Variant(VARIANT&& pv) noexcept;

	/// @brief Internally calls `VariantClear`.
	~Variant() noexcept;

public:
	Variant& operator=(const Variant&) = delete;
	Variant& operator=(Variant&&) = delete;

public:
	[[nodiscard]] std::string GetVariantType() const {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		return VariantTypeToString(vt);
	}
};

/// @brief A class to manage stack unwinding for `PROPVARIANT` objects.
class PropVariant final : public PROPVARIANT {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
public:
	/// @brief Ensures that `PropVariantInit` is called for an empty instance.
	PropVariant() noexcept;

	PropVariant(const PropVariant& oth);
	PropVariant(PropVariant&& oth) noexcept;

	explicit PropVariant(const PROPVARIANT& pv);
	explicit PropVariant(PROPVARIANT&& pv) noexcept;

	/// @brief Internally calls `PropVariantClear`.
	~PropVariant() noexcept;

public:
	PropVariant& operator=(const PropVariant&) = delete;
	PropVariant& operator=(PropVariant&&) = delete;

public:
	[[nodiscard]] std::string GetVariantType() const {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		return VariantTypeToString(vt);
	}
};

#if 0
/// @brief Log a `m3c::PropVariant` as `VT_xx: \<value\>`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const PropVariant& arg);

#endif
}  // namespace m3c
