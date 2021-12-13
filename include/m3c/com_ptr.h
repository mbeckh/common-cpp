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

#include <m3c/ComObject.h>
#include <m3c/LogArgs.h>
#include <m3c/finally.h>

#include <windows.h>
#include <objidl.h>
#include <unknwn.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>  // IWYU pragma: keep
#include <utility>

namespace m3c {

namespace internal {

/// @brief Base class with common static helper methods.
class com_ptr_base {
protected:
	constexpr com_ptr_base() noexcept = default;
	constexpr com_ptr_base(const com_ptr_base&) noexcept = default;
	constexpr com_ptr_base(com_ptr_base&&) noexcept = default;
	constexpr ~com_ptr_base() noexcept = default;

protected:
	constexpr com_ptr_base& operator=(const com_ptr_base&) noexcept = default;
	constexpr com_ptr_base& operator=(com_ptr_base&&) noexcept = default;

protected:
	/// @brief Wraps calls to `IUnknown::QueryInterface` and throws an exception in case of errors.
	/// @param pUnknown The `IUnknown` pointer.
	/// @param iid The `IID` to query.
	/// @param p The native result COM pointer.
	static void QueryInterface(_In_ IUnknown* pUnknown, const IID& iid, _Outptr_ void** p);
};

}  // namespace internal


/// @brief Simple smart pointer for COM objects to allow auto-release during stack unwinding.
/// @details Provided as a simple template class similar to `unique_ptr` to avoid `_com_ptr_t` which uses lots of macros.
/// @tparam T The COM interface type.
template <std::derived_from<IUnknown> T>
class com_ptr final : private internal::com_ptr_base {
private:
	/// @brief A helper class to block access to the methods of `IUnknown`.
	class restricted_ptr : public T {  // NOLINT(cppcoreguidelines-virtual-class-destructor): COM uses reference counting, purpose of class is to hide destruction logic.
	private:
		using T::AddRef;
		using T::QueryInterface;
		using T::Release;
	};

public:
	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr com_ptr() noexcept = default;

	/// @brief Creates a new instance with a different COM interface obtained by calling `QueryInterface`.
	/// @details If the source contains no object, this object is also set to `nullptr`.
	/// @tparam S The COM interface type of the source.
	/// @param ptr Another instance of a different type.
	template <typename S>
	[[nodiscard]] constexpr com_ptr(const com_ptr<S>& ptr)  // NOLINT(google-explicit-constructor): Allow implicit conversion from other com_ptr.
	    : m_ptr(ptr.template get_owner<T>()) {
		// empty
	}

	/// @brief Creates an empty instance.
	[[nodiscard]] constexpr explicit com_ptr(std::nullptr_t) noexcept {
		// empty
	}

	/// @brief Assigns an interface pointer and acquires ownership, i.e. increases the reference count.
	/// @param p The native pointer.
	[[nodiscard]] constexpr explicit com_ptr(_In_opt_ T* const p) noexcept
	    : m_ptr(p) {
		com_add_ref();
	}

private:
	/// @brief Assigns an interface pointer without increasing the reference count.
	/// This constructor is required for `make_com`.
	/// @param p The native pointer.
	[[nodiscard]] explicit com_ptr(_In_ internal::AbstractComObject* const p)
	    : m_ptr(p->QueryInterface<T>()) {
		// empty
	}

public:
	/// @brief Creates a new instance increasing the reference count.
	/// @param ptr Another `com_ptr`.
	[[nodiscard]] constexpr com_ptr(const com_ptr& ptr) noexcept
	    : m_ptr(ptr.get_owner()) {
		// empty
	}

	/// @brief Transfers ownership, i.e. does not increase the reference count.
	/// @param ptr Another instance.
	[[nodiscard]] constexpr com_ptr(com_ptr&& ptr) noexcept
	    : m_ptr(ptr.release()) {
	}

	/// @brief Decreases the reference count.
	constexpr ~com_ptr() noexcept {
		if (m_ptr) {
			[[likely]];
			m_ptr->Release();
		}
	}

public:
	/// @brief Creates a copy of a smart pointer and increases the reference count.
	/// @param ptr Another instance.
	/// @return This instance.
	constexpr com_ptr& operator=(const com_ptr& ptr) noexcept {  // NOLINT(bugprone-unhandled-self-assignment): reset handles self assignment properly.
		reset(ptr.m_ptr);
		return *this;
	}

	/// @brief Creates a new instance with a different COM interface obtained by calling `QueryInterface`.
	/// @details If the source contains no object, this object is also set to `nullptr`.
	/// @tparam S The COM interface of the source.
	/// @param ptr Another instance.
	/// @return This instance.
	template <typename S>
	constexpr com_ptr& operator=(const com_ptr<S>& ptr) {
		// first query then release for strong exception guarantee
		T* const p = ptr.template get_owner<T>();
		com_release();
		m_ptr = p;
		return *this;
	}

	/// @brief Transfers ownership, i.e. does not increase the reference count.
	/// @param ptr Another instance.
	/// @return This instance.
	constexpr com_ptr& operator=(com_ptr&& ptr) noexcept {
		com_release();
		m_ptr = ptr.release();
		return *this;
	}

