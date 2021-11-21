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

/// @file

#include "m3c/mutex.h"

#include "m3c/exception.h"

#include "m3c.events.h"

namespace m3c {

//
// mutex
//

_Acquires_exclusive_lock_(m_lock) _Requires_lock_not_held_(m_lock) void mutex::lock() noexcept {
	AcquireSRWLockExclusive(&m_lock);
}

_Acquires_shared_lock_(m_lock) _Requires_lock_not_held_(m_lock) void mutex::lock_shared() noexcept {
	AcquireSRWLockShared(&m_lock);
}

_Requires_exclusive_lock_held_(m_lock) _Releases_exclusive_lock_(m_lock) void mutex::unlock() noexcept {
	ReleaseSRWLockExclusive(&m_lock);
}

_Requires_shared_lock_held_(m_lock) _Releases_shared_lock_(m_lock) void mutex::unlock_shared() noexcept {
	ReleaseSRWLockShared(&m_lock);
}


//
// scoped_lock
//

_Acquires_exclusive_lock_(mtx.m_lock) _Requires_lock_not_held_(mtx.m_lock)
    _Post_same_lock_(mtx.m_lock, m_mutex.m_lock) scoped_lock::scoped_lock(mutex& mtx) noexcept
    : m_mutex(mtx) {
	mtx.lock();
}

_Requires_shared_lock_held_(m_mutex.m_lock) _Releases_shared_lock_(m_mutex.m_lock)
    scoped_lock::~scoped_lock() noexcept {
	m_mutex.unlock();
}


//
// shared_lock
//

_Acquires_shared_lock_(mtx.m_lock) _Requires_lock_not_held_(mtx.m_lock)
    _Post_same_lock_(mtx.m_lock, m_mutex.m_lock) shared_lock::shared_lock(mutex& mtx) noexcept
    : m_mutex(mtx) {
	mtx.lock_shared();
}

_Requires_shared_lock_held_(m_mutex.m_lock) _Releases_shared_lock_(m_mutex.m_lock)
    shared_lock::~shared_lock() noexcept {
	m_mutex.unlock_shared();
}


//
// condition_variable
//

void condition_variable::wait(scoped_lock& lock) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, INFINITE, 0)) {
		throw windows_error() + evt::condition_variable_Wait_E;
	}
}

void condition_variable::wait(shared_lock& lock) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED)) {
		throw windows_error() + evt::condition_variable_Wait_E;
	}
}

bool condition_variable::wait_for(scoped_lock& lock, const DWORD milliseconds) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, milliseconds, 0)) {
		if (const DWORD lastError = GetLastError(); lastError != ERROR_TIMEOUT) {
			throw windows_error(lastError) + evt::condition_variable_Wait_E;
		}
		return false;
	}
	return true;
}

bool condition_variable::wait_for(shared_lock& lock, const DWORD milliseconds) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, milliseconds, CONDITION_VARIABLE_LOCKMODE_SHARED)) {
		if (const DWORD lastError = GetLastError(); lastError != ERROR_TIMEOUT) {
			throw windows_error(lastError) + evt::condition_variable_Wait_E;
		}
		return false;
	}
	return true;
}

void condition_variable::notify_one() noexcept {
	WakeConditionVariable(&m_conditionVariable);
}

void condition_variable::notify_all() noexcept {
	WakeAllConditionVariable(&m_conditionVariable);
}

}  // namespace m3c
