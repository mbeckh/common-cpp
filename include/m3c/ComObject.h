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
/// @copyright The classes `#m3c::AbstractComObject` and `#m3c::ComObject` are based on ideas published in the
/// article https://msdn.microsoft.com/en-us/magazine/dn879357.aspx from Kenny Kerr (with the respective source code
/// published at https://github.com/kennykerr/modern/).
#pragma once

#include <windows.h>
#include <unknwn.h>

#include <concepts>
#include <cstddef>

namespace m3c {

namespace internal {
class AbstractComObject;
}  // namespace internal

// forward declare com_ptr and make_com to allow access to ReleaseNonDelegated by make_com

template <std::derived_from<IUnknown> T>
class com_ptr;

template <typename T, std::derived_from<internal::AbstractComObject> C, typename... Args>
com_ptr<T> make_com(Args&&... args);


namespace internal {

/// @brief Provides the `IUnknown` interface for COM classes to support aggregation.
/// @details Cannot be merged with `AbstractComObject` because casting requires a class without a vtable.
/// @see https://docs.microsoft.com/en-us/windows/win32/com/aggregation
/// @copyright Thanks to Raymond! https://devblogs.microsoft.com/oldnewthing/20211028-00/?p=105852
class Unknown {
protected:
	/// @brief Implements the `IUnknown` interface.
	/// @details Delegates all calls to the `AbstractComObject`.
	class UnknownImpl : public IUnknown {
	public:
		UnknownImpl() noexcept = default;
		UnknownImpl(const UnknownImpl&) = delete;
		UnknownImpl(UnknownImpl&&) = delete;
		virtual ~UnknownImpl() noexcept = default;

	public:
		UnknownImpl& operator=(const UnknownImpl&) = delete;
		UnknownImpl& operator=(UnknownImpl&&) = delete;

	public:  // IUnknown
		[[nodiscard]] HRESULT QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept final;
		ULONG AddRef() noexcept final;
		ULONG Release() noexcept final;

	private:
		/// @brief Return the enclosing `AbstractComObject`.
		/// @return The enclosing `AbstractComObject`.
		[[nodiscard]] AbstractComObject* GetAbstractComObject() noexcept;
	};

protected:
	Unknown() noexcept = default;

public:
	Unknown(const Unknown&) = delete;
	Unknown(Unknown&&) = delete;

protected:
	~Unknown() noexcept = default;  // MUST NOT be virtual

public:
	Unknown& operator=(const Unknown&) = delete;
	Unknown& operator=(Unknown&&) = delete;

protected:
	/// @brief Provides the `IUnknown` interface.
	UnknownImpl m_unknown;  // NOLINT(misc-non-private-member-variables-in-classes): Used by AbstractComObject only which inherits privately.
};


/// @brief The base class of all `ComObject` objects.
/// @note `AbstractComObject` does not inherit from `IUnknown` itself to avoid multiple inheritance happening "as a
/// surprise". `IUnknown` is implemented in `ComObject`.
class __declspec(novtable) AbstractComObject : private Unknown {  // NOLINT(cppcoreguidelines-virtual-class-destructor): COM uses reference counting.
protected:
	/// @brief Default constructor which also increments the global object count.
	[[nodiscard]] AbstractComObject() noexcept;

	/// @brief Constructor for aggregation.
	/// @param pOuter A pointer to the outer class in case of aggregation.
	[[nodiscard]] explicit AbstractComObject(_In_opt_ IUnknown* pOuter) noexcept;

public:
	AbstractComObject(const AbstractComObject&) = delete;
	AbstractComObject(AbstractComObject&&) = delete;

protected:
	/// @brief Destructor which also decrements the global object count.
	virtual ~AbstractComObject() noexcept;

public:
	AbstractComObject& operator=(const AbstractComObject&) = delete;
	AbstractComObject& operator=(AbstractComObject&&) = delete;

private:
	/// @brief Provides the implementation of `QueryInterface` for `IUnknown`.
	/// @param riid The requested interface id.
	/// @param ppObject A pointer for the result interface pointer.
	/// @return A `HRESULT` error code.
	[[nodiscard]] HRESULT QueryInterfaceNonDelegated(REFIID riid, _COM_Outptr_ void** ppObject) noexcept;

	/// @brief Provides the implementation of `AddRef` for `IUnknown`.
	/// @return The new reference count.
	ULONG AddRefNonDelegated() noexcept;