	/// @brief Resets the instance to hold no value.
	/// @return This instance.
	constexpr com_ptr& operator=(std::nullptr_t) noexcept {
		com_release();
		return *this;
	}

	/// @brief Allows the smart pointer to act as the interface.
	/// @remarks There is no `const` overload because COM does not use `const` methods.
	/// @return The native pointer to the interface.
	[[nodiscard]] constexpr _Ret_maybenull_ restricted_ptr* operator->() const noexcept {
		return reinterpret_cast<restricted_ptr*>(m_ptr);
	}

	/// @brief A deleted * operator.
	/// @note I've never seen anyone using references of COM objects or COM objects on the stack. So the * operator is deleted.
	const T& operator*() const = delete;

	/// @brief Get the address of the internal pointer, e.g. for COM object creation.
	/// @details The currently held object is released before returning the address.
	/// When a value is assigned to the return value of this function, ownership is managed by this instance.
	/// @return The address of the pointer which is managed internally.
	[[nodiscard]] constexpr _Ret_notnull_ T** operator&() noexcept {
		com_release();
		return std::addressof(m_ptr);
	}

	/// @brief Check if this instance currently manages a pointer.
	/// @return `true` if the pointer does not equal `nullptr`, else `false`.
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return !!m_ptr;
	}

public:
	/// @brief Returns the pointer without releasing the ownership.
	/// @return The native pointer to the interface.
	[[nodiscard]] constexpr _Ret_maybenull_ T* get() const noexcept {
		return m_ptr;
	}

	/// @brief Returns a native pointer as a new reference-
	/// @details The method calls `AddRef` internally. If the instance contains no object, the function returns `nullptr`.
	/// @return The native pointer to the interface or `nullptr` if this `com_ptr` is empty.
	[[nodiscard]] constexpr _Ret_maybenull_ T* get_owner() const noexcept {
		com_add_ref();
		return m_ptr;
	}

	/// @brief Returns a native pointer as a new reference to a different interface using `QueryInterface`.
	/// @details The method calls `AddRef` internally. This is the generic version of the method.
	/// If the instance contains no object, the function returns `nullptr`.
	/// @tparam Q The type of the requested interface.
	/// @return The native pointer to the interface or `nullptr` if this `com_ptr` is empty.
	template <std::derived_from<IUnknown> Q>
	[[nodiscard]] constexpr _Ret_maybenull_ Q* get_owner() const {
		if (m_ptr) {
			[[likely]];
			Q* p;  // NOLINT(cppcoreguidelines-init-variables): Out parameter for QueryInterface.
			QueryInterface(m_ptr, __uuidof(Q), reinterpret_cast<void**>(&p));
			return p;
		}
		return nullptr;
	}

	/// @brief Acquire ownership of a native pointer.
	/// @param p A native pointer.
	constexpr void reset(_In_opt_ T* const p = nullptr) noexcept {
		// First add reference, then release (just to be sure...). No check for rare case of self assignment required.
		T* const pPrevious = m_ptr;
		m_ptr = p;
		com_add_ref();
		if (pPrevious) {
			[[likely]];
			pPrevious->Release();
		}
	}

