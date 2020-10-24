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
/// @copyright The classes `#m3c::AbstractComObject` and `#m3c::ComObject` are based on ideas published in the
/// article https://msdn.microsoft.com/en-us/magazine/dn879357.aspx from Kenny Kerr (with the respective source code
/// published at https://github.com/kennykerr/modern/).

#pragma once

#include <unknwn.h>
#include <windows.h>

namespace m3c {

namespace internal {

/// @brief The base class of all `ComObject` objects.
class __declspec(novtable) AbstractComObject {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
protected:
	/// @brief Constructor which also increments the global object count.
	AbstractComObject() noexcept;

public:
	AbstractComObject(const AbstractComObject&) = delete;
	AbstractComObject(AbstractComObject&&) = delete;

protected:
	/// @brief Destructor which also decrements the global object count.
	virtual ~AbstractComObject() noexcept;

public:
	AbstractComObject& operator=(const AbstractComObject&) = delete;
	AbstractComObject& operator=(AbstractComObject&&) = delete;

public:  // IUnknown
	// AbstractComObject does not inherit from IUnknwon to avoid multiple inheritance happening as a "surprise".
	// Sub classes MUST therefore override the methods from IUnknwon and call these implementations.
	[[nodiscard]] HRESULT QueryInterface(REFIID riid, _COM_Outptr_ void** ppObject) noexcept;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	ULONG AddRef() noexcept;                                                                   // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	ULONG Release() noexcept;                                                                  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

public:
	/// @brief Query for an interface by IID.
	/// @param riid The IID of the requested interface.
	/// @return A pointer to the interface.
	/// @throws `com_exception` if the interface is not supported.
	[[nodiscard]] _Ret_notnull_ void* QueryInterface(REFIID riid);  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

	/// @brief Query for an interface.
	/// @tparam T The type of the interface.
	/// @return A pointer to the interface.
	/// @throws `com_exception` if the interface is not supported.
	template <typename T>
	[[nodiscard]] _Ret_notnull_ T* QueryInterface() {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		return static_cast<T*>(QueryInterface(__uuidof(T)));
	}

private:
	/// @brief Lookup a pointer to a particular interface.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	[[nodiscard]] virtual _Ret_maybenull_ void* FindInterface(REFIID riid) noexcept = 0;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.

protected:
	/// @brief Provides support for COM inheritance hierarchies.
	/// @details Classes which implement COM interfaces which in turn have base interfaces other than IUnknown MUST
	/// implement this method. The method MUST check for the base IIDs and return the correct pointer. If the interface
	/// is not supported, `nullptr` is REQUIRED. The method is called only after all base classes have been checked.
	/// @param riid The requested IID.
	/// @return A pointer to the interface if found, else `nullptr`.
	[[nodiscard]] virtual _Ret_maybenull_ void* FindBaseInterface([[maybe_unused]] REFIID riid) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		return nullptr;
	}

private:
	volatile ULONG m_refCount = 1;  ///< @brief The COM reference count of this object.
};

}  // namespace internal

/// @brief A common base class for all COM objects.
/// @details The class provides default implementations for `IUnknown` and reference counting for `DllCanUnloadNow`.
/// @tparam Interfaces The interfaces implemented by this COM object.
template <typename... Interfaces>
class __declspec(novtable) ComObject : public internal::AbstractComObject  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	, public Interfaces... {
protected:
	/// @brief Default constructor increments the global object count.
	ComObject() noexcept = default;

public:
	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject(const ComObject&) = delete;

	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject(ComObject&&) = delete;

protected:
	/// @brief Destructor which also decrements the global object count.
	virtual ~ComObject() noexcept = default;

public:
	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject& operator=(const ComObject&) = delete;

	/// @brief COM objects use reference counting and therefore MUST NOT be copied.
	ComObject& operator=(ComObject&&) = delete;

public:  // IUnknown
	using AbstractComObject::QueryInterface;
	[[nodiscard]] HRESULT __stdcall QueryInterface(REFIID riid, _COM_Outptr_ void** const ppObject) noexcept final {
		return AbstractComObject::QueryInterface(riid, ppObject);
	}
	ULONG __stdcall AddRef() noexcept final {
		return AbstractComObject::AddRef();
	}
	ULONG __stdcall Release() noexcept final {
		return AbstractComObject::Release();
	}

private:
	[[nodiscard]] _Ret_maybenull_ void* FindInterface(REFIID riid) noexcept final {
		return FindInterfaceInternal<Interfaces...>(riid);
	}

	/// @brief A helper method for unpacking the template arguments. This first method also handles the case of `IUnknown`.
	/// @tparam First The first interface.
	/// @tparam Remaining The remaining interfaces.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	template <typename First, typename... Remaining>
	[[nodiscard]] _Ret_maybenull_ void* FindInterfaceInternal(REFIID riid) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		if (IsEqualIID(riid, __uuidof(First)) || IsEqualIID(riid, IID_IUnknown)) {
			return static_cast<First*>(this);
		}
		return FindInterfaceRemaining<Remaining...>(riid);
	}

	/// @brief A helper method for the last iteration when unpacking the template arguments, internally calls `FindBaseInterface`.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	template <int = 0>
	[[nodiscard]] _Ret_maybenull_ void* FindInterfaceRemaining(REFIID riid) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		return FindBaseInterface(riid);
	}

	/// @brief A helper method for unpacking the template arguments.
	/// @tparam First The first interface.
	/// @tparam Remaining The remaining interfaces.
	/// @param riid The requested interface.
	/// @return A pointer to the interface or `nullptr` if the interface is not supported.
	template <typename First, typename... Remaining>
	[[nodiscard]] _Ret_maybenull_ void* FindInterfaceRemaining(REFIID riid) noexcept {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
		if (IsEqualIID(riid, __uuidof(First))) {
			return static_cast<First*>(this);
		}
		return FindInterfaceRemaining<Remaining...>(riid);
	}
};

}  // namespace m3c
