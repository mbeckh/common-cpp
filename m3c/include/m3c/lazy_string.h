#pragma once


#include <sal.h>

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <new>
#include <string>

namespace llamalog {
class LogLine;
}

namespace m3c {

/// @brief A string that uses a stack buffer and only creates `std::basic_string` if the data is longer than the buffer.
/// @tparam kSize The size of the stack buffer in characters (including the null character).
/// @tparam CharT The character type.
template <std::uint16_t kSize, typename CharT>
class basic_lazy_string {
public:
	/// @brief The type of `std::basic_string` objects which are handled by this class.
	using string_type = std::basic_string<CharT>;
	///< @brief The type of `std::basic_string_view` objects which are handled by this class.
	using string_view_type = std::basic_string_view<CharT>;

private:
	using size_type = std::uint16_t;  ///< @brief The type used for the @p kSize template parameter.

public:
	/// @brief Create a new empty `basic_lazy_string` object.
	constexpr basic_lazy_string() noexcept
		: m_inline(true)
		, m_size(0) {
		m_buffer[0] = 0;
	}

	/// @brief Create a copy of another `basic_lazy_string` object.
	/// @note This constructor is used if both strings share the same size of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	basic_lazy_string(const basic_lazy_string& oth) {
		m_inline = oth.size() < kSize;
		if (m_inline) {
			m_size = static_cast<size_type>(oth.size());
			std::memcpy(m_buffer, oth.c_str(), (m_size + 1) * sizeof(CharT));
		} else {
			new (&m_string) string_type(oth.m_string);
		}
	}

	/// @brief Create a copy of another `basic_lazy_string` object.
	/// @note This constructor is used if both strings have different sizes of the internal buffer.
	/// @tparam kOthSize The size of the internal buffer of @p oth.
	/// @param oth The other `basic_lazy_string` object.
	template <std::size_t kOthSize>
	basic_lazy_string(const basic_lazy_string<kOthSize, CharT>& oth) {
		m_inline = oth.size() < kSize;
		if (m_inline) {
			m_size = static_cast<size_type>(oth.size());
			std::memcpy(m_buffer, oth.c_str(), (m_size + 1) * sizeof(CharT));
		} else {
			if (oth.m_inline) {
				new (&m_string) string_type(oth.m_buffer, oth.m_size);
			} else {
				new (&m_string) string_type(oth.m_string);
			}
		}
	}

	/// @brief Create a new instance by moving another `basic_lazy_string` into this instance.
	/// @note This constructor is used if both strings share the same size of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	basic_lazy_string(basic_lazy_string&& oth) noexcept
		: m_inline(oth.m_inline) {
		if (m_inline) {
			m_size = oth.m_size;
			std::memcpy(m_buffer, oth.m_buffer, (m_size + 1) * sizeof(CharT));
		} else {
			new (&m_string) string_type(std::move(oth.m_string));
		}
	}

	/// @brief Create a new instance by moving another `basic_lazy_string` object.
	/// @note This constructor is used if both strings have different sizes of the internal buffer.
	/// @tparam kOthSize The size of the internal buffer of @p oth.
	/// @param oth The other `basic_lazy_string` object.
	template <size_type kOthSize>
	basic_lazy_string(basic_lazy_string<kOthSize, CharT>&& oth) {
		if (oth.m_inline) {
			m_inline = oth.m_size < kSize;
			if (m_inline) {
				m_size = oth.m_size;
				std::memcpy(m_buffer, oth.m_buffer, (m_size + 1) * sizeof(CharT));
			} else {
				new (&m_string) string_type(oth.m_buffer, oth.m_size);
			}
		} else {
			// move string even if it is smaller than our buffer
			new (&m_string) string_type(std::move(oth.m_string));
			m_inline = false;
		}
	}

	/// @brief Create a new instance from a null-terminated character sequence.
	/// @param str A pointer to a null-terminated character sequence.
	basic_lazy_string(_In_z_ const CharT* str)
		: basic_lazy_string(str, std::char_traits<CharT>::length(str)) {
		// empty
	}

