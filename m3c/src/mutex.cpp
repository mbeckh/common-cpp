#include "m3c/mutex.h"

#include "m3c/exception.h"

#include <llamalog/exception.h>

#include <mutex>
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

_Acquires_exclusive_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) scoped_lock::scoped_lock(mutex& mtx) noexcept
	: m_mutex(mtx) {
	mtx.lock();
}
_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) scoped_lock::~scoped_lock() noexcept {
	m_mutex.unlock();
}


//
// shared_lock
//

_Acquires_shared_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) shared_lock::shared_lock(mutex& mtx) noexcept
	: m_mutex(mtx) {
	mtx.lock_shared();
}
_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) shared_lock::~shared_lock() noexcept {
	m_mutex.unlock_shared();
}


//
// condition_variable
//

void condition_variable::wait(scoped_lock& lock) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, INFINITE, 0)) {
		LLAMALOG_THROW(m3c::windows_exception(GetLastError(), "SleepConditionVariableSRW"));
	}
}

void condition_variable::wait(shared_lock& lock) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED)) {
		LLAMALOG_THROW(m3c::windows_exception(GetLastError(), "SleepConditionVariableSRW"));
	}
}

bool condition_variable::wait_for(scoped_lock& lock, const DWORD milliseconds) {
	if (SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, milliseconds, 0)) {
		if (const DWORD lastError = GetLastError(); lastError != ERROR_TIMEOUT) {
			LLAMALOG_THROW(m3c::windows_exception(lastError, "SleepConditionVariableSRW"));
		}
		return false;
	}
	return true;
}

bool condition_variable::wait_for(shared_lock& lock, const DWORD milliseconds) {
	if (!SleepConditionVariableSRW(&m_conditionVariable, &lock.m_mutex.m_lock, milliseconds, CONDITION_VARIABLE_LOCKMODE_SHARED)) {
		if (const DWORD lastError = GetLastError(); lastError != ERROR_TIMEOUT) {
			LLAMALOG_THROW(m3c::windows_exception(lastError, "SleepConditionVariableSRW"));
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
