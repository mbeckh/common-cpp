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

#include <sal.h>

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>

#ifdef __clang_analyzer__
// clang can't handle some static_assert checks in templated code
// NOLINTNEXTLINE(readability-identifier-naming): Kind of funny that clang complains about a macro to work around shortcomings of clang.
#define static_assert_no_clang(...)
#else
#define static_assert_no_clang(...) static_assert(__VA_ARGS__)
#endif

namespace m3c {

/// @brief A compile-time evaluated helper function to select one of two strings based on character type.
/// @tparam CharT The type of the result string.
/// @param sz A string selected for `char` results.
/// @param wsz A string selected for `wchar_t` results.
/// @return @p sz if @p CharT is `char` and @p wsz if @p CharT is `wchar_t`.
template <typename CharT>
consteval _Ret_z_ const CharT* SelectString(_In_z_ const char* const sz, _In_z_ const wchar_t* const wsz) noexcept {
	if constexpr (std::is_same_v<char, CharT>) {
		return sz;
	} else if constexpr (std::is_same_v<wchar_t, CharT>) {
		return wsz;
	} else {
		static_assert_no_clang(false, "Unknown char type");
	}
}

// forward declaration of class
template <typename T>
class unique_ptr;

/// @brief A helper to check if a type matches any one out of a given set.
/// @tparam T The type to check.
/// @tparam A The possible alternatives.
template <typename T, typename... A>
struct is_any_of : std::disjunction<std::is_same<T, A>...> {
	// empty
};

/// @brief Constant to shorten expressions using `is_any_of`.
/// @tparam T The type to check.
/// @tparam A The possible alternatives.
template <typename T, typename... A>
inline constexpr bool is_any_of_v = is_any_of<T, A...>::value;

/// @brief A type out of a number of alternatives.
/// @tparam T The type.
/// @tparam A The possible alternatives.
template <typename T, typename... A>
concept AnyOf = is_any_of_v<T, A...>;


/// @brief Check if a type is a - possibly const or volatile - pointer to a type, i.e. `true` if  @p T is `Type*` or `const Type*`.
/// @tparam T The type to check.
/// @tparam Type The type the pointer points to.
template <typename T, typename Type>
struct is_pointer_to : std::bool_constant<std::is_pointer_v<T> && std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<T>>, Type>> {
	// empty
};

/// @brief Constant to shorten expressions using `is_pointer_to`.
/// @tparam T The type to check.
/// @tparam Type The type the pointer points to.
template <typename T, typename Type>
inline constexpr bool is_pointer_to_v = is_pointer_to<T, Type>::value;

/// @brief A pointer to a particular type.
/// @details `true` if @p T is @p Type*.
/// @tparam T The type.
/// @tparam Type The target of the pointer.
template <typename T, typename Type>
concept PointerTo = is_pointer_to_v<T, Type>;


/// @brief Check if a type is a specialization of a template.
template <typename, template <typename, typename...> typename>
struct is_specialization_of : public std::false_type {};

/// @brief Check if a type is a specialization of a template.
/// @tparam T The type to check.
/// @tparam Template The template to check.
template <typename... T, template <typename, typename...> typename Template>
struct is_specialization_of<Template<T...>, Template> : public std::true_type {};

/// @brief Constant to shorten expressions using `is_specialization_of`.
/// @tparam T The type to check.
/// @tparam Template The template to check.
template <typename T, template <typename, typename...> typename Template>
inline constexpr bool is_specialization_of_v = is_specialization_of<T, Template>::value;

/// @brief A specialization of a particular template. @details Templates having non-type argments are currently not supported.
/// @tparam T The type.
/// @tparam Template The template.
template <typename T, template <typename, typename...> typename Template>
concept SpecializationOf = is_specialization_of_v<T, Template>;


template <typename>
struct is_span : std::false_type {};

template <typename E, std::size_t kExtent>
struct is_span<std::span<E, kExtent>> : std::true_type {};

template <typename T>
inline constexpr bool is_span_v = is_span<T>::value;

template <typename T>
concept Span = is_span_v<T>;


/// @brief Check if a type is a `std::unique_ptr` or `m3c::unique_ptr` to a particular type.
template <typename...>
struct is_unique_ptr_to : std::false_type {};

/// @brief Check if a type is a `std::unique_ptr<T>`.
/// @tparam T The required type of the `std::unique_ptr`.
template <typename T>
struct is_unique_ptr_to<std::unique_ptr<T>> : std::true_type {};

/// @brief Check if a type is a `m3c::unique_ptr<T>`.
/// @tparam T The required type of the `m3c::unique_ptr`.
template <typename T>
struct is_unique_ptr_to<m3c::unique_ptr<T>> : std::true_type {};

/// @brief Check if a type is a `std::unique_ptr<T, D>`.
/// @tparam T The required type of the `std::unique_ptr`.
/// @tparam D The deleter.
template <typename T, typename D>
struct is_unique_ptr_to<std::unique_ptr<T, D>> : std::true_type {};

/// @brief Constant to shorten expressions using `is_unique_ptr_to`.
/// @tparam T The required type of the `std::unique_ptr`.
template <typename T>
inline constexpr bool is_unique_ptr_to_v = is_unique_ptr_to<T>::value;

/// @brief Either a `std::unique_ptr` or a `m3c::unique_ptr` to @p T.
/// @tparam T The required type of the `std::unique_ptr` and `m3c::unique_ptr` respectively.
template <typename T>
concept UniquePtrTo = is_unique_ptr_to_v<T>;

}  // namespace m3c