	/// @brief Provides the implementation of `Release` for `IUnknown`.
	/// @return The new reference count.
	ULONG ReleaseNonDelegated() noexcept;

public:
	/// @brief Query for an interface by IID.
	/// @param riid The IID of the requested interface.
	/// @return A pointer to the interface.
	/// @throws `com_exception` if the interface is not supported.
	[[nodiscard]] _Ret_notnull_ void* QueryInterface(REFIID riid);

	/// @brief Query for an interface.
	/// @tparam T The type of the interface.
	/// @return A pointer to the interface.
	/// @throws `com_exception` if the interface is not supported.
	template <std::derived_from<IUnknown> T>
	[[nodiscard]] _Ret_notnull_ T* QueryInterface() {
		return static_cast<T*>(QueryInterface(__uuidof(T)));
	}

private:
	/// @brief Lookup a pointer to a particular interface.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	[[nodiscard]] virtual _Ret_maybenull_ void* FindInterfaceInternal(REFIID riid) noexcept = 0;

protected:
	/// @brief Provides support for COM inheritance hierarchies.
	/// @details Classes which implement COM interfaces which in turn have base interfaces other than IUnknown MUST
	/// implement this method. The method MUST check for the base IIDs and return the correct pointer. If the interface
	/// is not supported, `nullptr` is REQUIRED. The method is called only after all base classes have been checked.
	/// @param riid The requested IID.
	/// @return A pointer to the interface if found, else `nullptr`.
	[[nodiscard]] virtual constexpr _Ret_maybenull_ void* FindInterface([[maybe_unused]] REFIID riid) noexcept {
		return nullptr;
	}

protected:
	/// @brief The pointer to the `IUnknown` interface which MAY be delegated in case of aggregation.
	IUnknown* m_pUnknown = &m_unknown;  // NOLINT(misc-non-private-member-variables-in-classes): Required by subclass and friend classes.

private:
	volatile ULONG m_refCount = 1;  ///< @brief The COM reference count of this object.

	friend class AbstractClassFactory;  // allow AbstractClassFactory to set m_pUnknown
	friend class Unknown::UnknownImpl;  // allow UnknownImpl to call ...NonDelegated methods

	template <typename T, std::derived_from<internal::AbstractComObject> C, typename... Args>
	friend com_ptr<T> m3c::make_com(Args&&... args);
};

}  // namespace internal


/// @brief A common base class for all COM objects.
/// @details The class provides default implementations for `IUnknown` and reference counting for `DllCanUnloadNow`.
/// @tparam Interfaces The interfaces implemented by this COM object.
template <std::derived_from<IUnknown>... Interfaces>
class __declspec(novtable) ComObject : public internal::AbstractComObject  // NOLINT(cppcoreguidelines-virtual-class-destructor): COM uses reference counting.
    , public Interfaces... {
protected:
	using AbstractComObject::AbstractComObject;

public:
	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject(const ComObject&) = delete;

	/// @brief COM objects use reference counting and therefore MUST NOT be moved.
	ComObject(ComObject&&) = delete;

protected:
	/// @brief Destructor which also decrements the global object count.
	~ComObject() noexcept override = default;

public:
	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject& operator=(const ComObject&) = delete;

	/// @brief COM objects use reference counting and therefore MUST NOT be moved.
	ComObject& operator=(ComObject&&) = delete;

public:  // IUnknown
	[[nodiscard]] HRESULT __stdcall QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept final {
		return m_pUnknown->QueryInterface(riid, ppObject);
	}
	ULONG __stdcall AddRef() noexcept final {
		return m_pUnknown->AddRef();
	}
	ULONG __stdcall Release() noexcept final {
		return m_pUnknown->Release();
	}

public:
	// make QueryInterface methods of AbstractComObject available to sub classes
	using AbstractComObject::QueryInterface;

private:
	[[nodiscard]] constexpr _Ret_maybenull_ void* FindInterfaceInternal(REFIID riid) noexcept final {
		return FindInterfaceInArgs<Interfaces...>(riid);
	}

	/// @brief A helper method for unpacking the template arguments.
	/// @tparam First The first interface.
	/// @tparam Remaining The remaining interfaces.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	template <typename First, typename... Remaining>
	[[nodiscard]] constexpr _Ret_maybenull_ void* FindInterfaceInArgs(REFIID riid) noexcept {
		if (IsEqualIID(riid, __uuidof(First))) {
			return static_cast<First*>(this);
		}
		return FindInterfaceInArgs<Remaining...>(riid);
	}

	/// @brief A helper method for the last iteration when unpacking the template arguments, internally calls `FindInterface`.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	template <int = 0>
	[[nodiscard]] constexpr _Ret_maybenull_ void* FindInterfaceInArgs(REFIID riid) noexcept {
		return FindInterface(riid);
	}
};

}  // namespace m3c
