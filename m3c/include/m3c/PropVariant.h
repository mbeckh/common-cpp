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

/// @file
#pragma once

#include <propidl.h>

#include <string>

namespace llamalog {
class LogLine;
}  // namespace llamalog

namespace m3c {

/// @brief Returns a string represenation of the type of a `PROPVARIANT`.
/// @param pv A `PROPVARIANT`.
/// @return The type as a string.
std::string VariantTypeToString(const PROPVARIANT& pv);

/// @brief A class to manage stack unwinding for `PROPVARIANT` objects.
class PropVariant final : public PROPVARIANT {
public:
	/// @brief Ensures that `PropVariantInit` is called for an empty instance.
	PropVariant() noexcept;

	PropVariant(const PropVariant& pv);
	PropVariant(PropVariant&& pv) noexcept;

	explicit PropVariant(const PROPVARIANT& pv);
	explicit PropVariant(PROPVARIANT&& pv) noexcept;

	/// @brief Internally calls PropVariantClear.
	~PropVariant() noexcept;

public:
	PropVariant& operator=(const PropVariant&) = delete;
	PropVariant& operator=(PropVariant&&) = delete;

public:
	std::string GetVariantType() const {
		return VariantTypeToString(*this);
	}
};

}  // namespace m3c

/// @brief Log a `m3c::PropVariant` as `VT_xx|<value>`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const m3c::PropVariant& arg);