	/// @brief Create a new instance from a character sequence and a length.
	/// @note The characters sequence MUST have at least @p length non-null characters.
	/// @param str A pointer to a character sequence.
	/// @param length The number of non-null characters to create this string from.
	basic_lazy_string(_In_reads_or_z_(length) const CharT* str, const std::size_t length) {
		m_inline = length < kSize;
		if (m_inline) {
			m_size = static_cast<size_type>(length);
			std::memcpy(m_buffer, str, m_size * sizeof(CharT));
			m_buffer[m_size] = 0;
		} else {
			new (&m_string) string_type(str, length);
		}
		assert(length == size());
	}

	/// @brief Create a new instance as a copy of a `std::basic_string`.
	/// @param str The `std::basic_string` object.
	basic_lazy_string(const string_type& str) {
		const std::size_t length = str.size();
		m_inline = length < kSize;
		if (m_inline) {
			// copying string as string would copy data anyway
			m_size = static_cast<size_type>(length);
			std::memcpy(m_buffer, str.c_str(), (length + 1) * sizeof(CharT));
		} else {
			new (&m_string) string_type(str);
		}
	}

	/// @brief Create a new instance by moving a `std::basic_string` into the newly created instance.
	/// @param str The `std::basic_string` object.
	basic_lazy_string(string_type&& str) noexcept
		: m_inline(false) {
		// always move-in string which might save copying all data
		new (&m_string) string_type(std::move(str));
	}

	/// @brief Create a new instance as a copy of a `std::basic_string_view`.
	/// @param str The `std::basic_string_view` object.
	basic_lazy_string(const string_view_type& str) noexcept
		: basic_lazy_string(str.data(), str.size()) {
		// empty
	}

	/// @brief Clears any allocated resources.
	~basic_lazy_string() noexcept {
		if (!m_inline) {
			m_string.~basic_string();
		}
	}

public:
	//
	// Assignment
	//

