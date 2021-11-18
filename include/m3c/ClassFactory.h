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

#include <windows.h>
#include <unknwn.h>

#include <concepts>
#include <new>  // IWYU pragma: keep

namespace m3c {

namespace internal {

/// @brief The base class of all `ClassFactory` objects.
/// @details The class also manages the number of locks acquired on all class factories.
class __declspec(novtable) AbstractClassFactory : public ComObject<IClassFactory> {  // NOLINT(cppcoreguidelines-virtual-class-destructor): COM uses reference counting.
protected:
	// Only allow creation from sub classes.
	[[nodiscard]] AbstractClassFactory() noexcept = default;

public:
	AbstractClassFactory(const AbstractClassFactory&) = delete;
	AbstractClassFactory(AbstractClassFactory&&) = delete;

protected:
	~AbstractClassFactory() noexcept override = default;

public:
	AbstractClassFactory& operator=(const AbstractClassFactory&) = delete;
	AbstractClassFactory& operator=(AbstractClassFactory&&) = delete;

public:  // IClassFactory
	[[nodiscard]] HRESULT __stdcall CreateInstance(_In_opt_ IUnknown* pOuter, REFIID riid, _COM_Outptr_ void** ppObject) noexcept final;
	[[nodiscard]] HRESULT __stdcall LockServer(BOOL lock) noexcept final;

private:
	/// @brief Create a new COM object.
	/// @return A newly created COM object.
	[[nodiscard]] virtual _Ret_notnull_ AbstractComObject* CreateObject() const = 0;
};

}  // namespace internal


/// @brief Class factory for COM objects.
template <std::derived_from<internal::AbstractComObject> T>
class ClassFactory final : public internal::AbstractClassFactory {  // NOLINT(cppcoreguidelines-virtual-class-destructor): COM uses reference counting.
public:
	[[nodiscard]] ClassFactory() noexcept = default;

	ClassFactory(const ClassFactory&) = delete;
	ClassFactory(ClassFactory&&) = delete;

protected:  // COM objects all have protected virtual destructors.
	~ClassFactory() noexcept override = default;

public:
	ClassFactory& operator=(const ClassFactory&) = delete;
	ClassFactory& operator=(ClassFactory&&) = delete;

private:
	[[nodiscard]] _Ret_notnull_ AbstractComObject* CreateObject() const final {
		return new T();
	}
};

}  // namespace m3c
