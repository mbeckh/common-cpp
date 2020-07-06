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

#include <new>

namespace m3c {

/// @brief Base class for all class factories to manage the number of locks acquired on all class factories.
class __declspec(novtable) AbstractClassFactory : public ComObject<IClassFactory> {
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

public:
	/// @brief Get the number of locks currently held. @details The value is shared by all `ClassFactory` objects.
	/// @return The number of locks.
	[[nodiscard]] static ULONG GetLockCount() noexcept {
		return m_lockCount;
	}

public:  // IClassFactory
	[[nodiscard]] virtual HRESULT __stdcall CreateInstance(_In_opt_ IUnknown* pOuter, REFIID riid, _COM_Outptr_ void** ppObject) noexcept final;
	[[nodiscard]] virtual HRESULT __stdcall LockServer(BOOL lock) noexcept final;

private:
	/// @brief Create a new COM object.
	/// @return A newly created COM object.
	[[nodiscard]] virtual AbstractComObject* CreateObject() const = 0;

private:
	/// @brief The number of locks currently held.
	inline static volatile ULONG m_lockCount;
};


/// @brief Class factory for COM objects derived from @ref `AbstractComObject`.
template <class T>
class ClassFactory final : public AbstractClassFactory {
	// Only valid for classes derived from `AbstractComObject`.
	static_assert(std::is_base_of_v<AbstractComObject, T>);

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
	[[nodiscard]] virtual AbstractComObject* CreateObject() const final {
		return new T();
	}
};

}  // namespace m3c
