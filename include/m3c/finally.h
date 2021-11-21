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

#include <type_traits>
#include <utility>

namespace m3c {

namespace internal {

/// @brief Helper class for running code during stack unwinding (similar to finally from the GSL).
/// @tparam F The type of the lambda called during stack unwinding.
template <class F>
requires std::is_nothrow_invocable_v<F>
class final_action final {
public:
	/// @brief Create a final action.
	/// @param f The lambda to be called during stack unwinding.
	[[nodiscard]] constexpr explicit final_action(F&& f) noexcept
	    : m_f(std::forward<F>(f)) {
		static_assert(noexcept(m_f()), "lambda must be noexcept");
	}

	final_action(const final_action&) = delete;  ///< @nocopyconstructor

	/// @brief Transfers control of the lambda to this instance.
	/// @param oth The previous owner.
	[[nodiscard]] constexpr final_action(final_action&& oth) noexcept
	    : m_f(std::move(oth.m_f))
	    , m_active(oth.m_active) {
		oth.m_active = false;
	}

	/// @brief Calls the lambda if still owned by this instance.
	constexpr ~final_action() noexcept {
		if (m_active) {
			[[likely]];
			m_f();
		}
	}

public:
	final_action& operator=(const final_action&) = delete;  ///< @noassignmentoperator
	final_action& operator=(final_action&&) = delete;       ///< @nomoveoperator

private:
	F m_f;                 ///< @brief Our lambda.
	bool m_active = true;  ///< @brief `true` if this instance is the owner of the lambda.
};

}  // namespace internal


/// @brief Run code during stack unwinding.
/// @tparam F The (auto-deducted) type of the lambda expression.
/// @param f The lambda expression. The expression MUST NOT throw any exceptions.
template <class F>
requires std::is_nothrow_invocable_v<F>
[[nodiscard]] constexpr internal::final_action<F> finally(F&& f) noexcept {
	return internal::final_action<F>(std::forward<F>(f));
}

}  // namespace m3c
