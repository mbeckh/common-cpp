/*
source_location standard header(core)

Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/// @file
/// @details "Polyfill" for clang which does not support std::source_location as of clang 14.0.0 [09287214]
/// @copyright Adapted from https://github.com/microsoft/STL/blob/main/stl/inc/source_location, Copyright (c) Microsoft Corporation.
#pragma once

#include <source_location>  // IWYU pragma: export

#ifdef __cpp_lib_source_location

#ifdef __clang_analyzer__
#error Fallback for source_location is no longer required.
#endif

#else

#ifndef __clang_analyzer__
#pragma message("Using fallback for <source_location>")
#endif


#include <cstdint>

namespace std {  // NOLINT(cert-dcl58-cpp): Whole point is a hack to make up for clang not fully supporting C++20.

struct source_location {
	// NOLINTBEGIN(bugprone-easily-swappable-parameters): Interface from std.
	[[nodiscard]] static constexpr source_location current(const std::uint_least32_t line = __builtin_LINE(),
	                                                       const std::uint_least32_t column = __builtin_COLUMN(),
	                                                       const char* const file = __builtin_FILE(),
	                                                       const char* const function = __builtin_FUNCTION()) noexcept {
		source_location result;
		result.m_line = line;
		result.m_column = column;
		result.m_file = file;
		result.m_function = function;
		return result;
	}
	// NOLINTEND(bugprone-easily-swappable-parameters)

	[[nodiscard]] constexpr source_location() noexcept = default;

	[[nodiscard]] constexpr std::uint_least32_t line() const noexcept {
		return m_line;
	}
	[[nodiscard]] constexpr std::uint_least32_t column() const noexcept {
		return m_column;
	}
	[[nodiscard]] constexpr const char* file_name() const noexcept {
		return m_file;
	}
	[[nodiscard]] constexpr const char* function_name() const noexcept {
		return m_function;
	}

private:
	std::uint_least32_t m_line{};
	std::uint_least32_t m_column{};
	const char* m_file = "";
	const char* m_function = "";
};

}  // namespace std

#endif
