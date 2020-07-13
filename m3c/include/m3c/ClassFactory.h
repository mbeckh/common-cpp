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

#include <unknwn.h>
#include <windows.h>

#include <new>  // IWYU pragma: keep

namespace m3c {

namespace internal {

/// @brief The base class of all `ClassFactory` objects.
/// @details The class also manages the number of locks acquired on all class factories.
class __declspec(novtable) AbstractClassFactory : public ComObject<IClassFactory> {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
protected:
	// Only allow creation from sub classes.
	AbstractClassFactory() noexcept = default;

public:
	AbstractClassFactory(const AbstractClassFactory&) = delete;
	AbstractClassFactory(AbstractClassFactory&&) = delete;

protected:
	virtual ~AbstractClassFactory() noexcept = default;

public:
	AbstractClassFactory& operator=(const AbstractClassFactory&) = delete;
	AbstractClassFactory& operator=(AbstractClassFactory&&) = delete;

public:  // IClassFactory
	[[nodiscard]] HRESULT __stdcall CreateInstance(_In_opt_ IUnknown* pOuter, REFIID riid, _COM_Outptr_ void** ppObject) noexcept final;
	[[nodiscard]] HRESULT __stdcall LockServer(BOOL lock) noexcept final;

private:
	/// @brief Create a new COM object.
	/// @return A newly created COM object.
	[[nodiscard]] virtual AbstractComObject* CreateObject() const = 0;  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
};

}  // namespace internal


/// @brief Class factory for COM objects.
template <class T>
class ClassFactory final : public internal::AbstractClassFactory {  // NOLINT(readability-identifier-naming): Windows/COM naming convention.
	// Only valid for classes derived from `internal::AbstractComObject`.
	static_assert(std::is_base_of_v<internal::AbstractComObject, T>);

public:
	ClassFactory() noexcept = default;
	ClassFactory(const ClassFactory&) = delete;
	ClassFactory(ClassFactory&&) = delete;

protected:  // COM objects all have protected virtual destructors.
	virtual ~ClassFactory() noexcept = default;

public:
	ClassFactory& operator=(const ClassFactory&) = delete;
	ClassFactory& operator=(ClassFactory&&) = delete;

private:
	[[nodiscard]] AbstractComObject* CreateObject() const final {
		return new T();
	}
};

}  // namespace m3c