	/// @brief Assign a copy of another `basic_lazy_string` to this instance.
	/// @note This operator  is used if both strings share the same size of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(const basic_lazy_string& oth) {
		if (oth.size() < kSize) {
			if (!m_inline) {
				m_string.~basic_string();
				m_inline = true;
			}
			m_size = static_cast<size_type>(oth.size());
			std::memcpy(m_buffer, oth.c_str(), (m_size + 1) * sizeof(CharT));
		} else {
			if (m_inline) {
				new (&m_string) string_type(oth.m_string);
				m_inline = false;
			} else {
				m_string = oth.m_string;
			}
		}
		return *this;
	}

	/// @brief Assign a copy of another `basic_lazy_string` to this instance.
	/// @note This operator is used if both strings have different sizes of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	/// @return A reference to this instance.
	template <size_type kOthSize>
	basic_lazy_string& operator=(const basic_lazy_string<kOthSize, CharT>& oth) {
		if (oth.size() < kSize) {
			if (!m_inline) {
				m_string.~basic_string();
				m_inline = true;
			}
			m_size = static_cast<size_type>(oth.size());
			std::memcpy(m_buffer, oth.c_str(), (m_size + 1) * sizeof(CharT));
		} else {
			if (oth.m_inline) {
				if (m_inline) {
					new (&m_string) string_type(oth.m_buffer, oth.m_size);
					m_inline = false;
				} else {
					m_string.assign(oth.m_buffer, oth.m_size);
				}
			} else {
				if (m_inline) {
					new (&m_string) string_type(oth.m_string);
					m_inline = false;
				} else {
					m_string = oth.m_string;
				}
			}
		}
		return *this;
	}

	/// @brief Move another `basic_lazy_string` into this instance.
	/// @note This operator is used if both strings share the same size of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(basic_lazy_string&& oth) noexcept {
		if (oth.m_inline) {
			if (!m_inline) {
				m_string.~basic_string();
				m_inline = true;
			}
			m_size = oth.m_size;
			std::memcpy(m_buffer, oth.m_buffer, (m_size + 1) * sizeof(CharT));
		} else {
			if (m_inline) {
				new (&m_string) string_type(std::move(oth.m_string));
				m_inline = false;
			} else {
				m_string = std::move(oth.m_string);
			}
		}
		return *this;
	}

	/// @brief Move another `basic_lazy_string` into this instance.
	/// @note This operator is used if both strings have different sizes of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	/// @return A reference to this instance.
	template <size_type kOthSize>
	basic_lazy_string& operator=(basic_lazy_string<kOthSize, CharT>&& oth) {
		if (oth.m_inline) {
			if (oth.m_size < kSize) {
				if (!m_inline) {
					m_string.~basic_string();
					m_inline = true;
				}
				m_size = oth.m_size;
				std::memcpy(m_buffer, oth.m_buffer, (m_size + 1) * sizeof(CharT));
			} else if (m_inline) {
				new (&m_string) string_type(oth.m_buffer, oth.m_size);
				m_inline = false;
			} else {
				m_string.assign(oth.m_buffer, oth.m_size);
			}
		} else {
			if (m_inline) {
				new (&m_string) string_type(std::move(oth.m_string));
				m_inline = false;
			} else {
				m_string = std::move(oth.m_string);
			}
		}
		return *this;
	}

	/// @brief Assign a null-terminated character sequence to this instance.
	/// @param str A pointer to a null-terminated character sequence.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(const CharT* const str) {
		return assign(str, std::wcslen(str));
	}

	/// @brief Assign a character sequence to this instance.
	/// @note The characters sequence MUST have at least @p length non-null characters.
	/// @param str A pointer to a null-terminated character sequence.
	/// @param length The number of non-null characters to create this string from.
	/// @return A reference to this instance.
	basic_lazy_string& assign(const CharT* const str, const std::size_t length) {
		if (length < kSize) {
			if (!m_inline) {
				m_string.~basic_string();
				m_inline = true;
			}
			m_size = static_cast<size_type>(length);
			std::memcpy(m_buffer, str, m_size * sizeof(CharT));
			m_buffer[m_size] = 0;
		} else {
			if (m_inline) {
				new (&m_string) string_type(str, length);
				m_inline = false;
			} else {
				m_string.assign(str, length);
			}
		}
		assert(length == size());
		return *this;
	}

	/// @brief Assign a copy of a `std::basic_string` to this instance.
	/// @param str The `std::basic_string` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(const string_type& str) {
		return assign(str.c_str(), str.size());
	}

	/// @brief Move a `std::basic_string` into this instance.
	/// @param str The `std::basic_string` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(string_type&& str) {
		if (m_inline) {
			new (&m_string) string_type(std::move(str));
			m_inline = false;
		} else {
			m_string = std::move(str);
		}
		return *this;
	}

	/// @brief Assign a copy of a `std::basic_string_view` to this instance.
	/// @param str The `std::basic_string_view` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator=(const string_view_type& str) {
		return assign(str.data(), str.size());
	}


	//
	// Append
	//

	/// @brief Append another `basic_lazy_string` to this instance.
	/// @note This operator is used for strings having the same or different sizes of the internal buffer.
	/// @tparam kOthSize The size of the internal buffer of @p add.
	/// @param add The other `basic_lazy_string` object.
	/// @return A reference to this instance.
	template <size_type kOthSize>
	basic_lazy_string& operator+=(const basic_lazy_string<kOthSize, CharT>& add) {
		const string_view_type sv = add.sv();
		return append(sv.data(), sv.size());
	}

	/// @brief Append a single character to this instance.
	/// @param ch The character to append.
	/// @return A reference to this instance.
	basic_lazy_string& operator+=(const CharT ch) {
		return append(&ch, 1);
	}

	/// @brief Append a null-terminated character sequence to this instance.
	/// @param add A pointer to a null-terminated character sequence.
	/// @return A reference to this instance.
	basic_lazy_string& operator+=(const CharT* const add) {
		return append(add, std::char_traits<CharT>::length(add));
	}

	/// @brief Append characters from a character sequence to this instance.
	/// @note The characters sequence MUST have at least @p len non-null characters.
	/// @param add A pointer to a character sequence.
	/// @return A reference to this instance.
	basic_lazy_string& append(const CharT* const add, const std::size_t len) {
		const std::size_t oldSize = size();
		const std::size_t addSize = len;
		const std::size_t newSize = oldSize + addSize;
		resize(newSize);

		CharT* const buffer = data();
		std::memcpy(&buffer[oldSize], add, addSize * sizeof(CharT));
		// setting 0 explicitly automatically allows cases where len is less than the length of add
		buffer[newSize] = 0;

		assert(newSize);
		return *this;
	}

	/// @brief Append a `std::basic_string` to this instance.
	/// @param add The `std::basic_string` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator+=(const string_type& add) {
		return append(add.c_str(), add.size());
	}

	/// @brief Append a `std::basic_string_view` to this instance.
	/// @param add The `std::basic_string_view` object.
	/// @return A reference to this instance.
	basic_lazy_string& operator+=(const string_view_type& add) {
		return append(add.data(), add.size());
	}

