#pragma once

#include <windows.h>

#include <chrono>

namespace m3c {

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
	_Acquires_exclusive_lock_(m_lock) _Requires_lock_not_held_(m_lock) void lock() noexcept;
	_Acquires_shared_lock_(m_lock) _Requires_lock_not_held_(m_lock) void lock_shared() noexcept;
	_Requires_exclusive_lock_held_(m_lock) _Releases_exclusive_lock_(m_lock) void unlock() noexcept;
	_Requires_shared_lock_held_(m_lock) _Releases_shared_lock_(m_lock) void unlock_shared() noexcept;

private:
	SRWLOCK m_lock = SRWLOCK_INIT;

	friend class condition_variable;
};

class scoped_lock final {
public:
	_Acquires_exclusive_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) explicit scoped_lock(mutex& mtx) noexcept;
	scoped_lock(const scoped_lock&) = delete;
	scoped_lock(scoped_lock&&) = delete;
	_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) ~scoped_lock() noexcept;

public:
	scoped_lock& operator=(const scoped_lock&) = delete;
	scoped_lock& operator=(scoped_lock&&) = delete;

private:
	mutex& m_mutex;

	friend class condition_variable;
};

class shared_lock final {
public:
	_Acquires_shared_lock_(mtx) _Requires_lock_not_held_(mtx) _Post_same_lock_(mtx, m_mutex) explicit shared_lock(mutex& mtx) noexcept;
	shared_lock(const shared_lock&) = delete;
	shared_lock(shared_lock&&) = delete;
	_Requires_shared_lock_held_(m_mutex) _Releases_shared_lock_(m_mutex) ~shared_lock() noexcept;

public:
	shared_lock& operator=(const shared_lock&) = delete;
	shared_lock& operator=(shared_lock&&) = delete;

private:
	mutex& m_mutex;

	friend class condition_variable;
};

#if 0
class scoped_lock final {
public:
	_Acquires_nonreentrant_lock_(m_mutex) _Post_same_lock_(m_mutex, mtx) explicit scoped_lock(mutex& mtx) noexcept
		: m_mutex(mtx) {
		mtx.lock();
	}
	scoped_lock(const scoped_lock&) = delete;
	scoped_lock(scoped_lock&&) = delete;
	~scoped_lock() noexcept {
		if (m_locked) {
			m_mutex.unlock();
		}
	}

public:
	scoped_lock& operator=(const scoped_lock&) = delete;
	scoped_lock& operator=(scoped_lock&&) = delete;

public:
	_Acquires_nonreentrant_lock_(m_mutex) void lock() noexcept {
		assert(!m_locked);
		m_mutex.lock();
		m_locked = true;
	}

	_Releases_nonreentrant_lock_(m_mutex) void unlock() noexcept {
		assert(m_locked);
		m_mutex.unlock();
		m_locked = false;
	}

private:
	mutex& m_mutex;
	bool m_locked = true;

	friend class condition_variable;
};

class shared_lock final {
public:
	_Acquires_nonreentrant_lock_(m_mutex) _Post_same_lock_(m_mutex, mtx) explicit shared_lock(mutex& mtx) noexcept
		: m_mutex(mtx) {
		mtx.lock_shared();
	}
	shared_lock(const scoped_lock&) = delete;
	shared_lock(scoped_lock&&) = delete;
	~shared_lock() noexcept {
		if (m_locked) {
			m_mutex.unlock_shared();
		}
	}

public:
	shared_lock& operator=(const shared_lock&) = delete;
	shared_lock& operator=(shared_lock&&) = delete;

public:
	_Acquires_nonreentrant_lock_(m_mutex) void lock() noexcept {
		assert(!m_locked);
		m_mutex.lock_shared();
		m_locked = true;
	}

	_Releases_nonreentrant_lock_(m_mutex) void unlock() noexcept {
		assert(m_locked);
		m_mutex.unlock_shared();
		m_locked = false;
	}

private:
	mutex& m_mutex;
	bool m_locked = true;

	friend class condition_variable;
};
#endif

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
	void wait(scoped_lock& lock);
	void wait(shared_lock& lock);

	template <typename R, typename P>
	[[nodiscard]] bool wait_for(scoped_lock& lock, const std::chrono::duration<R, P> duration) {
		return wait_for(lock, std::chrono::duration_cast<std::chrono::milliseconds>(duration));
	}

	template <typename R, typename P>
	[[nodiscard]] bool wait_for(shared_lock& lock, const std::chrono::duration<R, P> duration) {
		return wait_for(lock, std::chrono::duration_cast<std::chrono::milliseconds>(duration));
	}

	void notify_one() noexcept;
	void notify_all() noexcept;

private:
	[[nodiscard]] bool wait_for(scoped_lock& lock, DWORD milliseconds);
	[[nodiscard]] bool wait_for(shared_lock& lock, DWORD milliseconds);

private:
	CONDITION_VARIABLE m_conditionVariable = CONDITION_VARIABLE_INIT;
};

}  // namespace m3c