	/// @brief Release ownership of a pointer.
	/// @note The responsibility for releasing the object is transferred to the caller.
	/// @return The native pointer.
	[[nodiscard]] constexpr _Ret_maybenull_ T* release() noexcept {
		T* const p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	/// @brief Swap two objects.
	/// @param ptr The other `com_ptr`.
	constexpr void swap(com_ptr& ptr) noexcept {
		std::swap(m_ptr, ptr.m_ptr);
	}

	/// @brief Get a hash value for the object.
	/// @return A hash value calculated based on the managed pointer.
	[[nodiscard]] constexpr std::size_t hash() const noexcept {
		return std::hash<T*>{}(m_ptr);
	}

private:
	/// @brief Increases the reference count of the object (if not `nullptr`).
	constexpr void com_add_ref() const noexcept {
		if (m_ptr) {
			[[likely]];
			m_ptr->AddRef();
		}
	}

	/// @brief Decreases the reference count of the object (if not `nullptr`).
	constexpr void com_release() noexcept {
		if (m_ptr) {
			[[likely]];
			m_ptr->Release();
			m_ptr = nullptr;
		}
	}


private:
	T* m_ptr = nullptr;  ///< @brief The native pointer wrapped by this class.

	template <typename P, std::derived_from<internal::AbstractComObject> C, typename... Args>
	friend com_ptr<P> make_com(Args&&...);

	template <typename P>
	friend void operator>>(const com_ptr<P>&, _Inout_ LogFormatArgs&);

	template <typename P>
	friend void operator>>(const com_ptr<P>&, _Inout_ LogEventArgs&);
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
[[nodiscard]] constexpr bool operator==(const com_ptr<T>& ptr, const com_ptr<U>& oth) noexcept {
	return ptr.get() == oth.get();
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `com_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr holds the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const com_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const U* const p, const com_ptr<T>& ptr) noexcept {
	return ptr.get() == p;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds no object.
template <typename T>
[[nodiscard]] constexpr bool operator==(const com_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !ptr;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds no object.
template <typename T>
[[nodiscard]] constexpr bool operator==(std::nullptr_t, const com_ptr<T>& ptr) noexcept {
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
[[nodiscard]] constexpr bool operator!=(const com_ptr<T>& ptr, const com_ptr<U>& oth) noexcept {
	return ptr.get() != oth.get();
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param ptr A `com_ptr` object.
/// @param p A native pointer.
/// @return `true` if @p ptr does not hold the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] constexpr bool operator!=(const com_ptr<T>& ptr, const U* const p) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_ptr` with native pointers.
/// @tparam T The type of the managed native pointer.
/// @tparam U The type of the native pointer.
/// @param p A native pointer.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr does not hold the same COM object as @p p.
template <typename T, typename U>
[[nodiscard]] constexpr bool operator!=(const U* const p, const com_ptr<T>& ptr) noexcept {
	return ptr.get() != p;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds an object.
template <typename T>
[[nodiscard]] constexpr bool operator!=(const com_ptr<T>& ptr, std::nullptr_t) noexcept {
	return !!ptr;
}

/// @brief Allows comparison of `com_ptr` with `nullptr`.
/// @tparam T The type of the COM interface.
/// @param ptr A `com_ptr` object.
/// @return `true` if @p ptr holds an object.
template <typename T>
[[nodiscard]] constexpr bool operator!=(std::nullptr_t, const com_ptr<T>& ptr) noexcept {
	return !!ptr;
}

/// @brief Create a new `com_ptr` instance.
/// @details This class is for use with objects based on @ref `AbstractComObject` only.
/// @tparam T The type managed by the `com_ptr`. The interface MUST be implemented by @p C, else an exception is thrown.
/// @tparam C The type of the object to create.
/// @tparam Args The types of the arguments for the constructor of @p C.
/// @param args The arguments for the constructor of @p C.
/// @return A `com_ptr` holding a newly created COM object.
template <typename T, std::derived_from<internal::AbstractComObject> C, typename... Args>
[[nodiscard]] inline com_ptr<T> make_com(Args&&... args) {
	internal::AbstractComObject* const pObject = new C(std::forward<Args>(args)...);
	const auto release = finally([pObject]() noexcept {
		pObject->ReleaseNonDelegated();
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
constexpr void swap(com_ptr<T>& ptr, com_ptr<T>& oth) noexcept {
	ptr.swap(oth);
}

/// @brief Use the managed pointer as a log argument of type `fmt_ptr<IUnknown>`.
/// @tparam T The type of the managed object.
/// @param ptr A `com_ptr` object.
/// @param formatArgs The output target.
template <typename T>
inline void operator>>(const com_ptr<T>& ptr, _Inout_ LogFormatArgs& formatArgs) {
	formatArgs + fmt_ptr(static_cast<IUnknown* const&>(ptr.m_ptr));
}

/// @brief Use the address of the managed pointer as a log argument.
/// @tparam T The type of the managed object.
/// @param ptr A `com_ptr` object.
/// @param eventArgs The output target.
template <typename T>
inline void operator>>(const com_ptr<T>& ptr, _Inout_ LogEventArgs& eventArgs) {
	eventArgs << reinterpret_cast<const void* const&>(ptr.m_ptr);
}

/// @brief Adds logging for an `IStream` with two arguments: the managed pointer and the stream name.
/// @param ptr A `com_ptr` object.
/// @param formatArgs The output target.
template <>
void operator>>(const com_ptr<IStream>& ptr, _Inout_ LogFormatArgs& formatArgs);

/// @brief Adds logging for an `IStream` with two arguments: the managed pointer and the stream name.
/// @param ptr A `com_ptr` object.
/// @param eventArgs The output target.
template <>
void operator>>(const com_ptr<IStream>& ptr, _Inout_ LogEventArgs& eventArgs);

}  // namespace m3c


/// @brief Specialization of std::hash.
template <typename T>
struct std::hash<m3c::com_ptr<T>> {
	[[nodiscard]] constexpr std::size_t operator()(const m3c::com_ptr<T>& ptr) const noexcept {
		return ptr.hash();
	}
};

/// @brief Specialization of `fmt::formatter` for a `m3c::com_ptr`.
/// @details The formatting is forwarded to the formatter for `m3c::fmt_ptr` with type @p T.
/// @tparam T The type of the managed object.
/// @tparam CharT The character type of the formatter.
template <typename T, typename CharT>
struct fmt::formatter<m3c::com_ptr<T>, CharT> : fmt::formatter<m3c::fmt_ptr<T>, CharT> {
	/// @brief Format the `m3c::com_ptr`.
	/// @param arg A `m3c::com_ptr`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	template <typename FormatContext>
	[[nodiscard]] auto format(const m3c::com_ptr<T>& arg, FormatContext& ctx) const -> decltype(ctx.out()) {
		return __super::format(m3c::fmt_ptr(arg.get()), ctx);
	}
};