public:
	//
	// Concatenate
	//

	/// @brief Return a new `basic_lazy_string` built by appending @p rhs to this instance.
	/// @tparam kOthSize The size of the internal buffer of @p rhs.
	/// @param rhs The `basic_lazy_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	template <size_type kOthSize>
	basic_lazy_string operator+(const basic_lazy_string<kOthSize, CharT>& rhs) const {
		const string_view_type lsv = sv();
		const string_view_type rsv = rhs.sv();
		return concat(lsv.data(), lsv.size(), rsv.data(), rsv.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a single character to a `basic_lazy_string` object.
	/// @param str The `basic_lazy_string` to append to.
	/// @param ch The character to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const basic_lazy_string& str, const CharT ch) {
		const string_view_type sv = str.sv();
		return concat(sv.data(), sv.size(), &ch, 1);
	}

	/// @brief Return a new `basic_lazy_string` built by appending this instance to single character.
	/// @param ch The character to append to.
	/// @param str The `basic_lazy_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const CharT ch, const basic_lazy_string& str) {
		const string_view_type sv = str.sv();
		return concat(&ch, 1, sv.data(), sv.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a null-terminated character sequence to a `basic_lazy_string` object.
	/// @param lhs The `basic_lazy_string` to append to.
	/// @param rhs The null-terminated character sequence to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const basic_lazy_string& lhs, const CharT* const rhs) {
		const string_view_type sv = lhs.sv();
		return concat(sv.data(), sv.size(), rhs, std::char_traits<CharT>::length(rhs));
	}

	/// @brief Return a new `basic_lazy_string` built by appending a `basic_lazy_string` to a null-terminated character sequence.
	/// @param lhs The null-terminated character sequence to append to.
	/// @param rhs The `basic_lazy_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const CharT* const lhs, const basic_lazy_string& rhs) {
		const string_view_type sv = rhs.sv();
		return concat(lhs, std::char_traits<CharT>::length(lhs), sv.data(), sv.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a `std::basic_string` to a `basic_lazy_string` object.
	/// @param lhs The `basic_lazy_string` to append to.
	/// @param rhs The `std::basic_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const basic_lazy_string& lhs, const string_type& rhs) {
		const string_view_type sv = lhs.sv();
		return concat(sv.data(), sv.size(), rhs.c_str(), rhs.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a `basic_lazy_string` to a `std::basic_string`.
	/// @param lhs The `std::basic_string` to append to.
	/// @param rhs The `basic_lazy_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const string_type& lhs, const basic_lazy_string& rhs) {
		const string_view_type sv = rhs.sv();
		return concat(lhs.c_str(), lhs.size(), sv.data(), sv.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a `std::basic_string_view` to a `basic_lazy_string` object.
	/// @param lhs The `basic_lazy_string` to append to.
	/// @param rhs The `std::basic_string_view` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const basic_lazy_string& lhs, const string_view_type& rhs) {
		const string_view_type sv = lhs.sv();
		return concat(sv.data(), sv.size(), rhs.data(), rhs.size());
	}

	/// @brief Return a new `basic_lazy_string` built by appending a `basic_lazy_string` to a `std::basic_string_view`.
	/// @param lhs The `std::basic_string_view` to append to.
	/// @param rhs The `basic_lazy_string` to append.
	/// @return A newly created `basic_lazy_string` object.
	friend basic_lazy_string operator+(const string_view_type& lhs, const basic_lazy_string& rhs) {
		const string_view_type sv = rhs.sv();
		return concat(lhs.data(), lhs.size(), sv.data(), sv.size());
	}

private:
	/// @brief Return a new `basic_lazy_string` built by appending one character sequence to another.
	/// @note Both character sequences MUST have at least @p lSize viz. @p rSize non-null characters.
	/// @param lhs The character sequence to append to.
	/// @param lSize The number on non-null characters in @p lhs.
	/// @param rhs The character sequence to append.
	/// @param rSize The number on non-null characters in @p rhs.
	/// @return A newly created `basic_lazy_string` object.
	static basic_lazy_string concat(const CharT* const lhs, const std::size_t lSize, const CharT* const rhs, const std::size_t rSize) {
		const std::size_t newSize = lSize + rSize;

		basic_lazy_string result;
		result.resize(newSize);

		CharT* const buffer = result.data();
		std::memcpy(buffer, lhs, lSize * sizeof(CharT));
		std::memcpy(&buffer[lSize], rhs, rSize * sizeof(CharT));
		buffer[newSize] = 0;
		return result;
	}

public:
	//
	// Comparison
	//

	/// @brief Compare the instance with another `basic_lazy_string` object.
	/// @tparam kOthSize The size of the internal buffer of @p oth.
	/// @param oth The other `basic_lazy_string` object.
	/// @return `true` if both objects have the same character sequence, else `false`.
	template <size_type kOthSize>
	[[nodiscard]] bool operator==(const basic_lazy_string<kOthSize, CharT>& oth) const noexcept {
		return (*this <=> oth) == 0;
	}

	/// @brief Compare the instance with a null-terminated character sequence.
	/// @param oth The null-terminated character sequence.
	/// @return `true` if both objects have the same character sequence, else `false`.
	[[nodiscard]] bool operator==(const CharT* const oth) const noexcept {
		return (*this <=> oth) == 0;
	}

	/// @brief Compare the instance with a `std::basic_string`.
	/// @param oth The `std::basic_string` object.
	/// @return `true` if both objects have the same character sequence, else `false`.
	[[nodiscard]] bool operator==(const string_type& oth) const noexcept {
		return (*this <=> oth) == 0;
	}

	/// @brief Compare the instance with a `std::basic_string_view`.
	/// @param oth The `std::basic_string_view` object.
	/// @return `true` if both objects have the same character sequence, else `false`.
	[[nodiscard]] bool operator==(const string_view_type& oth) const noexcept {
		return (*this <=> oth) == 0;
	}

	/// @brief Compare the instance with another `basic_lazy_string`.
	/// @tparam kOthSize The size of the internal buffer of @p oth.
	/// @param oth The other `basic_lazy_string` object.
	/// @return A value that specifies the relative ordering of both objects.
	template <size_type kOthSize>
	[[nodiscard]] std::strong_ordering operator<=>(const basic_lazy_string<kOthSize, CharT>& oth) const noexcept {
		const std::size_t othSize = oth.size();
		if (const int cmp = std::char_traits<CharT>::compare(c_str(), oth.c_str(), std::min(size(), othSize)); cmp) {
			return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
		}
		return size() <=> othSize;
	}

	/// @brief Compare the instance with a null-terminated character sequence.
	/// @param oth The null-terminated character sequence.
	/// @return A value that specifies the relative ordering of the instance and the character sequence.
	[[nodiscard]] std::strong_ordering operator<=>(const CharT* const oth) const noexcept {
		const std::size_t othSize = std::char_traits<CharT>::length(oth);
		if (const int cmp = std::char_traits<CharT>::compare(c_str(), oth, std::min(size(), othSize)); cmp) {
			return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
		}
		return size() <=> othSize;
	}

	/// @brief Compare the instance with a `std::basic_string`.
	/// @param oth The `std::basic_string` object.
	/// @return A value that specifies the relative ordering of the instance and the `std::basic_string`.
	[[nodiscard]] std::strong_ordering operator<=>(const string_type& oth) const noexcept {
		const std::size_t othSize = oth.size();
		if (const int cmp = std::char_traits<CharT>::compare(c_str(), oth.c_str(), std::min(size(), othSize)); cmp) {
			return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
		}
		return size() <=> othSize;
	}

	/// @brief Compare the instance with a `std::basic_string_view`.
	/// @param oth The `std::basic_string_view` object.
	/// @return A value that specifies the relative ordering of the instance and the `std::basic_string_view`.
	[[nodiscard]] std::strong_ordering operator<=>(const string_view_type& oth) const noexcept {
		const std::size_t othSize = oth.size();
		if (const int cmp = std::char_traits<CharT>::compare(c_str(), oth.data(), std::min(size(), othSize)); cmp) {
			return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
		}
		return size() <=> othSize;
	}

public:
	/// @brief Create a `std::basic_string_view` that contains the data of this instance.
	/// @return A newly created `std::basic_string_view` object.
	[[nodiscard]] string_view_type sv() const noexcept {
		return string_view_type(c_str(), size());
	}

	/// @brief Access the data of this instance as a null-terminated character sequence.
	/// @return A pointer to a null-terminated character sequence.
	[[nodiscard]] const CharT* c_str() const noexcept {
		return m_inline ? m_buffer : m_string.c_str();
	}

	/// @brief Access the data of this instance as a modifiable null-terminated character sequence.
	/// @return A pointer to a null-terminated character sequence.
	[[nodiscard]] CharT* data() noexcept {
		return m_inline ? m_buffer : m_string.data();
	}

	/// @brief Get the length of the character sequence managed by this instance.
	/// @return The numer of non-null characters in this character sequence.
	[[nodiscard]] std::size_t size() const noexcept {
		return m_inline ? m_size : m_string.size();
	}

	/// @brief Checks if this instance stores any data.
	/// @return `true` if the `#size()` of this instance is 0, else `false`.
	[[nodiscard]] bool empty() const noexcept {
		return m_inline ? !m_size : m_string.empty();
	}

	/// @brief Discards the contents and resets the object to an empty state.
	void clear() noexcept {
		if (m_inline) {
			m_size = 0;
			m_buffer[0] = 0;
		} else {
			m_string.clear();
		}
	}

	/// @brief Allocated size for string growth.
	/// @param newSize The number of characters to allocate space for.
	void resize(const std::size_t newSize) {
		if (newSize == size()) {
			return;
		}
		if (m_inline) {
			if (newSize < kSize) {
				m_buffer[m_size = static_cast<size_type>(newSize)] = 0;
			} else {
				// first move to temp because of union
				string_type tmp(m_buffer, m_size);
				new (&m_string) string_type(std::move(tmp));
				m_inline = false;
				m_string.resize(newSize);
			}
		} else {
			m_string.resize(newSize);
		}
	}

	/// @brief Get a suitable hash value.
	/// @return A value that can be used in hash functions.
	[[nodiscard]] std::size_t hash() const noexcept {
		const CharT* const ptr = c_str();
		std::size_t h = 0;
		for (std::size_t i = 0; i < size(); ++i) {
			h = 31 * h + ptr[i];
		}
		return h;
	}

	/// @brief Exchanges this instance with another `basic_lazy_string` object.
	/// @note This function is used if both strings share the same size of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	void swap(basic_lazy_string& oth) noexcept {
		if (m_inline) {
			if (oth.m_inline) {
				const size_type minSize = std::min(m_size, oth.m_size) + 1;
				for (size_type i = 0; i < minSize; ++i) {
					const CharT tmp = m_buffer[i];
					m_buffer[i] = oth.m_buffer[i];
					oth.m_buffer[i] = tmp;
				}
				if (m_size < oth.m_size) {
					std::memcpy(&m_buffer[minSize], &oth.m_buffer[minSize], (oth.m_size - minSize + 1) * sizeof(CharT));
				} else if (m_size > oth.m_size) {
					std::memcpy(&oth.m_buffer[minSize], &m_buffer[minSize], (m_size - minSize + 1) * sizeof(CharT));
				} else {
					// just swap
				}
				std::swap(m_size, oth.m_size);
			} else {
				string_type tmp(std::move(oth.m_string));
				oth.m_string.~basic_string();
				oth.m_inline = true;
				std::memcpy(oth.m_buffer, m_buffer, (m_size + 1) * sizeof(CharT));
				oth.m_size = m_size;
				new (&m_string) string_type(std::move(tmp));
				m_inline = false;
			}
		} else {
			if (oth.m_inline) {
				string_type tmp(std::move(m_string));
				m_string.~basic_string();
				m_inline = true;
				std::memcpy(m_buffer, oth.m_buffer, (oth.m_size + 1) * sizeof(CharT));
				m_size = oth.m_size;
				new (&oth.m_string) string_type(std::move(tmp));
				oth.m_inline = false;
			} else {
				std::swap(m_string, oth.m_string);
			}
		}
	}

	/// @brief Exchanges this instance with another `basic_lazy_string` object.
	/// @note This function is used if both strings have different sizes of the internal buffer.
	/// @param oth The other `basic_lazy_string` object.
	template <size_type kOthSize>
	void swap(basic_lazy_string<kOthSize, CharT>& oth) {
		if (m_inline) {
			if (oth.m_inline) {
				if (oth.m_size < kSize) {
					if (m_size < kOthSize) {
						const size_type minSize = std::min(m_size, oth.m_size) + 1;
						for (size_type i = 0; i < minSize; ++i) {
							const CharT tmp = m_buffer[i];
							m_buffer[i] = oth.m_buffer[i];
							oth.m_buffer[i] = tmp;
						}
						if (m_size < oth.m_size) {
							std::memcpy(&m_buffer[minSize], &oth.m_buffer[minSize], (oth.m_size - minSize + 1) * sizeof(CharT));
						} else if (m_size > oth.m_size) {
							std::memcpy(&oth.m_buffer[minSize], &m_buffer[minSize], (m_size - minSize + 1) * sizeof(CharT));
						} else {
							// just swap
						}
						std::swap(m_size, oth.m_size);
					} else {
						string_type tmp(m_buffer, m_size);
						std::memcpy(m_buffer, oth.m_buffer, (oth.m_size + 1) * sizeof(CharT));
						m_size = oth.m_size;
						new (&oth.m_string) string_type(std::move(tmp));
						oth.m_inline = false;
					}
				} else {
					if (m_size < kOthSize) {
						string_type tmp(oth.m_buffer, oth.m_size);
						std::memcpy(oth.m_buffer, m_buffer, (m_size + 1) * sizeof(CharT));
						oth.m_size = m_size;
						new (&m_string) string_type(std::move(tmp));
						m_inline = false;
					} else {
						assert(false);
					}
				}
			} else {
				string_type tmp(std::move(oth.m_string));
				if (m_size < kOthSize) {
					oth.m_string.~basic_string();
					oth.m_inline = true;
					std::memcpy(oth.m_buffer, m_buffer, (m_size + 1) * sizeof(CharT));
					oth.m_size = m_size;
				} else {
					oth.m_string.assign(m_buffer, m_size);
				}
				new (&m_string) string_type(std::move(tmp));
				m_inline = false;
			}
		} else {
			if (oth.m_inline) {
				string_type tmp(std::move(m_string));
				if (oth.m_size < kSize) {
					m_string.~basic_string();
					m_inline = true;
					std::memcpy(m_buffer, oth.m_buffer, (oth.m_size + 1) * sizeof(CharT));
					m_size = oth.m_size;
				} else {
					m_string.assign(oth.m_buffer, oth.m_size);
				}
				new (&oth.m_string) string_type(std::move(tmp));
				oth.m_inline = false;
			} else {
				std::swap(m_string, oth.m_string);
			}
		}
	}

private:
	/// @brief `true` if the data is stored in the internal buffer, `false` if using a `std::basic_string`.
	bool m_inline;  // NOLINT(readability-identifier-naming): All constructors are explicitly defined.
	/// @brief The number of characters stored in the internal buffer. Undefined if data is stored in a `std::basic_string`.
	size_type m_size;  // NOLINT(readability-identifier-naming): All constructors are explicitly defined.
	union {
		/// @brief The internal buffer for storing data.
		CharT m_buffer[kSize];  // NOLINT(readability-identifier-naming): Member of anonymous union is part of enclosing scope.
		/// @brief A `std::basic_string` storing the data.
		std::basic_string<CharT> m_string;  // NOLINT(readability-identifier-naming): Member of anonymous union is part of enclosing scope.
	};

	template <size_type, typename>
	friend class basic_lazy_string;
};

/// @brief Swap function for `basic_lazy_string` objects with same buffer size.
/// @tparam kSize The buffer size of both strings.
/// @tparam CharT The character type.
/// @param str A `basic_lazy_string` object.
/// @param oth Another `basic_lazy_string` object.
template <std::uint16_t kSize, typename CharT>
inline void swap(basic_lazy_string<kSize, CharT>& str, basic_lazy_string<kSize, CharT>& oth) noexcept {
	str.swap(oth);
}

/// @brief Swap function for `basic_lazy_string` objects with different buffer size.
/// @tparam kSize0 The buffer size of the first string.
/// @tparam kSize1 The buffer size of the second string.
/// @tparam CharT The character type.
/// @param str A `basic_lazy_string` object.
/// @param oth Another `basic_lazy_string` object.
template <std::uint16_t kSize0, std::uint16_t kSize1, typename CharT>
inline void swap(basic_lazy_string<kSize0, CharT>& str, basic_lazy_string<kSize1, CharT>& oth) {
	str.swap(oth);
}

/// @brief A `basic_lazy_string` using regular characters.
template <std::uint16_t kSize>
using lazy_string = basic_lazy_string<kSize, char>;

/// @brief A `basic_lazy_string` using wide characters.
template <std::uint16_t kSize>
using lazy_wstring = basic_lazy_string<kSize, wchar_t>;

/// @brief Add the value of the lazy string to a `llamalog::LogLine`.
/// @tparam kSize The buffer size of the string.
/// @tparam CharT The character type.
/// @param logLine The output target.
/// @param arg A `m3c::basic_lazy_string`.
template <std::uint16_t kSize, typename CharT>
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const basic_lazy_string<kSize, CharT>& arg) {
	return logLine << arg.sv();
}

}  // namespace m3c

/// @brief Specialization of std::hash.
template <std::uint16_t kSize, typename CharT>
struct std::hash<m3c::basic_lazy_string<kSize, CharT>> {
	[[nodiscard]] std::size_t operator()(const m3c::basic_lazy_string<kSize, CharT>& str) const noexcept {
		return str.hash();
	}
};
