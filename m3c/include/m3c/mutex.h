/*
Copyright 2020 Michael Beckh

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

#include <windows.h>

#include <chrono>

namespace m3c {

/// @brief Same as `std::mutex` but using using the slim reader/writer (SWR) locks from the Windows API for synchronization.
/// @warn Other than `std::mutex`, SRW locks are NOT recursive.
class mutex final {
public:
	mutex() noexcept = default;
	mutex(const mutex&) = delete;
	mutex(mutex&&) = delete;
	~mutex() noexcept = default;

public:
	mutex& operator=(const mutex&) = delete;
	mutex& operator=(mutex&&) = delete;

public:
	/// @brief Acquires an exclusive lock on the `mutex` object.
	/// @warn In contrast to `std::mutex`, this call is NOT recursive.
	_Acquires_exclusive_lock_(m_lock) _Requires_lock_not_held_(m_lock) void lock() noexcept;
	/// @brief Acquires a shared lock on the `mutex` object.
	_Acquires_shared_lock_(m_lock) _Requires_lock_not_held_(m_lock) void lock_shared() noexcept;
	/// @brief Releases an exclusive lock on the `mutex` object.
	_Requires_exclusive_lock_held_(m_lock) _Releases_exclusive_lock_(m_lock) void unlock() noexcept;
	/// @brief Releases a shared lock on the `mutex` object.
	_Requires_shared_lock_held_(m_lock) _Releases_shared_lock_(m_lock) void unlock_shared() noexcept;

private:
	SRWLOCK m_lock = SRWLOCK_INIT;  ///< @brief The internal SRW lock.

	friend class condition_variable;
};

/// @brief Manages access to an exclusive lock on a `mutex` in a way safe for RAAI.
class scoped_lock final {
public:
	/// @brief Acquires an exclusive lock on a `mutex` object.
	_Acquires_exclusive_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) explicit scoped_lock(mutex& mtx) noexcept;
	scoped_lock(const scoped_lock&) = delete;
	scoped_lock(scoped_lock&&) = delete;
	/// @brief Releases the exclusive lock on the `mutex` object.
	_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) ~scoped_lock() noexcept;

public:
	scoped_lock& operator=(const scoped_lock&) = delete;
	scoped_lock& operator=(scoped_lock&&) = delete;

private:
	mutex& m_mutex;  ///< @brief The locked `mutex` object.

	friend class condition_variable;
};

/// @brief Manages access to a shared lock on a `mutex` in a way safe for RAAI.
class shared_lock final {
public:
	/// @brief Acquires a shared lock on a `mutex` object.
	_Acquires_shared_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) explicit shared_lock(mutex& mtx) noexcept;
	shared_lock(const shared_lock&) = delete;
	shared_lock(shared_lock&&) = delete;
	/// @brief Releases the shared lock on the `mutex` object.
	_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) ~shared_lock() noexcept;

public:
	shared_lock& operator=(const shared_lock&) = delete;
	shared_lock& operator=(shared_lock&&) = delete;

private:
	mutex& m_mutex;  ///< @brief The locked `mutex` object.

	friend class condition_variable;
};

/// @brief Manages a condition variable on a `mutex` object but uses slim reader/writer (SRW) locks for synchronization.
class condition_variable final {
public:
	condition_variable() noexcept = default;
	condition_variable(const condition_variable&) = delete;
	condition_variable(condition_variable&&) = delete;
	~condition_variable() noexcept = default;

public:
	condition_variable& operator=(const condition_variable&) = delete;
	condition_variable& operator=(condition_variable&&) = delete;

public:
	/// @brief Wait for the condition to be signaled.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @param lock The lock object.
	void wait(scoped_lock& lock);

	/// @brief Wait for the condition to be signaled.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @param lock The lock object.
	void wait(shared_lock& lock);

	/// @brief Wait until the condition is signaled or the timeout expires.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @tparam R The number of units (ticks) of the duration.
	/// @tparam P The period of the duration.
	/// @param lock The lock object.
	/// @param duration The maximum timeout for waiting.
	/// @return `true` if the condition was signaled, `false` if the timeout expired.
	template <typename R, typename P>
	[[nodiscard]] bool wait_for(scoped_lock& lock, const std::chrono::duration<R, P> duration) {
		return wait_for(lock, std::chrono::duration_cast<std::chrono::milliseconds>(duration));
	}

	/// @brief Wait until the condition is signaled or the timeout expires.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @tparam R The number of units (ticks) of the duration.
	/// @tparam P The period of the duration.
	/// @param lock The lock object.
	/// @param duration The maximum timeout for waiting.
	/// @return `true` if the condition was signaled, `false` if the timeout expired.
	template <typename R, typename P>
	[[nodiscard]] bool wait_for(shared_lock& lock, const std::chrono::duration<R, P> duration) {
		return wait_for(lock, std::chrono::duration_cast<std::chrono::milliseconds>(duration));
	}

	/// @brief Notify one thread waiting for the condition to be signaled.
	void notify_one() noexcept;

	/// @brief Notify all threads waiting for the condition to be signaled.
	void notify_all() noexcept;

private:
	/// @brief Wait until the condition is signaled or the timeout expires.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @param lock The lock object.
	/// @param milliseconds The maximum timeout for waiting.
	/// @return `true` if the condition was signaled, `false` if the timeout expired.
	[[nodiscard]] bool wait_for(scoped_lock& lock, DWORD milliseconds);

	/// @brief Wait until the condition is signaled or the timeout expires.
	/// @details The thread MUST hold @p lock before calling this function. The @p lock is releases while waiting.
	/// @param lock The lock object.
	/// @param milliseconds The maximum timeout for waiting.
	/// @return `true` if the condition was signaled, `false` if the timeout expired.
	[[nodiscard]] bool wait_for(shared_lock& lock, DWORD milliseconds);

private:
	/// @brief The internal condition variable for use with SRW locks.
	CONDITION_VARIABLE m_conditionVariable = CONDITION_VARIABLE_INIT;
};

}  // namespace m3c
