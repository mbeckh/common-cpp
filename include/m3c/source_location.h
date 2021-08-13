#pragma once
#include <cstdint>

namespace m3c {

struct source_location {
	static constexpr source_location current(const std::uint_least32_t line = __builtin_LINE(),
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

}  // namespace m3c
