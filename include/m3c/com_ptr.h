/*
Copyright 2019 Michael Beckh

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

#include <m3c/ComObject.h>
//#include <m3c/exception.h>
#include <m3c/finally.h>
//#include <m3c/types_log.h>  // IWYU pragma: keep

#include <guiddef.h>
#include <sal.h>
#include <unknwn.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <new>  // IWYU pragma: keep
#include <type_traits>
#include <utility>

struct IStream;

// namespace llamalog {
// class LogLine;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
// }  // namespace llamalog


namespace m3c {

namespace internal {

class __declspec(novtable) com_ptr_base {
protected:
	constexpr com_ptr_base() noexcept = default;

protected:
	static void QueryInterface(IUnknown* pUnknown, const IID& iid, void** p);
};

}  // namespace internal

/// @brief Simple smart pointer for COM objects to allow auto-release during stack unwinding.
/// @details Provided as a simple template class similar to `unique_ptr` to avoid `_com_ptr_t` which uses lots of macros.
/// @tparam T The COM interface type.
template <typename T>
class com_ptr final : private internal::com_ptr_base {
private:
	// prevent creation for non-COM classes.
	static_assert(std::is_base_of_v<IUnknown, T>);

	/// @brief A helper class to block access to the methods of `IUnknown`.
	class restricted_ptr : public T {
	private:
		using T::AddRef;
		using T::QueryInterface;
		using T::Release;
	};

public:
	/// @brief Creates an empty instance.
	constexpr com_ptr() noexcept = default;

	/// @brief Creates a new instance increasing the reference count.
	/// @param ptr Another `com_ptr`.
	com_ptr(const com_ptr& ptr) noexcept
		: m_ptr(ptr.get_owner()) {
		// empty
	}

	/// @brief Creates a new instance with a different COM interface obtained by calling `QueryInterface`.
	/// @details If the source contains no object, this object is also set to `nullptr`.
	/// @tparam S The COM interface type of the source.
	/// @param ptr Another instance of a different type.
	template <typename S>
	com_ptr(const com_ptr<S>& ptr)  // NOLINT(google-explicit-constructor): Allow implicit conversion from other com_ptr.
		: m_ptr(ptr.template get_owner<T>()) {
		// empty
	}

	/// @brief Transfers ownership, i.e. does not increase the reference count.
	/// @param ptr Another instance.
	com_ptr(com_ptr&& ptr) noexcept
		: m_ptr(ptr.release()) {
	}

	/// @brief Creates an empty instance.
	constexpr explicit com_ptr(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Assigns an interface pointer and acquires ownership, i.e. increases the reference count.
	/// @param p The native pointer.
	explicit com_ptr(_In_opt_ T* const p) noexcept
		: m_ptr(p) {
		com_add_ref();
	}

private:
	/// @brief Assigns an interface pointer without increasing the reference count.
	/// This constructor is required for `make_com`.
	/// @param p The native pointer.
	explicit com_ptr(_In_ internal::AbstractComObject* const p)
		: m_ptr(p->QueryInterface<T>()) {
		// empty
	}

public:
	/// @brief Decreases the reference count.
	~com_ptr() noexcept {
		if (m_ptr) [[likely]] {
			m_ptr->Release();
		}
	}

public:
	/// @brief Creates a copy of a smart pointer and increases the reference count.
	/// @param ptr Another instance.
	/// @return This instance.
	com_ptr& operator=(const com_ptr& ptr) noexcept {  // NOLINT(bugprone-unhandled-self-assignment, cert-oop54-cpp): reset handles self assignment properly.
		reset(ptr.m_ptr);
		return *this;
	}

	/// @brief Creates a new instance with a different COM interface obtained by calling `QueryInterface`.
	/// @details If the source contains no object, this object is also set to `nullptr`.
	/// @tparam S The COM interface of the source.
	/// @param ptr Another instance.
	/// @return This instance.
	template <typename S>
	com_ptr& operator=(const com_ptr<S>& ptr) {
		// first query then release for strong exception guarantee
		T* const p = ptr.template get_owner<T>();
		com_release();
		m_ptr = p;
		return *this;
	}

	/// @brief Transfers ownership, i.e. does not increase the reference count.
	/// @param ptr Another instance.
	/// @return This instance.
	com_ptr& operator=(com_ptr&& ptr) noexcept {
		com_release();
		m_ptr = ptr.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	com_ptr& operator=(std::nullptr_t) noexcept {
		com_release();
		return *this;
	}

	/// @brief Allows the smart pointer to act as the interface.
	/// @remarks There is no `const` overload because COM does not use `const` methods.
	/// @return The native pointer to the interface.
	[[nodiscard]] _Ret_maybenull_ restricted_ptr* operator->() const noexcept {
		return static_cast<restricted_ptr*>(m_ptr);
	}

	/// @brief An invalid * operator.
	/// @note I've never seen anyone using references of COM objects or COM objects on the stack. So the * operator does not compile.
	_Ret_maybenull_ restricted_ptr& operator*() const noexcept {
#ifndef __clang_analyzer__
		// clang can't handle this
		static_assert(false, "Are you sure you know what you're doing?");
#endif
	}

	/// @brief Get the address of the internal pointer, e.g. for COM object creation.
	/// @details The currently held object is released before returning the address.
	/// When a value is assigned to the return value of this function, ownership is managed by this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] _Ret_notnull_ T** operator&() noexcept {
		com_release();
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] explicit operator bool() const noexcept {
		return m_ptr;
	}

public:
	/// @brief Returns the pointer without releasing the ownership.
	/// @return The native pointer to the interface.
	[[nodiscard]] _Ret_maybenull_ T* get() const noexcept {
		return m_ptr;
	}

	/// @brief Returns a native pointer as a new reference-
	/// @details The method calls `AddRef` internally. If the instance contains no object, the function returns `nullptr`.
	/// @return The native pointer to the interface or `nullptr` if this `com_ptr` is empty.
	[[nodiscard]] _Ret_maybenull_ T* get_owner() const noexcept {
		com_add_ref();
		return m_ptr;
	}

	/// @brief Returns a native pointer as a new reference to a different interface using `QueryInterface`.
	/// @details The method calls `AddRef` internally. This is the generic version of the method.
	/// If the instance contains no object, the function returns `nullptr`.
	/// @tparam Q The type of the requested interface.
	/// @return The native pointer to the interface or `nullptr` if this `com_ptr` is empty.
	template <typename Q>
	[[nodiscard]] _Ret_maybenull_ Q* get_owner() const {
		if (m_ptr) [[likely]] {
			Q* p;  // NOLINT(cppcoreguidelines-init-variables): Out parameter for QueryInterface.
			QueryInterface(m_ptr, __uuidof(Q), (void**) &p);
			return p;
		}
		return nullptr;
	}

	/// @brief Acquire ownership of a native pointer.
	/// @param p A native pointer.
	void reset(_In_opt_ T* const p = nullptr) noexcept {
		// First add reference, then release (just to be sure...). No check for rare case of self assignment required.
		T* const pPrevious = m_ptr;
		m_ptr = p;
		com_add_ref();
		if (pPrevious) [[likely]] {
			pPrevious->Release();
		}
	}

	/// @brief Release ownership of a pointer.
	/// @note The responsibility for releasing the object is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param ptr The other `com_ptr`.
	void swap(com_ptr& ptr) noexcept {
		std::swap(m_ptr, ptr.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] std::size_t hash() const noexcept {
		return std::hash<T*>{}(m_ptr);
	}

private:
	/// @brief Increases the reference count of the object (if not `nullptr`).
	void com_add_ref() const noexcept {
		if (m_ptr) [[likely]] {
			m_ptr->AddRef();
		}
	}

	/// @brief Decreases the reference count of the object (if not `nullptr`).
	void com_release() noexcept {
		if (m_ptr) [[likely]] {
			m_ptr->Release();
			m_ptr = nullptr;
		}
	}


private:
	T* m_ptr = nullptr;  ///< @brief The native pointer wrapped by this class.

	template <typename T, typename C, typename... Args>
	friend typename std::enable_if_t<std::is_base_of_v<internal::AbstractComObject, C>, com_ptr<T>> make_com(Args&&... args);
};


//
// operator==
//

/// @brief Allows comparison of two `com_ptr` instances.
/// @tparam T The type of the first managed native pointer.
/// @tparam U The type of the second managed native pointer.
/// @param ptr A `com_ptr` object.
/// @param oth Another `com_ptr` object.
/// @return `true` if @p ptr points to the same COM object as @p oth.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const com_ptr<T>& ptr, const com_ptr<U>& oth) noexcept {
	return ptr.get() == oth.get();
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `com_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr holds the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const com_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator==(const U* const p, const com_ptr<T>& ptr) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds no object.
template <typename T>
[[nodiscard]] inline bool operator==(const com_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !ptr;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds no object.
template <typename T>
[[nodiscard]] inline bool operator==(std::nullptr_t, const com_ptr<T>& ptr) noexcept {
	return !ptr;
}

//
// operator!=
//

/// @brief Allows comparison of two `com_ptr` instances.
/// @tparam T The type of the first managed native pointer.
/// @tparam U The type of the second managed native pointer.
/// @param ptr A `com_ptr` object.
/// @param oth Another `com_ptr` object.
/// @return `true` if @p ptr does not point to the same COM object as @p oth.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const com_ptr<T>& ptr, const com_ptr<U>& oth) noexcept {
	return ptr.get() != oth.get();
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `com_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr does not hold the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const com_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr does not hold the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] inline bool operator!=(const U* const p, const com_ptr<T>& ptr) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds an object.
template <typename T>
[[nodiscard]] inline bool operator!=(const com_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !!ptr;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds an object.
template <typename T>
[[nodiscard]] inline bool operator!=(std::nullptr_t, const com_ptr<T>& ptr) noexcept {
	return !!ptr;
}

/// @brief Create a new `com_ptr` instance.
/// @details This class is for use with objects based on @ref `AbstractComObject` only.
/// @tparam T The type managed by the `com_ptr`. The interface MUST be implemented by @p C, else an exception is thrown.
/// @tparam C The type of the object to create.
/// @tparam Args The types of the arguments for the constructor of @p C.
/// @param args The arguments for the constructor of @p C.
/// @return A `com_ptr` holding a newly created COM object.
template <typename T, typename C, typename... Args>
[[nodiscard]] inline typename std::enable_if_t<std::is_base_of_v<internal::AbstractComObject, C>, com_ptr<T>> make_com(Args&&... args) {
	internal::AbstractComObject* const pObject = new C(std::forward<Args>(args)...);
	auto f = finally([pObject]() noexcept {
		pObject->Release();
	});
	// constructor calls QueryInterface internally so that the correct COM pointer is stored.
	// Please note: COM interface inheritance is somewhat different from usual OO inheritance.
	return com_ptr<T>(pObject);
}


/// @brief Swap function.
/// @tparam T The type of the managed native pointer.
/// @param ptr A `com_ptr` object.
/// @param oth Another `com_ptr` object.
template <typename T>
inline void swap(m3c::com_ptr<T>& ptr, m3c::com_ptr<T>& oth) noexcept {
	ptr.swap(oth);
}

}  // namespace m3c

/// @brief Specialization of std::hash.
template <typename T>
struct std::hash<m3c::com_ptr<T>> {
	[[nodiscard]] std::size_t operator()(const m3c::com_ptr<T>& ptr) const noexcept {
		return ptr.hash();
	}
};

#if 0
/// @brief Add the value of the smart pointer to a `llamalog::LogLine`.
/// @tparam T The type managed by the COM pointer.
/// @param logLine The output target.
/// @param arg A `com_ptr`.
template <typename T>
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, m3c::com_ptr<T>& arg) {
	m3c::com_ptr<IUnknown> ptr(arg);
	return operator<<(logLine, ptr);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, m3c::com_ptr<IUnknown>& arg);
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, m3c::com_ptr<IStream>& arg);

#endif
