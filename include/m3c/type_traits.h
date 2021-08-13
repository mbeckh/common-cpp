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
#pragma once

#include <type_traits>

namespace m3c {

/// @brief A helper to check if a class matches any of a given set.
/// @tparam T The class to check.
/// @tparam A The possible alternatives.
template <class T, class... A>
struct is_any : std::disjunction<std::is_same<T, A>...> {  // NOLINT(readability-identifier-naming): Named like C++ type traits.
														   // empty
};

/// @brief Constant to shorten expressions using `is_any`.
/// @tparam T The class to check.
/// @tparam A The possible alternatives.
template <class T, class... A>
constexpr bool is_any_v = is_any<T, A...>::value;  // NOLINT(readability-identifier-naming): Named like C++ type traits.}

}  // namespace m3c
